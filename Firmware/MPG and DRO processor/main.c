/*
 * main.c - MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 (alpha) / 2018-05-07
 */

/*

Copyright (c) 2018, Terje Io
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

· Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

· Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

· Neither the name of the copyright holder nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "lcd/lcd.h"
#include "fonts/font_23x16.h"
#include "fonts/freepixel_17x34.h"
#include "fonts/arial_48x55.h"

#include "serial.h"
#include "grbl.h"
#include "rpm.h"
#include "mpg.h"
#include "keypad.h"
#include "navigator.h"
#include "signals.h"

#define SERIAL_NO_DATA -1

#define RPMROW 222
#define STATUSROW 238
#define XROW 95
#define YROW 140
#define ZROW 185
#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2
#define POSCOL 50
#define POSFONT font_arial_48x55
#define BANNER "GRBL MPG"

// Event flags
#define EVENT_DRO     (1<<0)
#define EVENT_MPG     (1<<1)
#define EVENT_SIGNALS (1<<2)
#define EVENT_KEYDOWN (1<<3)
#define EVENT_KEYUP   (1<<4)

#define MIN(a, b) (((a) > (b)) ? (b) : (a))

typedef struct {
    float position;
    float offset;
    float mpg_base;
    int32_t mpg_position;
    bool dro_lock;
    uint16_t row;
} axis_data_t;

typedef struct {
    float fast_speed;
    float slow_speed;
    float step_speed;
    float fast_distance;
    float slow_distance;
    float step_distance;
} jog_config_t;

typedef struct {
    uint_fast16_t dro_refresh;
    uint_fast16_t mpg_refresh;
    uint_fast16_t signal_reset;
} event_counters_t;

volatile uint_fast8_t event = 0;
static event_counters_t event_interval = {200, 100, 20}, event_count;
static bool jogging = false, keyreleased = true, disableMPG = false;
bool WCOChanged = false, absDistance = true, modeMPG = false, awaitWCO = false;
float feedRate = 0.0f, spindleSpeed = 0.0f, angle = 0.0f, spindleRPM = 0.0f;
char status[8] = " ";
static axis_data_t axis[3];
static jogmode_t jogMode = JogMode_Fast;
static jog_config_t jog_config;

void p_debug(int32_t val) {
    static char buffer[12] = "";
sprintf(buffer, "%6d", val);
drawString(font_freepixel_17x34, 80, RPMROW, buffer, true);
}

char *ftoa (float value, char *format)
{
    static char buffer[15];

    sprintf(buffer, format, value);
    return buffer;
}

char *append (char *s)
{
    while(*s != '\0')
        s++;

    return s;
}

// BE WARNED: this function may be dangerous to use...
static char *strrepl (char *str, int c, char *str3)
{
    static char tmp[30];

    char *s = strrchr(str, c);

    while(s) {
        strcpy(tmp, str3);
        strcat(tmp, s + 1);
        strcpy(s, tmp);
        s = strrchr(str, c);
    }

    return str;
}

char *DRO_ParseDecimal (float *value, char *data, char *format)
{
    float val;

    val = (float)atof(data);
    if(val != *value) {
        *value = val;
        return ftoa(val, format);
    }
    return NULL;
}

void DRO_ShowRPM (char *rpm)
{
    if((rpm = DRO_ParseDecimal(&spindleRPM, rpm, "% 4.1f")) != NULL) {
        if(spindleRPM > 2000.0f) // TODO: add config for RPM warning
            setColor(Coral);
        drawString(font_freepixel_17x34, 80, RPMROW, rpm, true);
        setColor(White);
    }
}

void DRO_DisplayPosition (uint_fast8_t i, char *data)
{
    float pos = (float)atof(data);

    if(pos != axis[i].position || WCOChanged) {
        axis[i].position = pos;
        drawString(POSFONT, POSCOL, axis[i].row, ftoa(axis[i].position - axis[i].offset, "% 9.3f"), true);
    }
}

void DRO_ParsePositions (char *data, bool idle)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';
    DRO_DisplayPosition(X_AXIS, data);

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';
    DRO_DisplayPosition(Y_AXIS, data);

    data = next;
    DRO_DisplayPosition(Z_AXIS, data);

    WCOChanged = false;
}

void DRO_ParseWCO (char *data)
{
    char *next;
    float val;

    next = strchr(data, ',');
    *next++ = '\0';
    val = (float)atof(data);
    WCOChanged = axis[X_AXIS].offset != val;
    axis[X_AXIS].offset = val;

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';
    val = (float)atof(data);
    WCOChanged = WCOChanged || axis[Y_AXIS].offset != val;
    axis[Y_AXIS].offset = val;

    data = next;
    val = (float)atof(data);
    WCOChanged = WCOChanged || axis[Z_AXIS].offset != val;
    axis[Z_AXIS].offset = val;

    if(awaitWCO) {
        axis[X_AXIS].mpg_base = axis[X_AXIS].position;
        axis[Z_AXIS].mpg_base = axis[Z_AXIS].position;
        awaitWCO = false;
    }
}

void DRO_Feed (char *data)
{
    char *s;

    if((s = DRO_ParseDecimal(&feedRate, data, "%6.1f")))
        drawString(font_23x16, 40, STATUSROW, s, true);
}

void DRO_Speed (char *data)
{
    char *s;

    if((s = DRO_ParseDecimal(&spindleSpeed, data, "%6.1f")))
        drawString(font_23x16, 40, STATUSROW, s, true);
}

void resetPosition (bool await)
{
    MPG_Reset();
    axis[X_AXIS].mpg_position = 0;
    axis[Z_AXIS].mpg_position = 0;
    awaitWCO = await;
}

bool MPG_Move (void)
{
    mpg_t *pos;
    uint32_t velocity = 0;
    float delta_x = 0.0f, delta_y = 0.0f, delta_z = 0.0f;
    bool updated = false;
    static char buffer[50];

    if(awaitWCO)
        return false;

    strcpy(buffer, "G1");

    pos = MPG_GetPosition();

    if(!axis[Z_AXIS].dro_lock) {
        if(pos->z.position != axis[Z_AXIS].mpg_position) {
            delta_z = (float)(pos->z.position - axis[Z_AXIS].mpg_position) / 400.f;
            axis[Z_AXIS].mpg_position = pos->z.position;
            if(absDistance)
                axis[Z_AXIS].mpg_base += delta_z;
            velocity = pos->z.velocity;
            sprintf(append(buffer), "Z%.3f", absDistance ? axis[Z_AXIS].mpg_base - axis[Z_AXIS].offset : delta_z);
        }
    }

    if(!axis[X_AXIS].dro_lock) {
        if(pos->x.position != axis[X_AXIS].mpg_position) {
            delta_x = (float)(pos->x.position - axis[X_AXIS].mpg_position) / 400.f;
            axis[X_AXIS].mpg_position = pos->x.position;
            velocity = velocity == 0 ? pos->x.velocity : MIN(pos->x.velocity, velocity);
        } else if(angle != 0.0f)
            delta_x = delta_z * angle;

        if(delta_x != 0.0f) {
            if(absDistance)
                axis[X_AXIS].mpg_base += delta_x;
            sprintf(append(buffer), "X%.3f", absDistance ? axis[X_AXIS].mpg_base - axis[X_AXIS].offset : delta_x);
        }
    }

    if(!axis[Y_AXIS].dro_lock) {
        if(pos->y.position != axis[Y_AXIS].mpg_position) {
            delta_y = (float)(pos->y.position - axis[Y_AXIS].mpg_position) / 400.f;
            axis[Y_AXIS].mpg_position = pos->y.position;
            velocity = velocity == 0 ? pos->y.velocity : MIN(pos->y.velocity, velocity);
        }

        if(delta_y != 0.0f) {
            if(absDistance)
                axis[Y_AXIS].mpg_base += delta_y;
            sprintf(append(buffer), "Y%.3f", absDistance ? axis[Y_AXIS].mpg_base - axis[Y_AXIS].offset : delta_y);
        }
    }

    if((updated = delta_x != 0.0f || delta_y != 0.0f || delta_z != 0.0f)) {
//        WCOChanged = true;
        sprintf(append(buffer), "F%d", velocity * 50);
        serialWriteLn(buffer);
        drawString(font_23x16, 5, 40, buffer, true);

        setColor(Coral);

        if(delta_x != 0.0f) {
            sprintf(buffer, "% 9.3f", axis[X_AXIS].mpg_base - axis[X_AXIS].offset);
            drawString(POSFONT, POSCOL, axis[X_AXIS].row, buffer, true);
        }

        if(delta_y != 0.0f) {
            sprintf(buffer, "% 9.3f", axis[Y_AXIS].mpg_base - axis[Y_AXIS].offset);
            drawString(POSFONT, POSCOL, axis[Y_AXIS].row, buffer, true);
        }

        if(delta_z != 0.0f) {
            sprintf(buffer, "% 9.3f", axis[Z_AXIS].mpg_base - axis[Z_AXIS].offset);
            drawString(POSFONT, POSCOL, axis[Z_AXIS].row, buffer, true);
        }

        setColor(White);
    }

    return updated;
}

void DRO_FeedSpeed (char *data)
{
    char *next;
    float val;

    next = strchr(data, ',');
    *next++ = '\0';
    val = (float)atof(data);
    if(val != feedRate) {
        feedRate = val;
        drawString(font_23x16, 40, STATUSROW, ftoa(feedRate, "% 6.1f"), true);
    }
    data = next;
    val = (float)atof(data);
    if(val != spindleSpeed) {
        spindleSpeed = val;
        drawString(font_23x16, 140, STATUSROW, ftoa(spindleSpeed, "% 6.1f"), true);
    }
}

void driver_settings_restore (uint8_t restore_flag)
{
    if(restore_flag) {
        jog_config.step_speed    = 100.0f;
        jog_config.slow_speed    = 600.0f;
        jog_config.fast_speed    = 3000.0f;
        jog_config.step_distance = 0.25f;
        jog_config.slow_distance = 500.0f;
        jog_config.fast_distance = 3000.0f;

//        keypad_write_settings();
    }
}

void processKeypress (void)
{
    static char command[30];

    bool addedGcode, jogCommand = false;
    char keycode;

    if((keycode = keypad_get_keycode()))
      switch(keycode) {

        case '\r':
            signalMPGMode(!modeMPG);
            break;

        case 'U':
            disableMPG = true;
            break;

        case 'A':                                   // Mist override
            serialPutC(CMD_COOLANT_MIST_OVR_TOGGLE);
            break;

        case 'C':                                   // Coolant override
            serialPutC(CMD_COOLANT_FLOOD_OVR_TOGGLE);
            break;

        case 'e':                                   // Lock X-axis DRO for manual MPG sync
            if(modeMPG) {
                if(!(axis[X_AXIS].dro_lock = !axis[X_AXIS].dro_lock))
                    axis[X_AXIS].mpg_position = MPG_GetPosition()->x.position;
                setColor(axis[X_AXIS].dro_lock ? Yellow : White);
                sprintf(command, "% 9.3f", axis[X_AXIS].mpg_base - axis[X_AXIS].offset);
                drawString(POSFONT, POSCOL, axis[X_AXIS].row, command, true);
                setColor(White);
            }
            break;

        case 'E':
            if(modeMPG)
                serialWriteLn("G90G10L20P0X0");     // Zero X-axis
            break;

        case '0':                                   //Feed hold
            if(modeMPG)
                serialPutC(CMD_FEED_HOLD);
            else {
                event_count.signal_reset = event_interval.signal_reset;
                signalFeedHold(true);
            }
            break;

        case '.':                                   // Cycle start
            if(modeMPG)
                serialPutC(CMD_CYCLE_START);
            else {
                event_count.signal_reset = event_interval.signal_reset;
                signalCycleStart(true);
            }
            break;

        case 'h':                                   // "toggle" jog mode
            jogMode = jogMode == JogMode_Step ? JogMode_Fast : (jogMode == JogMode_Fast ? JogMode_Slow : JogMode_Step);
            break;

        case 'H':                                   // Home axes
            strcpy(command, "$H");
            break;

        case 'R':                                   // Jog X-axis right
            strcpy(command, "$J=G91X?F");
            break;

        case 'L':                                   // Jog X-axis left
            strcpy(command, "$J=G91X-?F");
            break;

        case 'F':                                   // Jog Y-axis forward
            strcpy(command, "$J=G91Y?F");
            break;

        case 'B':                                   // Jog Y-axis back
            strcpy(command, "$J=G91Y-?F");
            break;

        case 'q':                                   // Jog XY-axes SE
            strcpy(command, "$J=G91X?Y-?F");
            break;

        case 'r':                                   // Jog XY-axes NE
            strcpy(command, "$J=G91X?Y?F");
            break;

        case 's':                                   // Jog XY-axes NW
            strcpy(command, "$J=G91X-?Y?F");
            break;

        case 't':                                   // Jog XY-axes SW
            strcpy(command, "$J=G91X-?Y-?F");
            break;

        case 'u':                                   // Jog Z-axis up
            strcpy(command, "$J=G91Z?F");
            break;

        case 'D':                                   // Jog Z-axis down
            strcpy(command, "$J=G91Z-?F");
            break;
    }

    if(command[0] != '\0') {

        // add distance and speed to jog commands
        if((jogCommand = (command[0] == '$' && command[1] == 'J')))
            switch(jogMode) {

            case JogMode_Slow:
                strrepl(command, '?', ftoa(jog_config.slow_distance, "%5.0f"));
                strcat(command, ftoa(jog_config.slow_speed, "%5.0f"));
                break;

            case JogMode_Step:
                strrepl(command, '?', ftoa(jog_config.step_distance, "%5.3f"));
                strcat(command, ftoa(jog_config.step_speed, "%5.0f"));
                break;

            default:
                strrepl(command, '?', ftoa(jog_config.fast_distance, "%5.0f"));
                strcat(command, ftoa(jog_config.fast_speed, "%5.0f"));
                break;

        }

        if(!(jogCommand && keyreleased)) { // key still pressed? - do not execute jog command if released!
            addedGcode = true;
            serialWriteLn((char *)command);
            jogging = jogging || (jogCommand && addedGcode);
        }
    }
}


void timeoutHandler (void)
{
    if(modeMPG) {
        if(!(--event_count.dro_refresh)) {
            event_count.dro_refresh = event_interval.dro_refresh;
            event |= EVENT_DRO;
        }

        if(!(--event_count.mpg_refresh)) {
            event_count.mpg_refresh = event_interval.mpg_refresh;
            event |= EVENT_MPG;
        }

        if(event_count.signal_reset && !(--event_count.signal_reset))
            event |= EVENT_SIGNALS;
    }
}

void keyEvent (bool keyDown)
{
    if(keyDown)
        event |= EVENT_KEYDOWN;
    else
        event |= EVENT_KEYUP;

    keyreleased = !keyDown;
}

void main(void)
{
    int_fast16_t c;
    uint_fast16_t char_counter = 0;
    char line[256], *data;
    bool isStatus, isIdle, mpg_move = false;

    signalsInit();
    serialInit();

//    RPM_Init();
    MPG_Init();
    keypad_setup();

    navigator_setup();

    initGraphics();
    setOrientation(Orientation_Horizontal);

    driver_settings_restore(1);

//    TOUCH_Calibrate();

    clearScreen(true);

    lcd_display_t *screen = getDisplayDescriptor();

    navigator_configure(0, 0, screen->Width);

    signalMPGMode(modeMPG);

    memset(&axis, 0, sizeof(axis));

    axis[X_AXIS].row = XROW;
    axis[Y_AXIS].row = YROW;
    axis[Z_AXIS].row = ZROW;

    drawString(font_23x16, 5, 20, BANNER, false);
    drawString(POSFONT, POSCOL - getStringWidth(POSFONT, "X:") - 5, axis[X_AXIS].row, "X:", false);
    drawString(POSFONT, POSCOL - getStringWidth(POSFONT, "Y:") - 5, axis[Y_AXIS].row, "Y:", false);
    drawString(POSFONT, POSCOL - getStringWidth(POSFONT, "Z:") - 5, axis[Z_AXIS].row, "Z:", false);
    drawString(font_freepixel_17x34, 5, RPMROW, "RPM:", false);

    setSysTickCallback(timeoutHandler);
    setKeyclickCallback(keyEvent);

    // reset event counters
    event_count.dro_refresh = event_interval.dro_refresh;
    event_count.mpg_refresh = event_interval.mpg_refresh;
    event_count.signal_reset = 0;

    while(true) {

//        p_debug(navigator_get_pos());

        if(event) {

            if(event & EVENT_KEYUP) {
                event &= ~EVENT_KEYUP;
                if(jogging) {
                    keypad_flush();
                    jogging = false;
                    serialPutC(CMD_JOG_CANCEL);
                }
                if(disableMPG) {
                    disableMPG = false;
                    resetPosition(false);
                }
            }

            if(event & EVENT_KEYDOWN) {
                event &= ~EVENT_KEYDOWN;
                processKeypress();
            }

            if(event & EVENT_MPG) {
                event &= ~EVENT_MPG;
                mpg_move = MPG_Move();
            }

            if(event & EVENT_DRO) {
                event &= ~EVENT_DRO;
                if(!mpg_move)
                    serialPutC('?'); // Request realtime status from grbl
            }

            if(event & EVENT_SIGNALS) {
                event &= ~EVENT_SIGNALS;
                signalCycleStart(false);
                signalFeedHold(false);
            }

        }

        if((c = serialGetC()) != SERIAL_NO_DATA) {

            if((c == '\n') || (c == '\r')) { // End of line reached

                line[char_counter - 1] = '\0';

                if(char_counter > 1 && line[0] == '<') {

                    isStatus = true;
                    data = strtok(&line[1], "|");

                    while(data != NULL){

                        if(isStatus) {
                            if(strcmp(status, data)) {
                                isIdle = !strncmp(data, "Idle", 4);
                                strcpy(status, data);
                                c = strlen(status);
                                while(sizeof(status) - c > 1) {
                                    strcat(status, " ");
                                    c++;
                                }
                                drawString(font_23x16, 5, STATUSROW, status, true);
                            }
                            isStatus = false;
                        } else if(!strncmp(data, "MPos:", 5))
                            DRO_ParsePositions(data + 5, isIdle);

                        else if(!strncmp(data, "FS:", 3))
                            DRO_ShowRPM(strrchr(data + 3, ',') + 1);

                        else if(!strncmp(data, "WCO:", 4))
                            DRO_ParseWCO(data + 4);
/*
                        else if(!strncmp(data, "FS:", 3))
                            DRO_FeedSpeed(data + 3);
*/
                        else if(!strncmp(data, "Pn:", 3))
                            drawString(font_23x16, 220, STATUSROW, data, true);

                        else if(!strncmp(data, "MPG:", 4)) {
                            modeMPG = data[4] == '1';
                            if(modeMPG) {
                                serialWriteLn("$G");
                                resetPosition(true);
                            }
                            setColor(modeMPG ? Blue : White);
                            drawString(font_23x16, 5, 20, BANNER, true);
                            setColor(White);
                        }

                        data = strtok(NULL, "|");

                    }

                } else if(!strncmp(line, "[GC:", 4)) {

                    data = strtok(&line[4], " ");

                    while(data != NULL){

                        if(!strncmp(data, "F", 1))
                            DRO_Feed(data + 1);

                        if(!strncmp(data, "S", 1))
                            DRO_Speed(data + 1);

                        if(!strncmp(data, "G90", 3))
                            absDistance = true;

                        if(!strncmp(data, "G91", 3))
                            absDistance = false;

                        data = strtok(NULL, " ");
                    }
                } else if(!strncmp(line, "[MSG:", 5))
                    drawString(font_23x16, 5, STATUSROW, &line[5], true);

                char_counter = 0;

            } else if(char_counter <= 256)
                line[char_counter++] = c; // Upcase lowercase??
            else
                char_counter = 0;
        }
    }
}
