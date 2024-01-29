/*
 * keypad.c - I2C keypad handler
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.6 / 2022-01-30 / (c) Io Engineering / Terje
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

#include "config.h"
#include "keypad.h"
#include "interface.h"
#include "grbl/grbl.h"

#define KEYBUF_SIZE 16

typedef struct {
    char key[KEYBUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} keybuffer_t;

static jogmode_t jogMode = JogMode_Slow;
static keyevent_t keyevent;

static bool xlate = false, jogging = false;
static keybuffer_t keybuf = {0};
static keybuffer_t keyforward = {0};
static mpg_t mpg_prev;

static void fixkey (bool keydown, char key);

static bool passthrough (char *c)
{
    bool passtrough = false;

    switch(*c) {
        case '\r':
        case 'h':
        case CMD_FEED_HOLD_LEGACY:
        case CMD_CYCLE_START_LEGACY:
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
            passtrough = true;
            break;

        case 'I':
            *c = CMD_FEED_OVR_RESET;
            passtrough = true;
            break;

        case 'i':
            *c = CMD_OVERRIDE_FEED_COARSE_PLUS;
            passtrough = true;
            break;

        case 'j':
            *c = CMD_OVERRIDE_FEED_COARSE_MINUS;
            passtrough = true;
            break;

        case 'K':
            *c = CMD_OVERRIDE_SPINDLE_RESET;
            passtrough = true;
            break;

         case 'k':
            *c = CMD_OVERRIDE_SPINDLE_COARSE_PLUS;
            passtrough = true;
            break;

        case 'z':
            *c = CMD_OVERRIDE_SPINDLE_COARSE_MINUS;
            passtrough = true;
            break;

        default:
            break;
    }   

    return passtrough;
}

static bool is_jog_command (char c)
{   
    bool jog = true;

    switch(c) {

        case JOG_XR:
        case JOG_XL:
        case JOG_YF:
        case JOG_YB:
        case JOG_ZU:
        case JOG_ZD:
        case JOG_XRYF:
        case JOG_XRYB:
        case JOG_XLYF:
        case JOG_XLYB:
        case JOG_XRZU:
        case JOG_XRZD:
        case JOG_XLZU:
        case JOG_XLZD:
             break;

        default:
             jog = false;
             break;
    }

    return jog;
}

void keypad_enqueue_keycode (bool down, char c)
{
    if(c == 'h' && interface.on_keyclick2 != fixkey) {
        keypadJogModeNext();
        return;
    }

#if UART_MODE
    if(c == '\r') {
        serial_putC(CMD_MPG_MODE_TOGGLE);
        return;
    }
    keyevent.forward_uart = keyevent.forward;
#endif

// Lathe mode workaround
//    if(c == 'q' || c == 'r' || c == 's' || c == 't') // temp until keypad is corrected?
//        c += 4;

    if((keyevent.claimed = keyevent.forward_uart || (keyevent.forward && !passthrough(&c)))) {
        if(keyevent.forward_uart) {
            jogging = is_jog_command(c);
            serial_putC(c);
        } else {
            keyevent.keycode = c;
            keypad_setFwd(true);
        }
        return;
    }

    uint32_t bptr = (keybuf.head + 1) & (KEYBUF_SIZE - 1);    // Get next head pointer

    if(bptr != keybuf.tail) {                       // If not buffer full
        keybuf.key[keybuf.head] = c;                // add data to buffer
        keybuf.head = bptr;                         // and update pointer
 //       i2c_m.processed = true;                   //
        if(interface.on_keyclick2) {
            interface.on_keyclick2(true, c);        // Fire key down event
            if(!keypad_isKeydown())                 // If not still down then followed by
                interface.on_keyclick2(false, c);   // key up event
        }
    }
}

void setJogModeChangedCallback (on_jogModeChanged_ptr fn)
{
    if((interface.on_jogModeChanged = fn))
        interface.on_jogModeChanged(jogMode);
}

void setKeyclickCallback2 (on_keyclick_ptr fn, bool translate)
{
    interface.on_keyclick2 = fn;
    mpg_setCallback(NULL);
}

// Translate CNC assigned keycodes for UI use
static void fixkey (bool keydown, char key)
{
    static char c = 0;
/*
    static bool keywasdown = false;

    if(keydown) {

        if(keywasdown && != c)
            keyclickCallback(false, c);

        c = keypad_get_keycode();
    }

    keywasdown = keydown;
*/

    if(keydown)
        c = keypad_get_keycode();

    if(xlate) switch(c) {

        case 'e':
            c = '.';
            break;

        case 'h':
            c = 0x7F; // DEL
            break;

        case 'R':
            c = 0x0A; // LF
            break;

        case 'L':
            c = 0x0B; // VT
            break;

        case 'U':
            c = 0x09; // HT
            break;

        case 'D':
            c = 0x08; // BS
            break;

        case 'm':
            c = 128;
            break;

        case 'a':
            c = 129;
            break;

        case 'o':
            c = '-';
            break;
    }

    if(c) {
        interface.on_keyclick(keypad_isKeydown(), c);
        if(!keypad_isKeydown())
            c = 0;
    }
}

