/*
 * grblcomm.h - collects and dispatches lines of data from grbls serial output stream
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.3 / 2022-01-28 / (c) Io Engineering / Terje
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

#ifndef _GRBLCOMM_H_
#define _GRBLCOMM_H_

#include <stdbool.h>

#include "grbl.h"
#include "interface.h"
#include "LCD/colorRGB.h"

#define NUMSTATES 11

#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2

#define MAX_BLOCK_LENGTH 256

typedef enum {
    Unknown = 0,
    Idle,
    Run,
    Jog,
    Hold,
    Alarm,
    Check,
    Door,
    Tool,
    Home,
    Sleep
} grbl_state_t;

typedef struct {
    grbl_state_t state;
    uint8_t substate;
    RGBColor_t state_color;
    char state_text[8];
} grbl_t;

typedef struct {
    float x;
    float y;
    float z;
} coord_values_t;

typedef struct {
    float rpm_programmed;
    float rpm_actual;
    bool on;
    bool ccw;
    int32_t rpm_override;
} spindle_data_t;

typedef struct {
    bool flood;
    bool mist;
} coolant_data_t;

typedef union {
    uint32_t flags;
    struct {
        uint32_t mpg :1,
                 state :1,
                 xpos :1,
                 ypos :1,
                 zpos :1,
                 offset :1,
                 await_ack: 1,
                 await_wco_ok: 1,
                 leds :1,
                 dist : 1,
                 message: 1,
                 feed: 1,
                 rpm: 1,
                 alarm: 1,
                 error: 1,
                 xmode: 1,
                 pins: 1,
                 reset: 1,
                 feed_override: 1,
                 rapid_override: 1,
                 rpm_override: 1,
                 unassigned: 11;
    };
} changes_t;

typedef union {
    uint8_t flags;
    struct {
        uint32_t sd_card: 1,
                 lathe:   1,
                 tool_change: 1,
                 is_loaded: 1,
                 unassigned: 5;
    };
} grbl_options_t;

typedef struct {
    bool is_loaded;
    grbl_options_t options;
    char device[MAX_STORED_LINE_LENGTH];
} grbl_info_t;

typedef struct sd_file {
    uint32_t length;
    char name[MAX_STORED_LINE_LENGTH];
    struct sd_file *next;
} sd_file_t;

typedef struct {
    uint_fast16_t num_files;
    sd_file_t *files;
} sd_files_t;

typedef struct {
    grbl_t grbl;
    float position[3];
    float offset[3];
    spindle_data_t spindle;
    coolant_data_t coolant;
    int32_t feed_override;
    int32_t rapid_override;
    float feed_rate;
    bool useWPos;
    bool awaitWCO;
    bool absDistance;
    bool mpgMode;
    bool xModeDiameter;
    changes_t changed;
    uint8_t alarm;
    uint8_t error;
    char pins[10];
    char block[MAX_BLOCK_LENGTH];
    char message[MAX_BLOCK_LENGTH];
} grbl_data_t;

typedef struct {
    float fast_speed;
    float slow_speed;
    float step_speed;
    float fast_distance;
    float slow_distance;
    float step_distance;
} jog_config_t;

typedef struct {
    float rpm;
    float mpg_rpm;
    float rpm_min;
    float rpm_max;
} spindle_state_t;

typedef struct {
    bool is_loaded;
    bool homing_enabled;
    uint8_t mode;
    spindle_state_t spindle;
    jog_config_t jog_config;
} settings_t;

typedef void (*grbl_callback_ptr)(char *line);
typedef void (*grbl_settings_received_ptr)(settings_t *settings);
typedef void (*grbl_info_received_ptr)(grbl_info_t *info);
typedef void (*grbl_parser_state_received_ptr)(grbl_data_t *info);
typedef void (*grbl_sd_files_received_ptr)(sd_files_t *files);

grbl_data_t *setGrblReceiveCallback (grbl_callback_ptr fn);
void setGrblTransmitCallback (void (*fn)(bool ok, grbl_data_t *grbl_data));
void grblPollSerial (void);
void grblSerialFlush (void);
void grblClearAlarm (void);
void grblClearError (void);
void grblClearMessage (void);
void grblSendSerial (char *line);
bool grblParseState (char *state, grbl_t *grbl);
bool grblAwaitACK (const char *command, uint_fast16_t timeout_ms);
bool grblIsMPGActive (void);

void grblGetInfo (grbl_info_received_ptr on_info_received);
void grblGetSettings (grbl_settings_received_ptr on_settings_received);
void grblGetParserState (grbl_parser_state_received_ptr on_parser_state_received);
void grblGetSDFiles (grbl_sd_files_received_ptr on_sd_files_received);
grbl_options_t grblGetOptions (void);

void setGrblLegacyMode (bool on);
char mapRTC2Legacy (char c);

#endif
