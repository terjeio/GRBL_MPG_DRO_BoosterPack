/*
 * dro.c - DRO canvas, main screen
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v1.0.11 / 2022-01-28 / (c)Io Engineering / Terje
 */

/*

Copyright (c) 2018-2022, Terje Io
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its contributors may
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

#include "../LCD/lcd.h"
#include "../UILib/uilib.h"
#include "../fonts.h"
#include "../fonts/arial_48x55.h"
#include "../config.h"
#include "../grblcomm.h"
#include "../keypad.h"
#include "../interface.h"

#include "menu.h"

//#define LATHEMODE
#define RPMROW 200
#define STATUSROW 218
#define MSGROW 238
#ifdef LATHEMODE
#define XROW 95
#else
#define XROW 82
#endif
#define YROW XROW + 45
#define ZROW YROW + 45
#define POSCOL 50
#define POSFONT font_arial_48x55

// Event flags
#define EVENT_DRO            (1<<0)
#define EVENT_MPG            (1<<1)
#define EVENT_SIGNALS        (1<<2)
#define EVENT_KEYDOWN        (1<<3)
#define EVENT_KEYUP          (1<<4)
#define EVENT_JOGMODECHANGED (1<<5)

#define MIN(a, b) (((a) > (b)) ? (b) : (a))

typedef struct {
    float mpg_base;
    float mpg_factor;
    uint_fast8_t mpg_idx;
    int32_t mpg_position;
    bool dro_lock;
    bool visible;
    uint16_t row;
    Label *lblAxis;
    const char *label;
} axis_data_t;

typedef struct {
    uint_fast16_t dro_refresh;
    uint_fast16_t mpg_refresh;
    uint_fast16_t signal_reset;
} event_counters_t;

//

const char *const jogModeStr[] = { "Fast", "Slow", "Step"};
const float mpgFactors[2] = {1.0f, 10.0f};

//

volatile uint_fast8_t event = 0;
static uint_fast8_t mpg_axis = X_AXIS;
static float mpg_rpm = 200.0f;
static bool mpgMove = false, endMove = false;
static bool jogging = false, keyreleased = true, disableMPG = false, mpgReset = false, active = false;
static bool isLathe = false;
static float angle = 0.0f;
static uint32_t nav_midpos = 0;
static jogmode_t jogMode = JogMode_Slow;
static event_counters_t event_count;
static grbl_data_t *grbl_data = NULL;
static Canvas *canvasMain = 0;
static Label *lblDevice, *lblResponseL = NULL, *lblResponseR = NULL, *lblGrblState = NULL, *lblPinState = NULL, *lblFeedRate = NULL, *lblRPM = NULL, *lblJogMode = NULL;
static lcd_display_t *screen;
static settings_t *settings = NULL;
static grbl_info_t *grbl_info = NULL;
static axis_data_t axis[3] = {
    { .label = "X:"},
    { .label = "Y:"},
    { .label = "Z:"}
};
static event_counters_t event_interval = {
    .dro_refresh  = 20,
    .mpg_refresh  = 10,
    .signal_reset = 20
};

static leds_t leds = {
    .value = 0
};
#ifdef UART_MODE
static bool jogModePending = false;
#endif

static void displayGrblData (char *line);

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
    mpg_reset();
    axis[X_AXIS].mpg_position = 0;
    axis[Y_AXIS].mpg_position = 0;
    axis[Z_AXIS].mpg_position = 0;
    if(!(grbl_data->awaitWCO = await)) {
        axis[X_AXIS].mpg_base = grbl_data->position[X_AXIS];
        axis[Y_AXIS].mpg_base = grbl_data->position[Y_AXIS];
        axis[Z_AXIS].mpg_base = grbl_data->position[Z_AXIS];
    }
}

static bool MPG_Move (void)
{
    mpg_t *pos;
    uint32_t velocity = 0;
    float delta_x = 0.0f, delta_y = 0.0f, delta_z = 0.0f;
    bool updated = false;
    static char buffer[50];

    if(grbl_data->awaitWCO || jogging || grbl_data->alarm || !(grbl_data->grbl.state == Idle || grbl_data->grbl.state == Run))
        return false;

    strcpy(buffer, "G1");

    pos = mpg_getPosition();

    if(!axis[Z_AXIS].dro_lock) {
        if(pos->z.position != axis[Z_AXIS].mpg_position) {
            delta_z = (float)(pos->z.position - axis[Z_AXIS].mpg_position) * axis[Z_AXIS].mpg_factor / 400.0f;
            axis[Z_AXIS].mpg_position = pos->z.position;
            if(grbl_data->absDistance)
                axis[Z_AXIS].mpg_base += delta_z;
            velocity = pos->z.velocity;
            sprintf(append(buffer), "Z%.3f", grbl_data->absDistance ? axis[Z_AXIS].mpg_base - grbl_data->offset[Z_AXIS] : delta_z);
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
            if(grbl_data->absDistance)
                axis[X_AXIS].mpg_base += delta_x;
            sprintf(append(buffer), "X%.3f", grbl_data->absDistance ? axis[X_AXIS].mpg_base - grbl_data->offset[X_AXIS] : delta_x);
        }
    }

    if(!axis[Y_AXIS].dro_lock) {
        if(pos->y.position != axis[Y_AXIS].mpg_position) {
            delta_y = (float)(pos->y.position - axis[Y_AXIS].mpg_position) * axis[Y_AXIS].mpg_factor / 400.0f;
            axis[Y_AXIS].mpg_position = pos->y.position;
            velocity = velocity == 0 ? pos->y.velocity : MIN(pos->y.velocity, velocity);
        }

        if(delta_y != 0.0f) {
            if(grbl_data->absDistance)
                axis[Y_AXIS].mpg_base += delta_y;
            sprintf(append(buffer), "Y%.3f", grbl_data->absDistance ? axis[Y_AXIS].mpg_base - grbl_data->offset[Y_AXIS] : delta_y);
        }
    }

    if((updated = delta_x != 0.0f || delta_y != 0.0f || delta_z != 0.0f)) {

        sprintf(append(buffer), "F%d", velocity * 50);
        serial_writeLn(buffer);
//        drawString(font_23x16, 5, 40, buffer, true);

        setColor(Coral);

        if(delta_x != 0.0f)
            drawString(POSFONT, POSCOL, axis[X_AXIS].row, ftoa(axis[X_AXIS].mpg_base - grbl_data->offset[X_AXIS], "% 9.3f"), true);

        if(delta_y != 0.0f)
            drawString(POSFONT, POSCOL, axis[Y_AXIS].row, ftoa(axis[Y_AXIS].mpg_base - grbl_data->offset[Y_AXIS], "% 9.3f"), true);

        if(delta_z != 0.0f)
            drawString(POSFONT, POSCOL, axis[Z_AXIS].row, ftoa(axis[Z_AXIS].mpg_base - grbl_data->offset[Z_AXIS], "% 9.3f"), true);

        setColor(White);

        if(!leds.run) {
            leds.run = true;
            leds_setState(leds);
        }
    }

    return updated;
}

static void displayBanner (RGBColor_t color)
{
    lblDevice->widget.fgColor = color;
    UILibLabelDisplay(lblDevice, grbl_info->device);
}

static void displayPosition (uint_fast8_t i)
{
    if(axis[i].visible) {
        setColor(axis[i].dro_lock ? Yellow : White);
        drawString(POSFONT, POSCOL, axis[i].row, ftoa(grbl_data->position[i] - grbl_data->offset[i], "% 9.3f"), true);
        setColor(White);
    }
}

static void setMPGFactorBG (uint_fast8_t i, RGBColor_t color)
{
    setColor(color);
    fillRect(268, axis[i].row - 31, screen->Width - 1, axis[i].row - 8);
    setColor(White);
}

static void displayMPGFactor (uint_fast8_t i, uint_fast8_t mpg_idx)
{
    char buf[5];

    axis[i].mpg_idx = mpg_idx > (sizeof(mpgFactors) / sizeof(float)) - 1 ? 0 : mpg_idx;
    axis[i].mpg_factor = mpgFactors[axis[i].mpg_idx];

    if(axis[i].visible) {
        setMPGFactorBG(i, axis[i].mpg_factor == 1.0f ? Black : Red);
        sprintf(buf, "x%d", (uint32_t)axis[i].mpg_factor);
        drawString(font_23x16, 269, axis[i].row - 10, buf, false);
    }
}

static void displayJogMode (jogmode_t jogMode)
{
    lblJogMode->widget.fgColor = jogMode == JogMode_Fast ? Red : White;
    UILibLabelDisplay(lblJogMode, jogModeStr[jogMode]);
}

static void displayXMode (const char *mode)
{
    setColor(mode[0] == '?' ? Red : LawnGreen);
    drawString(font_23x16, 269, axis[X_AXIS].row - 32, mode, true);
    setColor(White);
}

static void driver_settings_restore (uint8_t restore_flag)
{

}

static void processKeypress (void)
{
    static char command[30];

    bool addedGcode, jogCommand = false;
    char keycode;

    if(!(keycode = keypad_get_keycode()))
        return;

    if(grbl_data->alarm && !(keycode == 'H' || keycode == '\r'))
        return;

    command[0] = '\0';

    switch(keycode) {

        case '\r':
            if(!grbl_data->mpgMode || grbl_data->grbl.state != Hold)
                signal_setMPGMode(!grbl_data->mpgMode);
            break;

        case 'T':
            mpg_axis = mpg_axis == Z_AXIS ? X_AXIS : mpg_axis + 1;
            for(uint_fast8_t i = 0; i < 3; i++) {
                if(axis[i].visible) {
                    axis[i].lblAxis->widget.fgColor = i == mpg_axis ? Green : White;
                    UILibLabelDisplay(axis[i].lblAxis, axis[i].label);
                }
            }
            mpg_setActiveAxis(mpg_axis);
            break;

        case '4':
            mpg_axis = X_AXIS;
            for(uint_fast8_t i = 0; i < 3; i++) {
                if(axis[i].visible) {
                    axis[i].lblAxis->widget.fgColor = i == mpg_axis ? Green : White;
                    UILibLabelDisplay(axis[i].lblAxis, axis[i].label);
                }
            }
            mpg_setActiveAxis(mpg_axis);
            break;

        case '5':
            mpg_axis = Y_AXIS;
            for(uint_fast8_t i = 0; i < 3; i++) {
                if(axis[i].visible) {
                    axis[i].lblAxis->widget.fgColor = i == mpg_axis ? Green : White;
                    UILibLabelDisplay(axis[i].lblAxis, axis[i].label);
                }
            }
            mpg_setActiveAxis(mpg_axis);
            break;

        case '6':
            mpg_axis = Z_AXIS;
            for(uint_fast8_t i = 0; i < 3; i++) {
                if(axis[i].visible) {
                    axis[i].lblAxis->widget.fgColor = i == mpg_axis ? Green : White;
                    UILibLabelDisplay(axis[i].lblAxis, axis[i].label);
                }
            }
            mpg_setActiveAxis(mpg_axis);
            break;

        case 'P':
        case 'S':                                   // Spindle
            if(grbl_data->mpgMode)
                if(grbl_data->grbl.state == Idle) {
                    bool spindle_on = !grbl_data->spindle.on;
                    if(spindle_on) {
                        grbl_data->spindle.ccw = signal_getSpindleDir();
                        if(mpg_rpm == 0)
                            mpg_rpm = 400.0f;
                    }
                    strcpy(command, spindle_on ? (grbl_data->spindle.ccw ? "M4" : "M3") : "M5");
                    if(spindle_on)
                        sprintf(append(command), "S%d", (int32_t)mpg_rpm);
                    serial_writeLn((char *)command);
                    command[0] = '\0';
                } else if(grbl_data->grbl.state == Hold || grbl_data->grbl.state == Door)
                    serial_putC(CMD_OVERRIDE_SPINDLE_STOP);
            break;

        case 'M':                                   // Mist override
            serial_putC(CMD_OVERRIDE_COOLANT_MIST_TOGGLE);
            break;

        case 'C':                                   // Coolant override
            serial_putC(CMD_OVERRIDE_COOLANT_FLOOD_TOGGLE);
            break;

        case 'a':                                   // Lock X-axis DRO for manual MPG sync
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle) {
                if(!(axis[X_AXIS].dro_lock = !axis[X_AXIS].dro_lock))
                    axis[X_AXIS].mpg_position = mpg_getPosition()->x.position;
                displayPosition(X_AXIS);
            }
            break;
 
        case 'g':                                   // Lock Y-axis DRO for manual MPG sync
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle) {
                if(!(axis[Y_AXIS].dro_lock = !axis[Y_AXIS].dro_lock))
                    axis[Y_AXIS].mpg_position = mpg_getPosition()->y.position;
                displayPosition(Y_AXIS);
            }
            break;
 
        case 'e':                                   // Lock Z-axis DRO for manual MPG sync
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle) {
                if(!(axis[Z_AXIS].dro_lock = !axis[Z_AXIS].dro_lock))
                    axis[Z_AXIS].mpg_position = mpg_getPosition()->z.position;
                displayPosition(Z_AXIS);
            }
            break;

        case 'm':
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle)
                displayMPGFactor(X_AXIS, ++axis[X_AXIS].mpg_idx);
            break;

        case 'n':
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle)
                displayMPGFactor(Y_AXIS, ++axis[Y_AXIS].mpg_idx);
            break;

        case 'o':
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle)
                displayMPGFactor(Z_AXIS, ++axis[Z_AXIS].mpg_idx);
            break;

        case 'A':
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle) {
                if(grbl_data->useWPos)
                    axis[X_AXIS].mpg_base = 0.0;
                serial_writeLn("G90G10L20P0X0");     // Zero X-axis
            }
            break;

        case 'G':
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle) {
                if(grbl_data->useWPos)
                    axis[Y_AXIS].mpg_base = 0.0;
                serial_writeLn("G90G10L20P0Y0");     // Zero Y-axis
            }
            break;

        case 'E':
            if(grbl_data->mpgMode && grbl_data->grbl.state == Idle) {
                if(grbl_data->useWPos)
                    axis[Z_AXIS].mpg_base = 0.0;
                serial_writeLn("G90G10L20P0Z0");     // Zero Z-axis
            }
            break;

        case CMD_FEED_HOLD_LEGACY:                  //Feed hold
            if(grbl_data->mpgMode)
                serial_putC(mapRTC2Legacy(CMD_FEED_HOLD));
            else {
                event_count.signal_reset = event_interval.signal_reset;
                signal_setFeedHold(true);
            }
            break;

        case CMD_CYCLE_START_LEGACY:                // Cycle start
            if(grbl_data->mpgMode)
                serial_putC(mapRTC2Legacy(CMD_CYCLE_START));
            else {
                event_count.signal_reset = event_interval.signal_reset;
                signal_setCycleStart(true);
            }
            break;

        case 'I':
            serial_putC(CMD_FEED_OVR_RESET);
            break;

        case 'i':
            serial_putC(CMD_OVERRIDE_FEED_COARSE_PLUS);
            break;

        case 'j':
            serial_putC(CMD_OVERRIDE_FEED_COARSE_MINUS);
            break;

        case 'K':
            serial_putC(CMD_OVERRIDE_SPINDLE_RESET);
            break;

         case 'k':
            serial_putC(CMD_OVERRIDE_SPINDLE_COARSE_PLUS);
            break;

        case 'z':
            serial_putC(CMD_OVERRIDE_SPINDLE_COARSE_MINUS);
            break;

        case CMD_STOP:
        case CMD_SAFETY_DOOR:
        case CMD_OPTIONAL_STOP_TOGGLE:
        case CMD_SINGLE_BLOCK_TOGGLE:
        case CMD_PROBE_CONNECTED_TOGGLE:
        case CMD_OVERRIDE_FAN0_TOGGLE:
        case CMD_OVERRIDE_COOLANT_FLOOD_TOGGLE:
        case CMD_OVERRIDE_COOLANT_MIST_TOGGLE:
        case CMD_OVERRIDE_FEED_COARSE_PLUS:
        case CMD_OVERRIDE_FEED_COARSE_MINUS:
        case CMD_OVERRIDE_FEED_FINE_PLUS:
        case CMD_OVERRIDE_FEED_FINE_MINUS:
        case CMD_OVERRIDE_RAPID_RESET:
        case CMD_OVERRIDE_RAPID_MEDIUM:
        case CMD_OVERRIDE_RAPID_LOW:
        case CMD_OVERRIDE_SPINDLE_RESET:
        case CMD_OVERRIDE_SPINDLE_COARSE_PLUS:
        case CMD_OVERRIDE_SPINDLE_COARSE_MINUS:
        case CMD_OVERRIDE_SPINDLE_FINE_PLUS:
        case CMD_OVERRIDE_SPINDLE_FINE_MINUS:
        case CMD_OVERRIDE_SPINDLE_STOP:
            serial_putC(keycode);
            break;

        case 'H':                                   // Home axes
            if(settings && settings->homing_enabled) {
                signal_setLimitsOverride(false);
                strcpy(command, "$H");
            }
            break;

        case JOG_XR:                                // Jog X
            strcpy(command, "$J=G91X?F");
            break;

        case JOG_XL:                                // Jog -X
            strcpy(command, "$J=G91X-?F");
            break;

        case JOG_YF:                                // Jog Y
            strcpy(command, "$J=G91Y?F");
            break;

        case JOG_YB:                                // Jog -Y
            strcpy(command, "$J=G91Y-?F");
            break;

        case JOG_ZU:                                // Jog Z
            strcpy(command, "$J=G91Z?F");
            break;

        case JOG_ZD:                                // Jog -Z
            strcpy(command, "$J=G91Z-?F");
            break;

        case JOG_XRYF:                              // Jog XY
            strcpy(command, "$J=G91X?Y?F");
            break;

        case JOG_XRYB:                              // Jog X-Y
            strcpy(command, "$J=G91X?Y-?F");
            break;

        case JOG_XLYF:                              // Jog -XY
            strcpy(command, "$J=G91X-?Y?F");
            break;

        case JOG_XLYB:                              // Jog -X-Y
            strcpy(command, "$J=G91X-?Y-?F");
            break;

        case JOG_XRZU:                              // Jog XZ
            strcpy(command, "$J=G91X?Z?F");
            break;

        case JOG_XRZD:                              // Jog X-Z
            strcpy(command, "$J=G91X?Z-?F");
            break;

        case JOG_XLZU:                              // Jog -XZ
            strcpy(command, "$J=G91X-?Z?F");
            break;

        case JOG_XLZD:                              // Jog -X-Z
            strcpy(command, "$J=G91X-?Z-?F");
            break;
    }

    if(command[0] != '\0') {

        // add distance and speed to jog commands
        if((jogCommand = (command[0] == '$' && command[1] == 'J')))
            switch(jogMode) {

            case JogMode_Slow:
                strrepl(command, '?', ftoa(settings->jog_config.slow_distance, "%5.0f"));
                strcat(command, ftoa(settings->jog_config.slow_speed, "%5.0f"));
                break;

            case JogMode_Step:
                strrepl(command, '?', ftoa(settings->jog_config.step_distance, "%5.3f"));
                strcat(command, ftoa(settings->jog_config.step_speed, "%5.0f"));
                break;

            default:
                strrepl(command, '?', ftoa(settings->jog_config.fast_distance, "%5.0f"));
                strcat(command, ftoa(settings->jog_config.fast_speed, "%5.0f"));
                break;

        }

        if(!(jogCommand && keyreleased)) { // key still pressed? - do not execute jog command if released!
            addedGcode = true;
            serial_writeLn((char *)command);
            jogging = jogging || (jogCommand && addedGcode);
        }
    }
}

void keyEvent (bool keyDown, char key)
{
    // NOTE: key is read from input buffer during event processing
    if(keyDown)
        event |= EVENT_KEYDOWN;
    else
        event |= EVENT_KEYUP;

    keyreleased = !keyDown;
}

void jogModeChanged (jogmode_t mode)
{
    jogMode = mode;
    event |= EVENT_JOGMODECHANGED;
#ifdef UART_MODE
    jogModePending = grbl_data->mpgMode;
#endif
}

static void on_settings_received (settings_t *setn)
{
    settings = setn;

    if(settings->is_loaded) {

        if((settings->mode == 2)) {
            axis[Y_AXIS].visible = false;
            axis[Z_AXIS].row = YROW;
            axis[Z_AXIS].visible = true;
            displayXMode("?");
        }
    }

    displayBanner(grbl_data->mpgMode ? (settings->is_loaded ? Blue : Red) : White);

//    if(grbl_data->mpgMode) // Get parser state
//        serial_writeLn("$G");
}

static void on_info_received (grbl_info_t *info)
{
    grbl_info = info;

    if(settings == NULL || !settings->is_loaded)
        grblGetSettings(on_settings_received);
}

static void displayGrblData (char *line)
{
    uint32_t c;

    if(endMove || mpgReset)
        grbl_data->changed.offset = true;

    if(grbl_data->changed.flags) {

        if(grbl_data->changed.reset)
            settings->is_loaded = false;

        if(grbl_data->changed.state) {
            lblGrblState->widget.fgColor = grbl_data->grbl.state_color;
            UILibLabelDisplay(lblGrblState, grbl_data->grbl.state_text);
            leds.run = grbl_data->grbl.state == Run || grbl_data->grbl.state == Jog;
            leds.hold = grbl_data->grbl.state == Hold;
            leds_setState(leds);

            switch(grbl_data->grbl.state) {

                case Idle:
                    MPG_ResetPosition(false);
                    break;

                case Home:
                    signal_setLimitsOverride(false);
                    break;

                case Alarm: // Switch to MPG mode if alarm #11 issued (homing required)
                    if(grbl_data->grbl.substate == 11 && !settings->is_loaded && !grbl_data->mpgMode)
                        signal_setMPGMode(true); // default is MPG on
                    break;
            }
        }

        if(grbl_data->changed.alarm) {
            if(grbl_data->alarm) {
                sprintf(line, "ALARM:%d", grbl_data->alarm);
                UILibLabelDisplay(lblResponseR, line);
            } else
                UILibLabelClear(lblResponseR);
        }

        if(grbl_data->changed.error) {
            if(grbl_data->error) {
                sprintf(line, "ERROR:%d", grbl_data->error);
                UILibLabelDisplay(lblResponseR, line);
            } else
                UILibLabelClear(lblResponseR);
        }

        if(grbl_data->changed.message)
             UILibLabelDisplay(lblResponseL, grbl_data->message);

        if(grbl_data->changed.offset || grbl_data->changed.await_wco_ok) {
            if((mpgReset || grbl_data->changed.await_wco_ok) && grbl_data->grbl.state == Idle) {
                mpgReset = false;
                MPG_ResetPosition(false);
            }
            grbl_data->changed.xpos =
            grbl_data->changed.ypos =
            grbl_data->changed.zpos = true;
        }

        if (!mpgMove) {

            if(grbl_data->changed.xpos)
                displayPosition(X_AXIS);

            if(grbl_data->changed.ypos)
                displayPosition(Y_AXIS);

            if(grbl_data->changed.zpos)
                displayPosition(Z_AXIS);

            endMove = false;
        }

        if(grbl_data->changed.mpg) {
            if(grbl_data->mpgMode != signal_getMPGMode())
                signal_setMPGMode(grbl_data->mpgMode);
            keypad_forward(!grbl_data->mpgMode);
            if(grbl_data->mpgMode) {
                if(grbl_info->is_loaded)
                    grblAwaitACK("$G", 50);
                MPG_ResetPosition(true);
                displayMPGFactor(X_AXIS, axis[X_AXIS].mpg_idx);
                displayMPGFactor(Y_AXIS, axis[Y_AXIS].mpg_idx);
                displayMPGFactor(Z_AXIS, axis[Z_AXIS].mpg_idx);
            } else {
                c = 3;
                do {
                    if(axis[--c].dro_lock) {
                        axis[c].dro_lock = false;
                        displayPosition(c);
                    }
                    setMPGFactorBG(c, Black);
                } while(c);                
#ifdef UART_MODE
                if(jogModePending) {
                    jogModePending = false;
                    serial_putC('0' + (char)jogMode);
                }
#endif
            }

            leds.mode = grbl_data->mpgMode;
            leds_setState(leds);
            displayBanner(grbl_data->mpgMode ? (settings->is_loaded ? Blue : Red) : White);
        }

        if(grbl_data->changed.feed) {
            sprintf(line, grbl_data->feed_rate < 1000.0f ? "%6.1f" : "%5.f", grbl_data->feed_rate);
            UILibLabelDisplay(lblFeedRate, line);
        }

        if(grbl_data->changed.rpm) {
            bool display_actual = grbl_data->spindle.on && grbl_data->spindle.rpm_actual > 0.0f;
            if(grbl_data->spindle.rpm_programmed > 0.0f)
                mpg_rpm = grbl_data->spindle.rpm_programmed;
            if(display_actual || leds.spindle != grbl_data->spindle.on) {
                sprintf(line, "%6.1f", display_actual ? grbl_data->spindle.rpm_actual : mpg_rpm);
                lblRPM->widget.fgColor = display_actual ? (grbl_data->spindle.rpm_actual > 2000.0f ? Coral : White) : Coral;
                UILibLabelDisplay(lblRPM, line);
            }
        }

        if(grbl_data->changed.leds) {
            leds.mist = grbl_data->coolant.mist;
            leds.flood = grbl_data->coolant.flood;
            leds.spindle = grbl_data->spindle.on;
            leds_setState(leds);
        }

        if(grbl_data->changed.pins)
            UILibLabelDisplay(lblPinState, grbl_data->pins);

        if(grbl_data->changed.xmode && isLathe)
            displayXMode(grbl_data->xModeDiameter ? "D" : "R");

        if(grbl_data->changed.await_wco_ok)
            MPG_ResetPosition(false);

        grbl_data->changed.flags = 0;
    }

    if(grbl_data->mpgMode) {
        if(grbl_info == NULL || !grbl_info->is_loaded)
           grblGetInfo(on_info_received);
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
                serial_putC(CMD_JOG_CANCEL);
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
                if(leds.run && grbl_data->grbl.state == Idle) {
                    leds.run = false;
                    leds_setState(leds);
                }
                endMove = true;
                mpgMove = false;
            }
        }

        if(event & EVENT_DRO) {
            event &= ~EVENT_DRO;
            if(!mpgMove && settings->is_loaded)
                serial_putC(grbl_data->awaitWCO ? CMD_STATUS_REPORT_ALL : mapRTC2Legacy(CMD_STATUS_REPORT)); // Request realtime status from grbl
        }

        if(event & EVENT_JOGMODECHANGED) {
            displayJogMode(jogMode);
            event &= ~EVENT_JOGMODECHANGED;
        }

        if(event & EVENT_SIGNALS) {
            event &= ~EVENT_SIGNALS;
            signal_setCycleStart(false);
            signal_setFeedHold(false);
        }
    }
}

static void canvasHandler (Widget *self, Event *uievent)
{
    static bool isReady = false;

    switch(uievent->reason) {

        case EventNullEvent:
            isReady = true;
            if(grbl_data->mpgMode) {
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
            if(grbl_data->grbl.state == Idle || grbl_data->grbl.state == Alarm || grbl_data->grbl.state == Unknown || grbl_data->alarm) {
                active = false;
                MenuShowCanvas(grbl_data->mpgMode);
            }
            break;

        case EventPointerChanged: // Used to set spindle RPM
            {
                uievent->claimed = true;
                // TODO: issue spindle override command(s) when not idle?
                if(grbl_data->mpgMode && !mpgReset && grbl_data->grbl.state == Idle) {
                    char command[10];
                    float rpm = (float)((int32_t)(uievent->y - nav_midpos));
                    if(rpm > 5.0f)
                        rpm = 5.0f;
                    else if(rpm < -5.0f)
                        rpm = -5.0f;
                    rpm = mpg_rpm + rpm * 8.0f;
                    if(rpm < 0.0f || rpm < settings->spindle.rpm_min)
                        rpm = settings->spindle.rpm_min;
                    else if (rpm > settings->spindle.rpm_max)
                        rpm = settings->spindle.rpm_max;
                    mpg_rpm = rpm;
                    if(grbl_data->spindle.on) {
                        sprintf(command, "S%d", (int32_t)mpg_rpm);
                        serial_writeLn((char *)command);
                    }
                    sprintf(command, "%6.1f", mpg_rpm);
                    lblRPM->widget.fgColor = Coral;
                    UILibLabelDisplay(lblRPM, command);
                } else if(isReady) {
                    int32_t override = (int32_t)(uievent->y - nav_midpos);
                    if(override > 0) {
                        if(grbl_data->spindle.rpm_override + override > MAX_SPINDLE_RPM_OVERRIDE)
                            override = MAX_SPINDLE_RPM_OVERRIDE - grbl_data->spindle.rpm_override;
                        while(override > SPINDLE_OVERRIDE_COARSE_INCREMENT) {
                            keypad_forward_keypress(CMD_OVERRIDE_SPINDLE_COARSE_PLUS);
                            override -= SPINDLE_OVERRIDE_COARSE_INCREMENT;
                        }
                        while(override >= SPINDLE_OVERRIDE_FINE_INCREMENT) {
                            keypad_forward_keypress(CMD_OVERRIDE_SPINDLE_FINE_PLUS);
                            override -= SPINDLE_OVERRIDE_FINE_INCREMENT;
                        }
                    } else {
                        override = - override;
                        while(override > SPINDLE_OVERRIDE_COARSE_INCREMENT) {
                            keypad_forward_keypress(CMD_OVERRIDE_SPINDLE_COARSE_MINUS);
                            override -= SPINDLE_OVERRIDE_COARSE_INCREMENT;
                        }
                        while(override > SPINDLE_OVERRIDE_FINE_INCREMENT) {
                            keypad_forward_keypress(CMD_OVERRIDE_SPINDLE_FINE_MINUS);
                            override -= SPINDLE_OVERRIDE_FINE_INCREMENT;
                        }
                    }
                }
                NavigatorSetPosition(0, nav_midpos, false);
            }
            break;

        case EventWidgetPainted:;
            char rpm[10];
            event = 0;
            isReady = false;
            setBackgroundColor(canvasMain->widget.bgColor);
            grbl_data = setGrblReceiveCallback(displayGrblData);
            for(uint_fast8_t i = 0; i < 3; i++) {
                if(axis[i].visible) {
                    axis[i].lblAxis->widget.fgColor = i == mpg_axis ? Green : White;
                    UILibLabelDisplay(axis[i].lblAxis, axis[i].label);
#ifdef LATHEMODE
                    if(i == X_AXIS)
                        displayXMode("?");
#endif
                }
            }
            drawString(font_freepixel_17x34, 5, RPMROW, "RPM:", false);
            sprintf(rpm, "%6.1f", mpg_rpm);
            lblRPM->widget.fgColor = Coral;
            UILibLabelDisplay(lblRPM, rpm);
            drawString(font_23x16, 220, RPMROW - 3, "Jog:", false);
            drawString(font_23x16, 87, STATUSROW, "Feed:", true);
            setJogModeChangedCallback(jogModeChanged);
            if(grbl_data->mpgMode) {
                displayMPGFactor(X_AXIS, axis[X_AXIS].mpg_idx);
                displayMPGFactor(Y_AXIS, axis[Y_AXIS].mpg_idx);
                displayMPGFactor(Z_AXIS, axis[Z_AXIS].mpg_idx);
            }
            mpgReset = true;
            leds = leds_getState();
            keypad_forward(!grbl_data->mpgMode);
            setKeyclickCallback2(keyEvent, false);
            NavigatorSetPosition(0, nav_midpos, false);
//            serial_putC(CMD_STATUS_REPORT_ALL); // Request realtime status from grbl
            active = true;
            break;

        case EventWidgetClosed:
            setKeyclickCallback2(NULL, false);
            setJogModeChangedCallback(NULL);
            setGrblReceiveCallback(NULL);
            keypad_forward(false);
            active = false;
            break;
    }
}

void DROInitCanvas (void)
{
    int_fast8_t i;

    delay(1);
    signal_setMPGMode(true); // default is MPG on

    serial_init();

//    RPM_Init();
    keypad_setup();

    driver_settings_restore(1);

    memset(&axis, 0, sizeof(axis));

#ifdef LATHEMODE
    axis[X_AXIS].row = XROW;
    axis[X_AXIS].visible = true;
    axis[Y_AXIS].row = YROW;
    axis[Y_AXIS].visible = false;
    axis[Z_AXIS].row = YROW;
    axis[Z_AXIS].visible = true;
#else
    axis[X_AXIS].row = XROW;
    axis[X_AXIS].visible = true;
    axis[Y_AXIS].row = YROW;
    axis[Y_AXIS].visible = true;
    axis[Z_AXIS].row = ZROW;
    axis[Z_AXIS].visible = true;
#endif

    axis[X_AXIS].label = "X:";
    axis[Y_AXIS].label = "Y:";
    axis[Z_AXIS].label = "Z:";

    i = 3;
    do {
        axis[--i].mpg_factor = mpgFactors[axis[i].mpg_idx];
    } while(i);

}

void DROShowCanvas (lcd_display_t *lcd_screen)
{
    screen = lcd_screen;

    if(!canvasMain) {

        uint_fast8_t i;

        nav_midpos = screen->Height / 2;
        canvasMain = UILibCanvasCreate(0, 0, screen->Width, screen->Height, canvasHandler);
        canvasMain->widget.bgColor = Black;

        lblDevice = UILibLabelCreate((Widget *)canvasMain, font_23x16, White, 2, 20, screen->Width - 4, NULL);
        lblDevice->widget.flags.alignment = Align_Center;

        for(i = 0; i < 3; i++) {
            if(axis[i].visible) {
                axis[i].lblAxis = UILibLabelCreate((Widget *)canvasMain, POSFONT, White, 0, axis[i].row, POSCOL - 5, NULL);
                axis[i].lblAxis->widget.flags.alignment = Align_Right;
            }
        }

        lblRPM = UILibLabelCreate((Widget *)canvasMain, font_freepixel_17x34, White, 60, RPMROW, 100, NULL);
        lblJogMode = UILibLabelCreate((Widget *)canvasMain, font_23x16, White, 265, RPMROW - 3, 53, NULL);
        lblGrblState = UILibLabelCreate((Widget *)canvasMain, font_23x16, White, 5, STATUSROW, 80, NULL);
        lblFeedRate = UILibLabelCreate((Widget *)canvasMain, font_23x16, White, 145, STATUSROW, 65, NULL);
        lblFeedRate->widget.flags.alignment = Align_Right;
        lblPinState = UILibLabelCreate((Widget *)canvasMain, font_23x16, White, 220, STATUSROW, 98, NULL);
        lblResponseL = UILibLabelCreate((Widget *)canvasMain, font_freepixel_9x17, White, 5, MSGROW, 200, NULL);
        lblResponseR = UILibLabelCreate((Widget *)canvasMain, font_23x16, Red, 210, MSGROW, 108, NULL);
        lblResponseR->widget.flags.alignment = Align_Right;
    }

    // reset event counters
    event_count.dro_refresh = event_interval.dro_refresh;
    event_count.mpg_refresh = event_interval.mpg_refresh;
    event_count.signal_reset = 0;

    if(grbl_data)
        signal_setMPGMode(grbl_data->mpgMode);

    if(grbl_info == NULL)
        grblGetInfo(on_info_received);

    leds_setState(leds);

    UILibCanvasDisplay(canvasMain);
}
