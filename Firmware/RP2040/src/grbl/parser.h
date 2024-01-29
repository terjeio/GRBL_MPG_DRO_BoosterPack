/*
 * parser.c - collects, parses and dispatches lines of data from
 *            grbls serial output stream and/or i2c display protocol
 *
 * v0.0.6 / 2023-08-11 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2018-2023, Terje Io
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

#include <stddef.h>
#include <stdbool.h>

#include "grbl.h"
#include "i2c_interface.h"
#include "../LCD/colorRGB.h"

#define SERIAL_NO_DATA -1
#define MAX_BLOCK_LENGTH 256

//#define PARSER_SERIAL_ENABLE

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
    float rpm_programmed;
    float rpm_actual;
    spindle_state_t state;
} spindle_data_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t led0 :1,
                led1 :1,
                led2 :1,
                led3 :1,
                led4 :1,
                led5 :1,
                led6 :1,
                led7 :1;
    };
    struct {
        uint8_t mode     :1,
                run      :1,
                hold     :1,
                spindle  :1,
                flood    :1,
                mist     :1,
                tlo_refd :1,
                unused   :1;
    };
} leds_t;

typedef union {
    uint32_t flags;
    struct {
        uint32_t xpos           :1,
                 ypos           :1,
                 zpos           :1,
                 apos           :1,
                 bpos           :1,
                 cpos           :1,
                 upos           :1,
                 vpos           :1,
                 mpg            :1,
                 state          :1,
                 offset         :1,
                 await_ack      :1,
                 await_wco_ok   :1,
                 leds           :1,
                 dist           :1,
                 message        :1,
                 feed           :1,
                 rpm            :1,
                 alarm          :1,
                 error          :1,
                 xmode          :1,
                 coolant        :1,
                 spindle        :1,
                 pins           :1,
                 reset          :1,
                 feed_override  :1,
                 rapid_override :1,
                 rpm_override   :1,
                 jog_mode       :1,
                 tlo_reference  :1,
                 auto_reporting :1,
                 unassigned     :1;
    };
} changes_t;

typedef struct {
    uint8_t n_axis;
    grbl_t grbl;
    machine_coords_t position;
    machine_coords_t offset;
    spindle_data_t spindle;
    coolant_state_t coolant;
    overrides_t override;
    float feed_rate;
    bool useWPos;
    bool awaitWCO;
    bool absDistance;
    bool mpgMode;
    bool xModeDiameter;
    bool tloReferenced;
    bool autoReporting;
    uint32_t autoReportingInterval;
    jog_mode_t jog_mode;
    changes_t changed;
    uint8_t alarm;
    uint8_t error;
    leds_t leds;
    char pins[10];
    char block[MAX_BLOCK_LENGTH];
    char message[MAX_BLOCK_LENGTH];
} grbl_data_t;

typedef void (*grbl_callback_ptr)(char *line);

#ifdef PARSER_SERIAL_ENABLE

typedef union {
    uint8_t flags;
    struct {
        uint32_t sd_card     :1,
                 lathe       :1,
                 tool_change :1,
                 is_loaded   :1,
                 unassigned  :5;
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
} spindle_settings_t;

typedef struct {
    bool is_loaded;
    bool homing_enabled;
    uint8_t mode;
    spindle_settings_t spindle;
    jog_config_t jog_config;
} settings_t;

typedef void (*grbl_settings_received_ptr)(settings_t *settings);
typedef void (*grbl_info_received_ptr)(grbl_info_t *info);
typedef void (*grbl_parser_state_received_ptr)(grbl_data_t *info);
typedef void (*grbl_sd_files_received_ptr)(sd_files_t *files);

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

#ifdef PARSER_I2C_ENABLE

typedef struct {
    size_t len;
    uint8_t data[256];
} i2c_rxdata_t;

void grblPollI2C (void);

extern i2c_rxdata_t *i2c_rx_poll (void);

#endif

grbl_data_t *setGrblReceiveCallback (grbl_callback_ptr fn);

/**/

uint32_t lcd_systicks (void);

#ifdef PARSER_SERIAL_ENABLE

extern int16_t serial_getC (void);
extern void serial_writeLn (const char *data);
extern void serial_RxCancel (void);

#endif

#endif
