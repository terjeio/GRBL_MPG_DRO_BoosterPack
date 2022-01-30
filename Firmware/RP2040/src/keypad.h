/*
 * keypad.h - I2C keypad handler
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.5 / 2022-01-28 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2018-20212, Terje Io
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

#ifndef _KEYPAD_H_
#define _KEYPAD_H_

#include <stdint.h>
#include <stdbool.h>

//#include "i2c_nb.h"

#define KEYBUF_SIZE 16
#define KEYPAD_I2CADDR 0x49

#define JOG_XR   'R'
#define JOG_XL   'L'
#define JOG_YF   'F'
#define JOG_YB   'B'
#define JOG_ZU   'U'
#define JOG_ZD   'D'
#define JOG_XRYF 'r'
#define JOG_XRYB 'q'
#define JOG_XLYF 's'
#define JOG_XLYB 't'
#define JOG_XRZU 'w'
#define JOG_XRZD 'v'
#define JOG_XLZU 'u'
#define JOG_XLZD 'x'

typedef enum {
    JogMode_Fast = 0,
    JogMode_Slow,
    JogMode_Step
} jogmode_t;


typedef struct {
    bool forward;
    bool forward_uart;
    bool claimed;
    uint8_t keycode;
} keyevent_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t led0: 1,
                led1: 1,
                led2: 1,
                led3: 1,
                led4: 1,
                led5: 1,
                led6: 1,
                led7: 1;
    };
    struct {
        uint8_t mode:    1,
                run:     1,
                hold:    1,
                spindle: 1,
                flood:   1,
                mist:    1,
                unused6: 1,
                unused7: 1;
    };
} leds_t;

#pragma pack(push, 1)

typedef uint8_t keytype_t;

enum keytype_t {
    Keytype_None = 0,
    Keytype_AutoRepeat,
    Keytype_SingleEvent,
    Keytype_LongPress,
    Keytype_Persistent
};

typedef struct {
    char key;
    keytype_t type;
    uint16_t scanCode;
} keypad_key_t;

#pragma pack(pop)

void keypad_setup (void);
void keypad_flush (void);
char keypad_get_keycode (void);
bool keypad_has_keycode (void);
void keypad_forward (bool on);
void keypad_forward_keypress(char c);
jogmode_t keypadJogModeNext (void);
jogmode_t keypadGetJogMode (void);
void setJogModeChangedCallback (void (*fn)(jogmode_t jogMode));
void setKeyclickCallback (void (*fn)(bool keydown, char key), bool translate);
void setKeyclickCallback2 (void (*fn)(bool keydown, char key), bool translate);
void keypad_enqueue_keycode (bool down, char c);
bool keypad_forward_queue_is_empty (void);
bool keypad_release (void);
bool keypad_release2 (void);
keyevent_t *keypad_get_forward_key (void);

#endif