// Handle MPG events for data entry
void mpg_handler (mpg_t mpg)
{
    static char c = '0' - 1;

    int32_t chg = 0;

    // Single number increment/decrement for Z-axis MPG
    if((mpg.z.position & ~0x03) - mpg_prev.z.position >= 4)
        chg = 1;
    else if((mpg.z.position & ~0x03) - mpg_prev.z.position <= -4)
        chg = -1;

    if(chg) {

        c += chg;

        if(c < '0')
            c = '9';
        else if(c > '9')
            c = '0';

        mpg_prev.z.position = mpg.z.position & ~0x03;
        interface.on_keyclick(true, c);
    }

    // Value spin for X-axis spin
    if((mpg.x.position & ~0x03) - mpg_prev.x.position >= 4)
        chg = 1;
    else if((mpg.x.position & ~0x03) - mpg_prev.x.position <= -4)
        chg = -1;
    else
        chg = 0;

    if(chg) {
        mpg_prev.x.position = mpg.x.position & ~0x03;
        interface.on_keyclick(true, chg > 0 ? 129 : 128);
    }
}

void setKeyclickCallback (on_keyclick_ptr fn, bool translate)
{
    xlate = translate;
    interface.on_keyclick = fn;
    interface.on_keyclick2 = fixkey;
    if(translate) {
        mpg_setCallback(mpg_handler);
        mpg_t *mpg = mpg_getPosition();
        memcpy(&mpg_prev, mpg, sizeof(mpg_t));
    }
}

void keypad_setup (void)
{
    keyevent.keycode = 0;
  //  keyevent.forward = GPIOPinRead(KEYINTR_PORT, KEYINTR_PIN);
    keyevent.forward = true;
}

void keypad_flush (void)
{
    keybuf.tail = keybuf.head;
}

bool keypad_has_keycode (void)
{
    return keybuf.tail != keybuf.head;
}

// Returns 0 if no keycode enqueued
char keypad_get_keycode (void)
{
    uint32_t data = 0, bptr = keybuf.tail;

    if(bptr != keybuf.head) {
        data = keybuf.key[bptr++];               // Get next character, increment tmp pointer
        keybuf.tail = bptr & (KEYBUF_SIZE - 1);  // and update pointer
    }

    return data;
}

void keypad_forward (bool on)
{
    keyevent.forward = on;
    keyevent.claimed = false;
    keyevent.keycode = 0;
    keypad_setFwd(false);
}

bool keypad_forward_queue_is_empty (void)
{
    return keyforward.tail == keyforward.head;
}

void keypad_forward_keypress (char c)
{
    if(keyevent.forward) {

        while(keyevent.keycode); // block until previous trans done

        uint32_t bptr = (keyforward.head + 1) & (KEYBUF_SIZE - 1);    // Get next head pointer

        if(bptr != keyforward.tail) {               // If not buffer full
            keyforward.key[keyforward.head] = c;    // add data to buffer
            keyforward.head = bptr;                 // and update pointer
        }

        if(!keyevent.claimed)
            keypad_setFwd(true);
    }
}

// Returns 0 if no keycode enqueued
keyevent_t *keypad_get_forward_key (void)
{
    if(!keyevent.claimed) {
        uint32_t bptr = keyforward.tail;
        if(bptr != keyforward.head) {
            keyevent.keycode = keyforward.key[bptr++];     // Get next character, increment tmp pointer
            keyforward.tail = bptr & (KEYBUF_SIZE - 1); // and update pointer
        } else
            keyevent.keycode = 0;
    }

    return &keyevent;
}

bool keypad_release2 (void)
{
    bool was_claimed;

    if((was_claimed = keyevent.claimed)) {
        keyevent.keycode = 0;
        if(!keypad_isKeydown()) {
            keyevent.claimed = false;
            keypad_setFwd(false);
        }
    }

    return was_claimed;
}

bool keypad_release (void)
{
    bool was_claimed;

    if((was_claimed = keyevent.claimed)) {
        keyevent.claimed = false;
        if(keyevent.forward_uart && jogging) {
            jogging = false;
            serial_putC(CMD_JOG_CANCEL);
        }
    }

    return was_claimed;
}

jogmode_t keypadJogModeNext (void) {

    jogMode = jogMode == JogMode_Step ? JogMode_Fast : (jogMode == JogMode_Fast ? JogMode_Slow : JogMode_Step);

#if UART_MODE
    if(keyevent.forward && !grblIsMPGActive())
        serial_putC('0' + (char)jogMode);
#else
    keyevent.claimed = true;
    keyevent.keycode = '0' + (char)jogMode;
    keypad_setFwd(true);
#endif

    if(interface.on_jogModeChanged)
        interface.on_jogModeChanged(jogMode);

    return jogMode;
}

jogmode_t keypadGetJogMode (void)
{
    return jogMode;
}
