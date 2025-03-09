/*
 * parser.c - collects, parses and dispatches lines of data from
 *            grbls serial output stream and/or i2c display protocol
 *
 * v0.0.7 / 2025-02-27 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2018-2025, Terje Io
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
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "parser.h"
#include "i2c_interface.h"

// This array must match the grbl_state_t enum in grblcomm.h!
const char *const grblState[NUMSTATES] = {
    "Unknown",
    "Idle",
    "Run",
    "Jog",
    "Hold",
    "Alarm",
    "Check",
    "Door",
    "Tool",
    "Home",
    "Sleep"
};

// This array must match the grbl_state_t in parser.h!
const RGBColor_t grblStateColor[NUMSTATES] = {
    Black,
    White,
    LightGreen,
    Yellow,
    Coral,
    Red,
    Yellow,
    Coral,
    Yellow,
    LightSkyBlue,
    LightSkyBlue
};

static grbl_data_t grbl_data = {
    .n_axis               = 3,
    .changed              = (uint32_t)-1,
    .position             = {0.0f, 0.0f, 0.0f},
    .offset               = {0.0f, 0.0f, 0.0f},
    .absDistance          = true,
    .xModeDiameter        = false,
    .override.feed_rate   = 100,
    .override.rapid_rate  = 100,
    .override.spindle_rpm = 100,
    .grbl.state           = Unknown,
    .grbl.state_color     = White,
    .grbl.state_text      = "",
    .grbl.substate        = 0,
    .mpgMode              = false,
    .alarm                = 0,
    .error                = 0,
    .pins                 = "",
    .message              = "",
    .useWPos              = false,
    .awaitWCO             = true,
    .spindle.state        = {0},
    .coolant              = {0}
};

#ifdef PARSER_SERIAL_ENABLE
static void parseData (char *block);
static void (*grblTransmitCallback)(bool ok, grbl_data_t *grbl_data) = NULL;
static grbl_settings_received_ptr on_settings_received = NULL;
#endif

static struct {
    grbl_callback_ptr on_report_received;
#ifdef PARSER_SERIAL_ENABLE
    grbl_settings_received_ptr on_settings_received;
    grbl_info_received_ptr on_info_received;
    grbl_sd_files_received_ptr on_sd_files_received;
    grbl_callback_ptr on_line_received;
#endif
} grbl_event = {
    .on_report_received = NULL,
#ifdef PARSER_SERIAL_ENABLE
    .on_settings_received = NULL,
    .on_info_received = NULL,
    .on_sd_files_received = NULL,
    .on_line_received = parseData
#endif
};

static bool legacy_rt_commands = true;
static volatile bool ack_received = false, await = false;

void setGrblLegacyMode (bool on)
{
    legacy_rt_commands = on;
}

char mapRTC2Legacy (char c)
{
    if(legacy_rt_commands) switch(c) {

        case CMD_STATUS_REPORT:
            c = CMD_STATUS_REPORT_LEGACY;
            break;

        case CMD_CYCLE_START:
            c = CMD_CYCLE_START_LEGACY;
            break;

        case CMD_FEED_HOLD:
            c = CMD_FEED_HOLD_LEGACY;
            break;
    }

    return c;
}

grbl_data_t *setGrblReceiveCallback (grbl_callback_ptr fn)
{
    grbl_event.on_report_received = fn;
#ifdef PARSER_SERIAL_ENABLE
    grblTransmitCallback = NULL;
#endif
    grbl_data.changed.flags = (uint32_t)-1;
    grbl_data.changed.await_ack = grbl_data.changed.reset = false;
    grbl_data.changed.unassigned = 0;

    return &grbl_data;
}

static void setLeds (grbl_state_t state)
{
    leds_t leds = (leds_t){ grbl_data.leds.value };

    leds.run = state == Run || state == Jog;
    leds.hold = state == Hold;
    if(grbl_data.changed.leds = grbl_data.leds.value != leds.value)
        grbl_data.leds = leds;
}

bool grblIsMPGActive (void)
{
    return grbl_data.mpgMode;
}

#ifdef PARSER_SERIAL_ENABLE

settings_t settings = {
    .spindle.rpm     = 0.0f,
    .spindle.mpg_rpm = 200.0f,
    .spindle.rpm_min = 0.0f,
    .spindle.rpm_max = 1000.0f,
    .jog_config.step_speed    = 100.0f,
    .jog_config.slow_speed    = 600.0f,
    .jog_config.fast_speed    = 3000.0f,
    .jog_config.step_distance = 0.1f,
    .jog_config.slow_distance = 500.0f,
    .jog_config.fast_distance = 500.0f
};

static grbl_info_t grbl_info = {
    .options = {0},
    .device  = "grblHAL MPG & DRO",
};

static sd_files_t sd_files = {0};

static void processReply (char *line)
{
    grblTransmitCallback(strcmp(line, "ok") == 0, &grbl_data);
}

void setGrblTransmitCallback (void (*fn)(bool ok, grbl_data_t *grbl_data))
{
    grblTransmitCallback = fn;
    grbl_event.on_report_received = fn ? processReply : NULL;
}

void grblSerialFlush (void)
{
    serial_RxCancel();
}

static bool parseDecimal (float *value, char *data)
{
    bool changed;

    float val = (float)atof(data);
    if((changed = val != *value))
        *value = val;

    return changed;
}

static bool parseInt (int32_t *value, char *data)
{
    bool changed;

    int32_t val = (int32_t)atoi(data);
    if((changed = val != *value))
        *value = val;

    return changed;
}

static bool parseUint (uint32_t *value, char *data)
{
    bool changed;

    uint32_t val = (uint32_t)atoi(data);
    if((changed = val != *value))
        *value = val;

    return changed;
}

static void parsePositions (char *data)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.position.x, data))
        grbl_data.changed.xpos = true;

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.position.y, data))
        grbl_data.changed.ypos = true;

    if(parseDecimal(&grbl_data.position.z, next))
        grbl_data.changed.zpos = true;
}

static void parseOffsets (char *data)
{
    if(grbl_data.useWPos) {

        grbl_data.changed.offset = grbl_data.offset.x != 0.0f ||
                                    grbl_data.offset.y != 0.0f ||
                                     grbl_data.offset.z != 0.0f;

        grbl_data.offset.x =
        grbl_data.offset.y =
        grbl_data.offset.z = 0.0f;

    } else {

        char *next;

        next = strchr(data, ',');
        *next++ = '\0';

        if(parseDecimal(&grbl_data.offset.x, data))
            grbl_data.changed.offset = true;

        data = next;
        next = strchr(data, ',');
        *next++ = '\0';

        if(parseDecimal(&grbl_data.offset.y, data))
            grbl_data.changed.offset = true;

        if(parseDecimal(&grbl_data.offset.z, next))
            grbl_data.changed.offset = true;
    }

    grbl_data.changed.await_wco_ok = grbl_data.awaitWCO;
}

static void parseOverrides (char *data)
{
    char *next;
    uint32_t value;

    next = strchr(data, ',');
    *next++ = '\0';

    value = grbl_data.override.feed_rate;
    if(parseUint(&value, data)) {
        grbl_data.changed.feed_override = true;
        grbl_data.override.feed_rate = (override_t)value;
    }

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';

    value = grbl_data.override.rapid_rate;
    if(parseUint(&value, data)) {
        grbl_data.changed.rapid_override = true;
        grbl_data.override.rapid_rate = (override_t)value;
    }

    value = grbl_data.override.spindle_rpm;
    if(parseUint(&value, data)) {
        grbl_data.changed.rpm_override = true;
        grbl_data.override.spindle_rpm = (override_t)value;
    }
}

static void parseFeedSpeed (char *data)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.feed_rate, data))
        grbl_data.changed.feed = true;

    data = next;
    if((next = strchr(data, ',')))
        *next++ = '\0';

    if(parseDecimal(&grbl_data.spindle.rpm_programmed, data))
        grbl_data.changed.rpm = true;

    if(next && parseDecimal(&grbl_data.spindle.rpm_actual, next))
        grbl_data.changed.rpm = true;
}

static void parseData (char *block)
{
    static char buf[MAX_BLOCK_LENGTH];

    uint32_t c;
    bool pins = true;
    char *line = &buf[0];

    if((ack_received = !strcmp(block, "ok"))) {
        grbl_data.changed.await_ack = false;
        grblClearError(); // TODO: grbl needs to be fixed for continuing to process from input buffer after error...
        if(grbl_data.alarm)
            grblClearAlarm();
        return;
    }

    if(block[0] == '<' || block[0] =='[') // strip last character from long messages
        block[strlen(block) - 1] = '\0';

    strncpy(line, block, MAX_BLOCK_LENGTH);
    line[MAX_BLOCK_LENGTH - 1] = '\0';

    if(line[0] == '<') {
        pins = false;
        line = strtok(&line[1], "|");

        if(line) {

            if(grblParseState(line, &grbl_data.grbl)) {

                grbl_data.changed.state = true;

                setLeds(grbl_data.grbl.state);

                if(!(grbl_data.grbl.state == Alarm || grbl_data.grbl.state == Tool) && grbl_data.message[0] != '\0')
                    grblClearMessage();
            }

            if(grbl_data.alarm && grbl_data.grbl.state != Alarm)
                grblClearAlarm();

            line = strtok(NULL, "|");
        }

        while(line) {

            if(!strncmp(line, "WPos:", 5)) {
                if(!grbl_data.useWPos) {
                    grbl_data.useWPos = true;
                    grbl_data.changed.offset = true;
                }
                parsePositions(line + 5);

            } else if(!strncmp(line, "MPos:", 5)) {
                if(grbl_data.useWPos) {
                    grbl_data.useWPos = false;
                    grbl_data.changed.offset = true;
                }
                parsePositions(line + 5);

            } else if(!strncmp(line, "FS:", 3))
                parseFeedSpeed(line + 3);

            else if(!strncmp(line, "WCO:", 4))
                parseOffsets(line + 4);

            else if(!strncmp(line, "Pn:", 3)) {
                pins = true;
                if((grbl_data.changed.pins = (strcmp(grbl_data.pins, line + 3) != 0)));
                    strcpy(grbl_data.pins, line + 3);

            } else if(!strncmp(line, "D:", 2)) {
                grbl_data.xModeDiameter = line[2] == '1';
                grbl_data.changed.xmode = true;

            } else if(!strncmp(line, "A:", 2)) {

                line = &line[2];
                grbl_data.spindle.state.on =
                grbl_data.spindle.state.ccw =
                grbl_data.coolant.flood =
                grbl_data.coolant.mist = false;
                grbl_data.changed.leds = true;

                while((c = *line++)) {
                    switch(c) {

                        case 'M':
                            grbl_data.coolant.mist = true;
                            break;

                        case 'F':
                            grbl_data.coolant.flood = true;
                            break;

                        case 'S':
                            grbl_data.spindle.state.ccw = false;
                            grbl_data.spindle.state.on = true;
                            break;

                        case 'C':
                           grbl_data.spindle.state.ccw = true;
                           grbl_data.spindle.state.on = true;
                           break;
                    }
                }

            } else if(!strncmp(line, "Ov:", 3))
                parseOverrides(line + 3);

            else if(!strcmp(line, "AR") || !strncmp(line, "AR:", 3)) {
                grbl_data.autoReporting = true;
                if(line[2] == ':')
                    grbl_data.changed.auto_reporting = parseUint(&grbl_data.autoReportingInterval, &line[3]);
                else {
                    grbl_data.changed.auto_reporting = grbl_data.autoReportingInterval != 0;
                    grbl_data.autoReportingInterval = 0;
                }
            } else if(!strncmp(line, "MPG:", 4)) {
                if((grbl_data.changed.mpg = grbl_data.mpgMode != (line[4] == '1'))) {
                    grbl_data.mpgMode = !grbl_data.mpgMode;
                    grbl_event.on_line_received = parseData;
                }

            } else if(!strncmp(line, "SD:", 3)) {
                if((grbl_data.changed.message = !!strcmp(grbl_data.message, line + 3)))
                    strncpy(grbl_data.message, line + 3, sizeof(grbl_data.message) - 1);

            } else if(!strncmp(line, "TLR:", 4)) {
                if((grbl_data.changed.tlo_reference = grbl_data.tloReferenced != (line[4] == '1'))) {
                    grbl_data.tloReferenced = !grbl_data.tloReferenced;
                    grbl_data.changed.leds = true;
                    grbl_data.leds.tlo_refd = grbl_data.changed.tlo_reference;
                }
            }

            line = strtok(NULL, "|");
        }

        if(!pins && (grbl_data.changed.pins = (grbl_data.pins[0] != '\0')))
            grbl_data.pins[0] = '\0';

    } else if(line[0] == '[') {
        line++;
        if(!strncmp(line, "GC:", 3)) {

            line = strtok(&line[3], " ");

            while(line) {

                if(!strncmp(line, "F", 1) && parseDecimal(&grbl_data.feed_rate, line + 1))
                    grbl_data.changed.feed = true;

                if(!strncmp(line, "S", 1) && parseDecimal(&grbl_data.spindle.rpm_programmed, line + 1))
                    grbl_data.changed.rpm = true;

                if(!strncmp(line, "G7", 2)) {
                    grbl_data.xModeDiameter = true;
                    grbl_data.changed.xmode = true;
                }

                if(!strncmp(line, "G8", 2)) {
                    grbl_data.xModeDiameter = false;
                    grbl_data.changed.xmode = true;
                }

                if(!strncmp(line, "G90", 3) && !grbl_data.absDistance) {
                    grbl_data.absDistance = true;
                    grbl_data.changed.dist = true;
                }

                if(!strncmp(line, "G91", 3) && grbl_data.absDistance) {
                    grbl_data.absDistance = false;
                    grbl_data.changed.dist = true;
                }

                if(!strncmp(line, "M5", 2)) {
                    if(grbl_data.spindle.state.on)
                        grbl_data.changed.leds = true;
                    grbl_data.spindle.state.on = grbl_data.leds.spindle = false;
                } else if(!strncmp(line, "M3", 2)) {
                    if(!grbl_data.spindle.state.on || grbl_data.spindle.state.ccw)
                        grbl_data.changed.leds = true;
                    grbl_data.spindle.state.on = grbl_data.leds.spindle = true;
                    grbl_data.spindle.state.ccw = false;
                } else if(!strncmp(line, "M4", 2)) {
                    if(!grbl_data.spindle.state.on || !grbl_data.spindle.state.ccw)
                        grbl_data.changed.leds = true;
                    grbl_data.spindle.state.on = grbl_data.leds.spindle = true;
                    grbl_data.spindle.state.ccw = true;
                }

                if(!strncmp(line, "M9", 2)) {
                    if(grbl_data.coolant.mist || grbl_data.coolant.flood)
                        grbl_data.changed.leds = true;
                    grbl_data.coolant.mist = grbl_data.leds.mist = false;
                    grbl_data.coolant.flood = grbl_data.leds.flood = false;
                } else {
                    if(!strncmp(line, "M7", 2)) {
                        if(!grbl_data.coolant.mist)
                            grbl_data.changed.leds = true;
                        grbl_data.coolant.mist = grbl_data.leds.mist = true;
                    }
                    if(!strncmp(line, "M8", 2)) {
                        if(!grbl_data.coolant.flood)
                            grbl_data.changed.leds = true;
                        grbl_data.coolant.flood = grbl_data.leds.flood = true;
                    }
                }
                line = strtok(NULL, " ");
            }
        } else if(!strncmp(line, "MSG:", 4)) {
            grbl_data.changed.message = true;
            strncpy(grbl_data.message, line + 4, 250);
        }
    } else if(!strncmp(line, "error:", 6)) {
        grbl_data.error = (uint8_t)atoi(line + 6);
        grbl_data.changed.error = true;
    } else if(!strncmp(line, "ALARM:", 6)) {
        grbl_data.alarm = (uint8_t)atoi(line + 6);
        grbl_data.changed.alarm = true;
    } else if(!strncmp(line, "Grbl", 4)) {
        grbl_data.changed.reset = true;
        grblClearError();
        grblClearAlarm();
        grblClearMessage();
        grbl_event.on_line_received = parseData;
    }

    if(grbl_event.on_report_received && !grbl_data.changed.await_ack)
        grbl_event.on_report_received(grbl_data.block);
}

void grblClearAlarm (void)
{
    grbl_data.changed.alarm = grbl_data.alarm != 0;
    grbl_data.alarm = 0;
}

void grblClearError (void)
{
    grbl_data.changed.error = grbl_data.error != 0;
    grbl_data.error = 0;
}

void grblClearMessage (void)
{
    grbl_data.changed.message = grbl_data.message[0] != '\0';
    grbl_data.message[0] = '\0';
}

void grblSendSerial (char *line)
{
    serial_writeLn(line);
}

bool grblParseState (char *data, grbl_t *grbl)
{
    bool changed = false;
    uint_fast8_t len, substate = 0;
    char *s = strchr(data, ':');

    if(s) {
        *s++ = '\0';
        substate = atoi(s);
    }

    len = strlen(data);

    if(len < sizeof(grbl->state_text) && strncmp(grbl->state_text, data, len)) {
        uint_fast8_t state = 0;
        while(state < NUMSTATES) {
            if((changed = !strcmp(data, grblState[state]))) {
                grbl->state = (grbl_state_t)state;
                grbl->substate = substate;
                grbl->state_color = grblStateColor[state];
                strcpy(grbl->state_text, data);
                break;
            }
            state++;
        }
    }

    if(!changed && grbl->substate != substate) {
        changed = true;
        grbl->substate = substate;
    }

    return changed;
}

static void parse_info (char *line)
{
    if(!strcmp(line, "ok")) {
        grbl_event.on_line_received = parseData;
        if(grbl_event.on_info_received) {
            grbl_event.on_info_received(&grbl_info);
            grbl_event.on_info_received = NULL;
        }
    } else if(!strncmp(line, "[VER:", 5)) {
        grbl_info.is_loaded = true;
        if((line = strchr(line + 5, ':')))
            line[strlen(line) - 1] = '\0';
        if(line && (++line)[0] != '\0') {
            strncpy(grbl_info.device, line, MAX_STORED_LINE_LENGTH - 1);
            grbl_info.device[MAX_STORED_LINE_LENGTH - 1] = '\0';
        }
    } else if(!strncmp(line, "[NEWOPT:", 8)) {

        line[strlen(line) - 1] = '\0';
        line = strtok(&line[8], ",");

        while(line) {

            if(!strncmp(line, "SD", 2))
                grbl_info.options.sd_card = true;
            else if(!strncmp(line, "TC", 2))
                grbl_info.options.tool_change = true;

            line = strtok(NULL, ",");
        }
    } else
        parseData(line);
}

void grblGetInfo (grbl_info_received_ptr on_info_received)
{
    if(grbl_data.mpgMode && grbl_event.on_line_received == parseData) {
        grbl_event.on_info_received = on_info_received;
        grbl_event.on_line_received = parse_info;
        serial_RxCancel();
        serial_writeLn("$I");
    } else if (on_info_received)
        on_info_received(&grbl_info); // return default values
}

grbl_options_t grblGetOptions (void)
{
    return grbl_info.options;
}

static void parse_settings (char *line)
{
    static uint_fast16_t setting = 0;

    if(!strcmp(line, "ok")) {
        grbl_event.on_line_received = parseData;
        settings.is_loaded = setting > 0;
        if(grbl_event.on_settings_received) {
            grbl_event.on_settings_received(&settings);
            grbl_event.on_settings_received = NULL;
        }
    } else if(line[0] == '$' && strchr(line, '=')) {

        line = strtok(&line[1], "=");
        setting = atoi(line);
        line = strtok(NULL, "=");
        float value = atof(line);

        switch((setting_type_t)setting) {

            case Setting_JogStepSpeed:
                settings.jog_config.step_speed = value;
                break;

            case Setting_JogSlowSpeed:
                settings.jog_config.slow_speed = value;
                break;

            case Setting_JogFastSpeed:
                settings.jog_config.fast_speed = value;
                break;

            case Setting_JogStepDistance:
                settings.jog_config.step_distance = value;
                break;

            case Setting_JogSlowDistance:
                settings.jog_config.slow_distance = value;
                break;

            case Setting_JogFastDistance:
                settings.jog_config.fast_distance = value;
                break;

            case Setting_RpmMin:
                settings.spindle.rpm_min = value;
                break;

            case Setting_RpmMax:
                settings.spindle.rpm_max = value;
                break;

            case Setting_EnableLegacyRTCommands:
                setGrblLegacyMode((uint32_t)value != 0);
                break;

            case Setting_HomingEnable:
                settings.homing_enabled = ((uint32_t)value & 0x01) != 0;
                break;

            case Setting_LaserMode:
                settings.mode = (uint32_t)value;
                break;

            default:
                break;
        }
    } else
        parseData(line);
}

void grblGetSettings (grbl_settings_received_ptr on_settings_received)
{
    if(grbl_data.mpgMode && grbl_event.on_line_received == parseData) {
        grbl_event.on_settings_received = on_settings_received;
        grbl_event.on_line_received = parse_settings;
        serial_RxCancel();
        serial_writeLn("$$");
    } else if (on_settings_received)
        on_settings_received(&settings); // return default values
}

static void parse_sd_files (char *line)
{
    if(!strcmp(line, "ok")) {
        grbl_event.on_line_received = parseData;
        if(grbl_event.on_sd_files_received) {
            grbl_event.on_sd_files_received(&sd_files);
            grbl_event.on_sd_files_received = NULL;
        }
    } else if(!strncmp(line, "[FILE:", 6)) {

        sd_file_t *file, *next = sd_files.files;
        
        if((file = (sd_file_t *)malloc(sizeof(sd_file_t)))) {
        
            char *size;
            
            line += 6;
            if((size = strstr(line, "|SIZE:"))) {
                *size = '\0';
                size += 6;
            }

            strcpy(file->name, line);
            file->next = NULL;

            if(size && ((line = strchr(size, '|')) || (line = strchr(size, ']')))) {
                *line = '\0';
                file->length = (uint32_t)atoi(size);
            } else
                file->length = 0;

            if(sd_files.files == NULL)
                sd_files.files = file;
            else {
                while(next->next)
                    next = next->next;
                next->next= file;    
            }
            sd_files.num_files++;
        }    
    } else
        parseData(line);
}

void grblGetSDFiles (grbl_sd_files_received_ptr on_sd_files_received)
{
    if(grbl_data.mpgMode && grbl_info.options.sd_card && grbl_event.on_line_received == parseData) {
        sd_file_t *file ;
        while(sd_files.files) {
            file = sd_files.files->next;
            free(sd_files.files);
            sd_files.files = file;
        }
        sd_files.num_files = 0;
        grbl_event.on_sd_files_received = on_sd_files_received;
        grbl_event.on_line_received = parse_sd_files;
        serial_RxCancel();
        serial_writeLn("$F");
    } else if (on_settings_received)
        on_sd_files_received(&sd_files); // return default values
}

static void await_ack (char *line)
{
    ack_received = !strcmp(line, "ok");
}

bool grblAwaitACK (const char *command, uint_fast16_t timeout_ms)
{
    ack_received = false;
    grbl_data.changed.await_ack = true;
//    grbl_event.on_line_received = await_ack;
    serial_RxCancel();
    serial_writeLn(command);

    timeout_ms += lcd_systicks();
    while(!ack_received && lcd_systicks() <= timeout_ms)
        grblPollSerial();

    grbl_data.changed.await_ack = false;
//    grbl_event.on_line_received = parseData;

    return ack_received;
}

void grblPollSerial (void)
{
    static int_fast16_t c;
    static uint_fast16_t char_counter = 0;

    while((c = serial_getC()) != SERIAL_NO_DATA) {

        if(c == 0x18) //ASCII_CAN
            char_counter = 0;
        else if(((c == '\n') || (c == '\r'))) { // End of line reached

            grbl_data.block[char_counter] = '\0';
            
            if(char_counter > 0)
                 grbl_event.on_line_received(grbl_data.block);

            char_counter = 0;

        } else if(char_counter < MAX_BLOCK_LENGTH - 1)
            grbl_data.block[char_counter++] = (char)c;
    }
}

__attribute__((weak)) int16_t serial_getC (void) { return SERIAL_NO_DATA; }
__attribute__((weak)) void serial_writeLn (const char *data) {}
__attribute__((weak)) void serial_RxCancel (void);

#endif // PARSER_SERIAL_ENABLE

/* I2C display plugin protocol parser */

