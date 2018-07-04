/*
 * dro.c - DRO canvas, main screen
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v1.0.5 / 2018-07-01 / ©Io Engineering / Terje
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
//#include "fonts/font_23x16.h"
#include "fonts/freepixel_17x34.h"
#include "fonts/arial_48x55.h"

#include "UILib/uilib.h"
#include "canvas/menu.h"

#include "grblcomm.h"
#include "rpm.h"
#include "mpg.h"
#include "keypad.h"
#include "navigator.h"
#include "signals.h"

#define RPMROW 200
#define STATUSROW 218
#define MSGROW 238
#define XROW 95
#define YROW 140
#define ZROW 185
#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2
#define POSCOL 50
#define POSFONT font_arial_48x55

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
    float mpg_factor;
    uint_fast8_t mpg_idx;
    int32_t mpg_position;
    bool dro_lock;
    bool visible;
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

typedef struct {
    bool on;
    bool ccw;
    uint_fast16_t rpm;
    uint_fast16_t mpg_rpm;
} spindle_state_t;

//

const char *banner = "EMCO Compact 5 Lathe";

// This array must match the grbl_state_t enum above!
const RGBColor_t grblStateColor[NUMSTATES] = {
    Black,
    White,
    LightGreen,
    Yellow,
    Coral,
    Coral,
    Red,
    Yellow,
    Coral,
    Coral,
    Coral,
    Coral,
    Yellow
};

const char *const jogModeStr[] = { "Fast ", "Slow ", "Step "};
const float mpgFactors[2] = {1.0f, 10.0f};

//

volatile uint_fast8_t event = 0;
static bool mpgMove = false, endMove = false;
static bool msg = false, jogging = false, keyreleased = true, disableMPG = false, mpgReset = false;
static bool WCOChanged = false, absDistance = true, modeMPG = false, awaitWCO = false, useWPos = false, active = false;
static float feedRate = 0.0f, angle = 0.0f, spindleRPM = 0.0f;
static axis_data_t axis[3];
static jogmode_t jogMode = JogMode_Slow;
static jog_config_t jog_config;
static event_counters_t event_count;
static Canvas *canvasMain = 0;
static TextBox *txtResponseL = NULL, *txtResponseR = NULL, *txtGrblState = NULL;

static lcd_display_t *screen;

static event_counters_t event_interval = {
    .dro_refresh  = 20,
    .mpg_refresh  = 10,
    .signal_reset = 20
};

static leds_t leds = {
    .value = 0
};

static spindle_state_t spindle_state = {
    .on =      false,
    .ccw =     false,
    .rpm =     0,
    .mpg_rpm = 200
};

static grbl_t grbl = {
    .state = Unknown,
    .state_text = " "
};

void p_debug(int32_t val) {
    static char buffer[12] = "";
    sprintf(buffer, "%6d", val);
    drawString(font_freepixel_17x34, 80, RPMROW, buffer, true);
}

char *ftoa (float value, char *format)
{
    static char buffer[25];

    sprintf(buffer, format, value);
    return buffer;
}

char *append (char *s)
{
    while(*s)
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

static void MPG_ResetPosition (bool await)
{
    MPG_Reset();
    axis[X_AXIS].mpg_position = 0;
    axis[Y_AXIS].mpg_position = 0;
    axis[Z_AXIS].mpg_position = 0;
    if(!(awaitWCO = await)) {
        axis[X_AXIS].mpg_base = axis[X_AXIS].position;
        axis[Y_AXIS].mpg_base = axis[Y_AXIS].position;
        axis[Z_AXIS].mpg_base = axis[Z_AXIS].position;
    }
}

static bool MPG_Move (void)
{
    mpg_t *pos;
    uint32_t velocity = 0;
    float delta_x = 0.0f, delta_y = 0.0f, delta_z = 0.0f;
    bool updated = false;
    static char buffer[50];

    if(awaitWCO || jogging || !(grbl.state == Idle || grbl.state == Run))
        return false;

    strcpy(buffer, "G1");

    pos = MPG_GetPosition();

    if(!axis[Z_AXIS].dro_lock) {
        if(pos->z.position != axis[Z_AXIS].mpg_position) {
            delta_z = (float)(pos->z.position - axis[Z_AXIS].mpg_position) * axis[Z_AXIS].mpg_factor / 400.0f;
            axis[Z_AXIS].mpg_position = pos->z.position;
            if(absDistance)
                axis[Z_AXIS].mpg_base += delta_z;
            velocity = pos->z.velocity;
            sprintf(append(buffer), "Z%.3f", absDistance ? axis[Z_AXIS].mpg_base - axis[Z_AXIS].offset : delta_z);
        }
    }

    if(!axis[X_AXIS].dro_lock) {
        if(pos->x.position != axis[X_AXIS].mpg_position) {
            delta_x = (float)(pos->x.position - axis[X_AXIS].mpg_position) * axis[X_AXIS].mpg_factor / 400.0f;
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
            delta_y = (float)(pos->y.position - axis[Y_AXIS].mpg_position) * axis[Y_AXIS].mpg_factor / 400.0f;
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

        sprintf(append(buffer), "F%d", velocity * 50);
        serialWriteLn(buffer);
//        drawString(font_23x16, 5, 40, buffer, true);

        setColor(Coral);

        if(delta_x != 0.0f)
            drawString(POSFONT, POSCOL, axis[X_AXIS].row, ftoa(axis[X_AXIS].mpg_base - axis[X_AXIS].offset, "% 9.3f"), true);

        if(delta_y != 0.0f)
            drawString(POSFONT, POSCOL, axis[Y_AXIS].row, ftoa(axis[Y_AXIS].mpg_base - axis[Y_AXIS].offset, "% 9.3f"), true);

        if(delta_z != 0.0f)
            drawString(POSFONT, POSCOL, axis[Z_AXIS].row, ftoa(axis[Z_AXIS].mpg_base - axis[Z_AXIS].offset, "% 9.3f"), true);

        setColor(White);

        if(!leds.run) {
            leds.run = true;
            keypad_leds(leds);
        }
    }

    return updated;
}

static char *parseDecimal (float *value, char *data, char *format)
{
    float val;

    val = (float)atof(data);
    if(val != *value) {
        *value = val;
        return ftoa(val, format);
    }

    return NULL;
}

static void displayPosition (uint_fast8_t i, float pos, bool force_update)
{
    if(pos != axis[i].position || (force_update && axis[i].position != HUGE_VALF)) {
        axis[i].position = pos;
        if(axis[i].visible) {
            setColor(axis[i].dro_lock ? Yellow : White);
            drawString(POSFONT, POSCOL, axis[i].row, ftoa(axis[i].position - axis[i].offset, "% 9.3f"), true);
            setColor(White);
        }
    }
}

static void parsePositions (char *data, bool force_update)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';
    displayPosition(X_AXIS, (float)atof(data), force_update);

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';
    displayPosition(Y_AXIS, (float)atof(data), force_update);

    data = next;
    displayPosition(Z_AXIS, (float)atof(data), force_update);

    WCOChanged = false;
}

static void parseWCO (char *data)
{
    if(useWPos) {
        axis[X_AXIS].offset = 0.0f;
        axis[Y_AXIS].offset = 0.0f;
        axis[Z_AXIS].offset = 0.0f;
    } else {
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
    }

    if(awaitWCO) {
        awaitWCO = false;
        MPG_ResetPosition(false);
    }

}

static void displayFeedRate (char *data)
{
    char *s;

    if((s = parseDecimal(&feedRate, data, "%6.1f"))) {
        drawString(font_23x16, 145, STATUSROW, s, true);
    }
}

static void displayRPM (char *data)
{
    char *s;

    if((s = parseDecimal(&spindleRPM, data, "%6.1f"))) {
        if(spindleRPM > 2000.0f) // TODO: add config for RPM warning
            setColor(Coral);
        drawString(font_freepixel_17x34, 60, RPMROW, s, true);
        setColor(White);
    }
}

static void displayFeedRateSpeed (char *data)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';
    displayFeedRate(data);

    data = next;
    displayRPM(strrchr(data, ',') + 1);
}

static void setMPGFactorBG (uint_fast8_t i, RGBColor_t color)
{
    setColor(color);
    fillRect(268, axis[i].row - 33, screen->Width - 1, axis[i].row - 10);
    setColor(White);
}

static void displayMPGFactor (uint_fast8_t i, uint_fast8_t mpg_idx)
{
    char buf[5];

    if(mpg_idx > (sizeof(mpgFactors) / sizeof(float)) - 1)
        axis[i].mpg_idx = 0;

    axis[i].mpg_factor = mpgFactors[axis[i].mpg_idx];

    if(axis[i].visible) {
        setMPGFactorBG(i, axis[i].mpg_factor == 1.0f ? Black : Red);
        sprintf(buf, "x%d", (uint32_t)axis[i].mpg_factor);
        drawString(font_23x16, 269, axis[i].row - 12, buf, false);
    }
}

static void displayGRBLState (void)
{
    txtGrblState->widget.fgColor = grblStateColor[grbl.state];
    UILibTextBoxDisplay(txtGrblState, grbl.state_text);
    leds.run = grbl.state == Run || grbl.state == Jog;
    leds.hold = grbl.state == Hold0 || grbl.state == Hold1;
    keypad_leds(leds);
}

static void setJogMode (jogmode_t mode)
{
    jogMode = mode;
    setColor(jogMode == JogMode_Fast ? Red : White);
    drawString(font_23x16, 265, RPMROW - 3, jogModeStr[jogMode], true);
    setColor(White);
}

static void driver_settings_restore (uint8_t restore_flag)
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

static void processKeypress (void)
{
    static char command[30];

    bool addedGcode, jogCommand = false;
    char keycode;

    command[0] = '\0';

    if((keycode = keypad_get_keycode()))
      switch(keycode) {

        case '\r':
            if(!modeMPG || grbl.state != Hold0)
                signalMPGMode(!modeMPG);
            break;

        case 'u':
            disableMPG = true;
            break;

        case 'S':                                   // Spindle
            if(modeMPG && grbl.state == Idle) {
                bool spindle_on = !spindle_state.on && spindle_state.mpg_rpm > 0;
                spindle_state.ccw = signalSpindleDir();
                strcpy(command, spindle_on ? (spindle_state.ccw ? "M4" : "M3") : "M5");
                if(spindle_on)
                    sprintf(append(command), "S%d", spindle_state.mpg_rpm);
                serialWriteLn((char *)command);
                command[0] = '\0';
            }
            break;

        case 'M':                                   // Mist override
            serialPutC(CMD_COOLANT_MIST_OVR_TOGGLE);
            break;

        case 'C':                                   // Coolant override
            serialPutC(CMD_COOLANT_FLOOD_OVR_TOGGLE);
            break;

        case 'a':                                   // Lock X-axis DRO for manual MPG sync
            if(modeMPG && grbl.state == Idle) {
                if(!(axis[X_AXIS].dro_lock = !axis[X_AXIS].dro_lock))
                    axis[X_AXIS].mpg_position = MPG_GetPosition()->x.position;
                displayPosition(X_AXIS, axis[X_AXIS].position, true);
            }
            break;

        case 'e':                                   // Lock Z-axis DRO for manual MPG sync
            if(modeMPG && grbl.state == Idle) {
                if(!(axis[Z_AXIS].dro_lock = !axis[Z_AXIS].dro_lock))
                    axis[Z_AXIS].mpg_position = MPG_GetPosition()->z.position;
                displayPosition(Z_AXIS, axis[Z_AXIS].position, true);
            }
            break;

        case 'm':
            if(modeMPG && grbl.state == Idle)
                displayMPGFactor(X_AXIS, ++axis[X_AXIS].mpg_idx);
            break;

        case 'n':
            if(modeMPG && grbl.state == Idle)
                displayMPGFactor(Y_AXIS, ++axis[Y_AXIS].mpg_idx);
            break;

        case 'o':
            if(modeMPG && grbl.state == Idle)
                displayMPGFactor(Z_AXIS, ++axis[Z_AXIS].mpg_idx);
            break;

        case 'A':
            if(modeMPG && grbl.state == Idle) {
                if(useWPos)
                    axis[X_AXIS].mpg_base = 0.0;
                serialWriteLn("G90G10L20P0X0");     // Zero X-axis
            }
            break;

        case 'E':
            if(modeMPG && grbl.state == Idle) {
                if(useWPos)
                    axis[Z_AXIS].mpg_base = 0.0;
                serialWriteLn("G90G10L20P0Z0");     // Zero Z-axis
            }
            break;

        case '!':                                   //Feed hold
            if(modeMPG)
                serialPutC(CMD_FEED_HOLD);
            else {
                event_count.signal_reset = event_interval.signal_reset;
                signalFeedHold(true);
            }
            break;

        case '~':                                   // Cycle start
            if(modeMPG)
                serialPutC(CMD_CYCLE_START);
            else {
                event_count.signal_reset = event_interval.signal_reset;
                signalCycleStart(true);
            }
            break;

        case 'h':                                   // "toggle" jog mode
            setJogMode(jogMode == JogMode_Step ? JogMode_Fast : (jogMode == JogMode_Fast ? JogMode_Slow : JogMode_Step));
            break;

        case 'H':                                   // Home axes
//            strcpy(command, "$H");
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
            strcpy(command, "$J=G91X-?Z?F");
            break;

        case 'r':                                   // Jog XY-axes NE
            strcpy(command, "$J=G91X?Z-?F");
            break;

        case 's':                                   // Jog XY-axes NW
            strcpy(command, "$J=G91X?Z?F");
            break;

        case 't':                                   // Jog XY-axes SW
            strcpy(command, "$J=G91X-?Z-?F");
            break;

        case 'U':                                   // Jog Z-axis up
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

static void clearMessage (void)
{
    msg = false;
    UILibTextBoxClear(txtResponseL);
    UILibTextBoxClear(txtResponseR);
}

void keyEvent (bool keyDown)
{
    if(keyDown)
        event |= EVENT_KEYDOWN;
    else
        event |= EVENT_KEYUP;

    keyreleased = !keyDown;
}

static void parseGrblData (char *line)
{
    uint32_t c;

    if(line[0] == '<') {

        line = strtok(&line[1], "|");

        if(line) {

            if(grblParseState(line, &grbl)) {
                displayGRBLState();
                if(msg && grbl.state != Alarm)
                    clearMessage();
            }

            line = strtok(NULL, "|");
        }

        while(line) {

            if(!strncmp(line, "WPos:", 5)) {
                useWPos = true;
                parsePositions(line + 5, WCOChanged || endMove);
                endMove = false;
                if(mpgReset && grbl.state == Idle) {
                    mpgReset = false;
                    MPG_ResetPosition(false);
                }

            } else if(!strncmp(line, "MPos:", 5)) {
                useWPos = false;
                parsePositions(line + 5, WCOChanged || endMove);
                endMove = false;
                if(mpgReset && grbl.state == Idle) {
                    mpgReset = false;
                    MPG_ResetPosition(false);
                }

            } else if(!strncmp(line, "FS:", 3))
                displayFeedRateSpeed(line + 3);

            else if(!strncmp(line, "WCO:", 4))
                parseWCO(line + 4);

            else if(!strncmp(line, "Pn:", 3))
                drawString(font_23x16, 220, STATUSROW, line, true);

            else if(!strncmp(line, "A:", 2)) {

                line = &line[2];
                leds.flood = 0;
                leds.mist = 0;
                leds.spindle = 0;

                while((c = *line++)) {
                    switch(c) {

                        case 'M':
                            leds.mist = 1;
                            break;

                        case 'F':
                           leds.flood = 1;
                           break;

                        case 'S':
                        case 'C':
                           leds.spindle = 1;
                           break;
                    }
                }

                keypad_leds(leds);

            } else if(!strncmp(line, "MPG:", 4)) {
                modeMPG = line[4] == '1';
                signalMPGMode(modeMPG);
                if(modeMPG) {
                    serialWriteLn("$G");
                    MPG_ResetPosition(true);
                    displayMPGFactor(X_AXIS, axis[X_AXIS].mpg_idx);
                    displayMPGFactor(Y_AXIS, axis[Y_AXIS].mpg_idx);
                    displayMPGFactor(Z_AXIS, axis[Z_AXIS].mpg_idx);
                } else {
                    c = 3;
                    do {
                        if(axis[--c].dro_lock) {
                            axis[c].dro_lock = false;
                            displayPosition(c, axis[c].position, true);
                        }
                        setMPGFactorBG(c, Black);
                    } while(c);
                }
                setColor(modeMPG ? Blue : White);
                leds.mode = modeMPG;
                keypad_leds(leds);
                drawStringAligned(font_23x16, 0, 20, banner, Align_Center, screen->Width, true);
                setColor(White);
            }

            line = strtok(NULL, "|");
        }

    } else if(!strncmp(line, "[GC:", 4)) {

        line = strtok(&line[4], " ");

        leds.flood = 0;
        leds.mist = 0;

        while(line) {

            if(!strncmp(line, "F", 1))
                displayFeedRate(line + 1);

            if(!strncmp(line, "S", 1))
                displayRPM(line + 1);

            if(!strncmp(line, "G90", 3))
                absDistance = true;

            if(!strncmp(line, "G91", 3))
                absDistance = false;

            if(!strncmp(line, "M5", 2))
                leds.spindle = spindle_state.on = false;
            else if(!strncmp(line, "M3", 2) || !strncmp(line, "M4", 2))
                leds.spindle = spindle_state.on = true;

            if(!strncmp(line, "M7", 2))
                leds.mist = 1;

            if(!strncmp(line, "M8", 2))
               leds.flood = 1;

            line = strtok(NULL, " ");
        }

        keypad_leds(leds);

    } else if(!strncmp(line, "[MSG:", 5)) {
        msg = true;
        UILibTextBoxDisplay(txtResponseL, &line[5]);
    } else if(!strncmp(line, "error:", 6) || !strncmp(line, "ALARM:", 6)) {
        msg = true;
        UILibTextBoxDisplay(txtResponseR, line);
    }
}

void DROProcessEvents (void)
{
    if(!active)
        return;

    if(event) {

        if(event & EVENT_KEYUP) {
            event &= ~EVENT_KEYUP;
            if(jogging) {
             //   keypad_flush();
                jogging = false;
                mpgReset = true;
                serialPutC(CMD_JOG_CANCEL);
            }
            if(disableMPG) {
                disableMPG = false;
                MPG_ResetPosition(false);
            }
        }

        if(event & EVENT_KEYDOWN) {
            processKeypress();
            if(!keypad_has_keycode())
                event &= ~EVENT_KEYDOWN;
        }

        if(event & EVENT_MPG) {
            event &= ~EVENT_MPG;
            if(MPG_Move())
                mpgMove = true;
            else if(mpgMove) {
                if(leds.run && grbl.state == Idle) {
                    leds.run = false;
                    keypad_leds(leds);
                }
                endMove = true;
                mpgMove = false;
            }
        }

        if(event & EVENT_DRO) {
            event &= ~EVENT_DRO;
            if(!mpgMove)
                serialPutC('?'); // Request realtime status from grbl
        }

        if(event & EVENT_SIGNALS) {
            event &= ~EVENT_SIGNALS;
            signalCycleStart(false);
            signalFeedHold(false);
        }
    }
}

static void canvasHandler (Widget *self, Event *uievent)
{
    switch(uievent->reason) {

        case EventNullEvent:
            if(modeMPG) {
                if(!(--event_count.dro_refresh)) {
                    event_count.dro_refresh = event_interval.dro_refresh;
                    event |= EVENT_DRO;
                }

                if(!(--event_count.mpg_refresh)) {
                    event_count.mpg_refresh = event_interval.mpg_refresh;
                    event |= EVENT_MPG;
                }
            }

            if(event_count.signal_reset && !(--event_count.signal_reset))
                event |= EVENT_SIGNALS;
            break;

        case EventPointerUp:
            uievent->claimed = true;
            if(grbl.state == Idle || grbl.state == Alarm || grbl.state == Unknown) {
                active = false;
                MenuShowCanvas();
            }
            break;

        case EventPointerChanged:
            uievent->claimed = true;
            spindleRPM = NavigatorGetYPosition() * 3;
            break;

        case EventWidgetPainted:
            event = 0;
            setColor(modeMPG ? Blue : White);
            setBackgroundColor(canvasMain->widget.bgColor);
            drawStringAligned(font_23x16, 0, 20, banner, Align_Center, screen->Width, true);
            setColor(White);
            if(axis[X_AXIS].visible)
                drawStringAligned(POSFONT, 0, axis[X_AXIS].row, "X:", Align_Right, POSCOL - 5, false);
            if(axis[Y_AXIS].visible)
                drawStringAligned(POSFONT, 0, axis[Y_AXIS].row, "Y:", Align_Right, POSCOL - 5, false);
            if(axis[Z_AXIS].visible)
                drawStringAligned(POSFONT, 0, axis[Z_AXIS].row, "Z:", Align_Right, POSCOL - 5, false);
            drawString(font_freepixel_17x34, 5, RPMROW, "RPM:", false);
            drawString(font_23x16, 220, RPMROW - 3, "Jog:", false);
            drawString(font_23x16, 87, STATUSROW, "Feed:", true);
            setJogMode(jogMode);
            displayGRBLState();
            if(modeMPG) {
                displayMPGFactor(X_AXIS, axis[X_AXIS].mpg_idx);
                displayMPGFactor(Y_AXIS, axis[Y_AXIS].mpg_idx);
                displayMPGFactor(Z_AXIS, axis[Z_AXIS].mpg_idx);
            }
            WCOChanged = true;
            leds = keypad_GetLedState();
            setKeyclickCallback(keyEvent);
            setGrblReceiveCallback(parseGrblData);
            NavigatorSetPosition(0, (uint32_t)(spindleRPM / 3.0f), 0);
            active = true;
            break;

        case EventWidgetClosed:
            setKeyclickCallback(0);
            setGrblReceiveCallback(0);
            active = false;
            break;

    }
}

void DROInitCanvas (void)
{
    int_fast8_t i;

    signalsInit();
    serialInit();

//    RPM_Init();
    MPG_Init();
    keypad_setup();

    driver_settings_restore(1);

    memset(&axis, 0, sizeof(axis));

    axis[X_AXIS].row = XROW;
    axis[X_AXIS].visible = true;
//    axis[Y_AXIS].row = YROW;
    axis[Y_AXIS].visible = false;
    axis[Z_AXIS].row = YROW;
    axis[Z_AXIS].visible = true;

    i = 3;
    do {
        axis[--i].position = HUGE_VALF;
        axis[i].mpg_factor = mpgFactors[axis[i].mpg_idx];
    } while(i);

}

void DROShowCanvas (lcd_display_t *lcd_screen)
{
    screen = lcd_screen;

    if(!canvasMain) {

        canvasMain = UILibCanvasCreate(0, 0, screen->Width, screen->Height, canvasHandler);
        canvasMain->widget.bgColor = Black;

        txtGrblState = UILibTextBoxCreate((Widget *)canvasMain, font_23x16, White, 5, STATUSROW, 80, NULL);
        txtResponseL = UILibTextBoxCreate((Widget *)canvasMain, font_23x16, White, 0, MSGROW, 209, NULL);
        txtResponseR = UILibTextBoxCreate((Widget *)canvasMain, font_23x16, Red, 210, MSGROW, 108, NULL);
        txtResponseR->widget.flags.alignment = Align_Right;
    }

    // reset event counters
    event_count.dro_refresh = event_interval.dro_refresh;
    event_count.mpg_refresh = event_interval.mpg_refresh;
    event_count.signal_reset = 0;

    signalMPGMode(modeMPG);
    keypad_leds(leds);

    UILibCanvasDisplay(canvasMain);
}
