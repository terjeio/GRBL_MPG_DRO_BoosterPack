/*
 * interface.h - driver interface
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2022-01-03 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2021-2022, Terje Io
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "keypad.h"
#include "UILib/uilib.h"

typedef union {
    uint_fast8_t pins;
    struct {
        uint_fast8_t a :1,
                     b :1;
    };
} qei_state_t;

typedef struct {
 //   encoder_t encoder;
    int32_t count;
    int32_t vel_count;
    uint_fast16_t state;
    volatile uint32_t dbl_click_timeout;
    volatile uint32_t vel_timeout;
    uint32_t vel_timestamp;
} qei_t;

typedef struct {
    int32_t position;
    uint32_t velocity;
} mpg_axis_t;

typedef struct {
    mpg_axis_t x, y, z;
} mpg_t;

typedef void (*on_keyclick_ptr)(bool keydown, char key);
typedef bool (*on_serial_block_ptr)(void);
typedef void (*on_jogModeChanged_ptr)(jogmode_t jogMode);
typedef void (*on_mpgChanged_ptr)(mpg_t mpg);

typedef struct interface
{
    on_keyclick_ptr on_keyclick;
    on_keyclick_ptr on_keyclick2;
    on_serial_block_ptr on_serial_block;
    on_jogModeChanged_ptr on_jogModeChanged;
    on_mpgChanged_ptr on_mpgChanged;
    on_navigator_event_ptr on_navigator_event;
} interface_t;

extern interface_t interface;

extern void hal_init (void);

extern void serial_init (void);
extern int16_t serial_getC (void);
extern bool serial_putC (const char c);
extern void serial_writeLn (const char *data);
extern bool keypad_isKeydown (void);
extern void keypad_setFwd (bool on);
extern void signal_setFeedHold (bool on);
extern void signal_setCycleStart (bool on);
extern void signal_setMPGMode (bool on);
extern bool signal_getMPGMode (void);
extern void signal_setLimitsOverride (bool on);
extern bool signal_getSpindleDir (void);
extern void leds_setState (leds_t leds);
extern leds_t leds_getState (void);
extern void navigator_setLimits (int16_t min, int16_t max);
extern void mpg_setActiveAxis (uint_fast8_t axis);
extern mpg_t *mpg_getPosition (void);
extern void mpg_reset (void);
extern void mpg_setCallback (on_mpgChanged_ptr fn);
extern uint32_t rpm_get (void);