#ifdef PARSER_I2C_ENABLE

static char *spins (control_signals_t signals)
{
    static const char pinmap[] = "RHSDLTE FM    P ";
    static char pins[17];

    char *s = pins, *map = (char *)pinmap;

    while(signals.mask) {
        if((signals.mask & 0x01) & *map != ' ')
            *s++ = *map;
        map++;
        signals.mask >>= 1;
    }   
    *s = '\0';

    return pins;
}

void grblPollI2C (void)
{
    uint_fast8_t idx;
    i2c_rxdata_t *i2c_msg; 
    grbl_state_t state;
    machine_status_packet_t *packet;

    if((i2c_msg = i2c_rx_poll()) == NULL)
        return;

    if(i2c_msg->len < offsetof(machine_status_packet_t, msgtype))
        return;

    packet = (machine_status_packet_t *)i2c_msg->data;

    switch(packet->machine_state) {

        case MachineState_Alarm:
            state = Alarm;
            break;

        case MachineState_Cycle:
            state = Run;
            if((grbl_data.changed.message = *grbl_data.message != '\0'))
                *grbl_data.message = '\0';
            break;

        case MachineState_Hold:
            state = Hold;
            break;

        case MachineState_ToolChange:
            state = Tool;
            break;

        case MachineState_Idle:
            state = Idle;
            break;

        case MachineState_Homing:
            state = Home;
            break;

        case MachineState_Jog:
            state = Jog;
            if((grbl_data.changed.message = *grbl_data.message != '\0'))
                *grbl_data.message = '\0';
            break;

        default:
            state = Unknown;
            break;
    }

    if((grbl_data.changed.state = grbl_data.grbl.state != state)) {

        grbl_data.grbl.state = state;
        grbl_data.grbl.substate = packet->machine_substate;
        grbl_data.grbl.state_color = grblStateColor[state];
        strcpy(grbl_data.grbl.state_text, grblState[state]);

        setLeds(grbl_data.grbl.state);
    }

    if(i2c_msg->len >= offsetof(machine_status_packet_t, msg) && packet->msgtype) {

        switch(packet->msgtype) {

            case MachineMsg_ClearMessage:
                if((grbl_data.changed.message = *grbl_data.message != '\0'))
                    *grbl_data.message = '\0';
                break;

            case MachineMsg_WorkOffset:
                grbl_data.changed.offset = true;
                memcpy(&grbl_data.offset, (machine_coords_t *)packet->msg, sizeof(grbl_data.offset));
                break;

            case MachineMsg_Overrides:
                {
                    overrides_t override;
                    memcpy(&override, packet->msg, sizeof(overrides_t)); // RP2040 (ARM M0) hard faults on unaligned read
                    if((grbl_data.changed.feed_override = grbl_data.override.feed_rate != override.feed_rate))
                        grbl_data.override.feed_rate = override.feed_rate;
                    if((grbl_data.changed.rapid_override = grbl_data.override.rapid_rate != override.rapid_rate))
                        grbl_data.override.rapid_rate = override.rapid_rate;
                    if((grbl_data.changed.rpm_override = grbl_data.override.spindle_rpm != override.spindle_rpm))
                        grbl_data.override.spindle_rpm = override.spindle_rpm;
                }
                break;

            default:
                if(packet->msgtype < 128) { // string message
                    grbl_data.changed.message = true;
                    packet->msg[packet->msgtype] = '\0';
                    strcpy(grbl_data.message, (char *)packet->msg);
                } // else unhandled message type
                break;
        }
    }

    idx = grbl_data.n_axis;

    do {
        idx--;
        if(grbl_data.position.values[idx] != packet->coordinate.values[idx]) {
            grbl_data.changed.flags |= 1 << idx;
            grbl_data.position.values[idx] = packet->coordinate.values[idx];
        }
    } while(idx);

    if((grbl_data.changed.feed = grbl_data.feed_rate != packet->feed_rate))
        grbl_data.feed_rate = packet->feed_rate;

    if((grbl_data.changed.error = grbl_data.error != packet->status_code))
        grbl_data.error = packet->status_code;

    if((grbl_data.changed.coolant = grbl_data.coolant.value != packet->coolant_state.value)) {
        grbl_data.changed.leds = true;
        grbl_data.leds.mist = packet->coolant_state.mist;
        grbl_data.leds.flood = packet->coolant_state.flood;
        grbl_data.coolant = packet->coolant_state;
    }

    if((grbl_data.changed.spindle = grbl_data.spindle.state.value != packet->spindle_state.value)) {
        grbl_data.changed.leds = true;
        if(!(grbl_data.leds.spindle = packet->spindle_state.on))
            grbl_data.spindle.rpm_actual = 0.0f;
        grbl_data.spindle.state = packet->spindle_state;
    }

    if((grbl_data.changed.rpm = (grbl_data.spindle.state.on ? grbl_data.spindle.rpm_actual : grbl_data.spindle.rpm_programmed) != (float)packet->spindle_rpm)) {
        if(grbl_data.spindle.state.on)
            grbl_data.spindle.rpm_actual = (float)packet->spindle_rpm;
        else
            grbl_data.spindle.rpm_programmed = (float)packet->spindle_rpm;
    }

    char *pins = spins(packet->signals);
    if((grbl_data.changed.pins = !!strcmp(grbl_data.pins, pins)))
        strcpy(grbl_data.pins, pins);

    if((grbl_data.changed.mpg = grbl_data.mpgMode != packet->machine_modes.mpg))
        grbl_data.mpgMode = packet->machine_modes.mpg;

    if((grbl_data.changed.tlo_reference = grbl_data.tloReferenced != packet->machine_modes.tlo_referenced)) {
        grbl_data.tloReferenced = packet->machine_modes.tlo_referenced;
        grbl_data.changed.leds = true;
        grbl_data.leds.tlo_refd = grbl_data.changed.tlo_reference;
    }

    if((grbl_data.changed.xmode = grbl_data.xModeDiameter != packet->machine_modes.diameter))
        grbl_data.xModeDiameter = packet->machine_modes.diameter;

    if((grbl_data.changed.jog_mode = grbl_data.jog_mode.value != packet->jog_mode.value))
        grbl_data.jog_mode = packet->jog_mode;

    if(grbl_event.on_report_received && grbl_data.changed.flags)
        grbl_event.on_report_received(grbl_data.block);
}

__attribute__((weak)) i2c_rxdata_t *i2c_rx_poll (void) { return NULL; }

#endif // PARSER_I2C_ENABLE
