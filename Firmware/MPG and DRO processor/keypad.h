/*
 * keypad.c - I2C keypad interface for Texas Instruments Tiva C (TM4C123) processor
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-05 / ©Io Engineering / Terje
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

#ifndef _KEYPAD_H_
#define _KEYPAD_H_

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

typedef union {
    uint8_t value;
    struct {
        uint8_t led1: 1,
                led0: 1,
                led2: 1,
                led3: 1,
                led4: 1,
                led5: 1,
                led6: 1,
                led7: 1;
    };
    struct {
        uint8_t run:     1,
                mode:    1,
                hold:    1,
                spindle: 1,
                flood:   1,
                mist:    1,
                unused6: 1,
                unused7: 1;
    };
} leds_t;

void keypad_setup (void);
void keypad_flush (void);
char keypad_get_keycode (void);
bool keypad_has_keycode (void);
void keypad_leds (leds_t leds);
void keypad_forward (bool on);
jogmode_t keypadJogModeNext (void);
jogmode_t keypadGetJogMode (void);
leds_t keypad_GetLedState (void);
void setJogModeChangedCallback (void (*fn)(jogmode_t jogMode));
void setKeyclickCallback (void (*fn)(bool keydown));
void setKeyclickCallback2 (void (*fn)(bool keydown, char key), bool translate);

#endif
