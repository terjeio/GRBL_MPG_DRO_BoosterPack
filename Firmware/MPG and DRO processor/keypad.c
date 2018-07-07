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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tiva.h"
#include "grbl.h"
#include "keypad.h"
#include "navigator.h"
#include "mpg.h"

typedef enum {
    I2CState_Idle = 0,
    I2CState_SendNext,
    I2CState_SendLast,
    I2CState_AwaitCompletion,
    I2CState_ReceiveNext,
    I2CState_ReceiveNextToLast,
    I2CState_ReceiveLast,
} i2c_state_t;

typedef struct {
    volatile i2c_state_t state;
    uint8_t count;
    uint8_t *data;
    bool getKeycode;
    bool processed;
    uint8_t buffer[8];
} i2c_master_trans_t;

typedef struct {
    bool forward;
    bool claimed;
    uint8_t keycode;
} i2c_slave_trans_t;

static bool keyDown = false;
static jogmode_t jogMode = JogMode_Slow;
static i2c_master_trans_t i2c_m;
static i2c_slave_trans_t i2c_s;
static leds_t leds_state = {
    .value = 255
};

#define KEYBUF_SIZE 16
#define i2cIsBusy (i2cBusy || I2CMasterBusy(I2C1_BASE))
//#define i2cIsBusy ((i2c_m.state != I2CState_Idle) || I2CMasterBusy(I2C1_BASE))

static volatile bool i2cBusy = false;
static bool xlate = false;
static char keybuf_buf[16];
static volatile uint32_t keybuf_head = 0, keybuf_tail = 0;

static void fixkey (bool keydown);
static void master_interrupt_handler (void);
static void slave_interrupt_handler (void);
static void keyclick_int_handler (void);

static void (*keyclickCallback)(bool keydown) = 0;
static void (*keyclickCallback2)(bool keydown, char key) = 0;
static void (*jogModeChangedCallback)(jogmode_t jogMode) = 0;

void master_interrupt_handler (void);

static void enqueue_keycode (char c)
{
    if(c == 'h' && keyclickCallback != fixkey) {
        keypadJogModeNext();
        return;
    }

    if(c == 'q' || c == 'r' || c == 's' || c == 't') // temp until keypad is corrected?
        c += 4;

    if((i2c_s.claimed = i2c_s.forward && !(c == '\r' || c == 'h' || c == CMD_FEED_HOLD || c == CMD_CYCLE_START))) {
        i2c_s.keycode = c;
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);
        return;
    }

    uint32_t bptr = (keybuf_head + 1) & (KEYBUF_SIZE - 1);    // Get next head pointer

    if(bptr != keybuf_tail) {               // If not buffer full
        keybuf_buf[keybuf_head] = c;        // add data to buffer
        keybuf_head = bptr;                 // and update pointer
        i2c_m.processed = true;            //
        if(keyclickCallback) {
            keyclickCallback(true);         // Fire key down event
            if(!keyDown)                    // If not still down then followed by
                keyclickCallback(false);    // key up event
        }
    }
}

void setJogModeChangedCallback (void (*fn)(jogmode_t jogMode))
{
    if((jogModeChangedCallback = fn))
        jogModeChangedCallback(jogMode);
}

void setKeyclickCallback (void (*fn)(bool keydown))
{
    keyclickCallback = fn;
    setMPGCallback(0);
}

// Translate CNC assigned keycodes for UI use
static void fixkey (bool keydown)
{
    static char c = 0;
/*
    static bool keywasdown = false;

    if(keydown) {

        if(keywasdown && != c)
            keyclickCallback2(false, c);

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

        case 'o':
            c = '-';
            break;
    }

    keyclickCallback2(keydown, c);
}

void mpg_handler (mpg_t mpg)
{
    static int32_t pos = 0;
    static char c = '0' - 1;

    int32_t chg = 0;

    if((mpg.z.position & ~0x03) - pos >= 4)
        chg = 1;
    else if((mpg.z.position & ~0x03) - pos <= -4)
        chg = -1;

    if(chg) {

        c += chg;

        if(c < '0')
            c = '9';
        else if(c > '9')
            c = '0';

        pos = mpg.z.position & ~0x03;

        keyclickCallback2(true, c);
    }
}

void setKeyclickCallback2 (void (*fn)(bool keydown, char key), bool translate)
{
    xlate = translate;
    keyclickCallback2 = fn;
    keyclickCallback = fixkey;
    if(translate)
        setMPGCallback(mpg_handler);
}

void keypad_setup (void)
{
	/* Master - reads keypresses from keypad and sets LED status */

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C1);
    SysCtlPeripheralReset(SYSCTL_PERIPH_I2C1);

	GPIOPinConfigure(GPIO_PA6_I2C1SCL);
	GPIOPinConfigure(GPIO_PA7_I2C1SDA);
	GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_OD);

	GPIOPinTypeI2CSCL(GPIO_PORTA_BASE, GPIO_PIN_6);
	GPIOPinTypeI2C(GPIO_PORTA_BASE, GPIO_PIN_7);

	I2CMasterInitExpClk(I2C1_BASE, SysCtlClockGet(), false);
	I2CIntRegister(I2C1_BASE, master_interrupt_handler);

	GPIOPinTypeGPIOInput(KEYINTR_PORT, KEYINTR_PIN);
	GPIOPadConfigSet(KEYINTR_PORT, KEYINTR_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD); // -> WPU

	GPIOIntRegister(KEYINTR_PORT, keyclick_int_handler);
	GPIOIntTypeSet(KEYINTR_PORT, KEYINTR_PIN, GPIO_BOTH_EDGES);
	GPIOIntEnable(KEYINTR_PORT, KEYINTR_PIN);

    i2c_m.count = 0;
    i2c_m.state = I2CState_Idle;

    I2CMasterIntClear(I2C1_BASE);
    I2CMasterIntEnable(I2C1_BASE);

	/* Slave - relays kepresses to grbl processor */

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C3);
    SysCtlPeripheralReset(SYSCTL_PERIPH_I2C3);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

    GPIOPinConfigure(GPIO_PD0_I2C3SCL);
    GPIOPinConfigure(GPIO_PD1_I2C3SDA);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_OD);

    GPIOPinTypeI2CSCL(GPIO_PORTD_BASE, GPIO_PIN_0);
    GPIOPinTypeI2C(GPIO_PORTD_BASE, GPIO_PIN_1);

    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_2);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

    I2CSlaveInit(I2C3_BASE, KEYPAD_I2CADDR);
    I2CIntRegister(I2C3_BASE, slave_interrupt_handler);
    I2CSlaveIntEnable(I2C3_BASE);

    i2c_s.keycode = 0;
    i2c_s.forward = false;
}

// get single byte - via interrupt
static void I2CGetSWKeycode (void)
{
   if(!i2cIsBusy) { // ignore if busy
       i2c_m.processed = false;
       i2c_m.getKeycode = keyclickCallback != NULL;
       i2c_m.data  = i2c_m.buffer;
       i2c_m.count = 1;
       i2c_m.state = I2CState_ReceiveLast;
       I2CMasterSlaveAddrSet(I2C1_BASE, KEYPAD_I2CADDR, true);
       I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
   }
}

static void I2CSend (uint32_t i2cAddr, const uint8_t value)
{
   while(i2cIsBusy);

   i2c_m.buffer[0] = value;
   i2c_m.count     = 1;
   i2c_m.data      = i2c_m.buffer;
   i2c_m.state     = I2CState_AwaitCompletion;

   I2CMasterSlaveAddrSet(I2C1_BASE, i2cAddr, false);
   I2CMasterDataPut(I2C1_BASE, *i2c_m.data);
   I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_SINGLE_SEND);
}

void keypad_flush (void)
{
    keybuf_tail = keybuf_head;
}

bool keypad_has_keycode (void)
{
    return keybuf_tail != keybuf_head;
}

// Returns 0 if no keycode enqueued
char keypad_get_keycode (void)
{
    uint32_t data = 0, bptr = keybuf_tail;

    if(bptr != keybuf_head) {
        data = keybuf_buf[bptr++];               // Get next character, increment tmp pointer
        keybuf_tail = bptr & (KEYBUF_SIZE - 1);  // and update pointer
    }

    return data;
}

void keypad_leds (leds_t leds)
{
    if(leds_state.value != leds.value) {
        leds_state.value = leds.value;
        I2CSend(KEYPAD_I2CADDR, leds.value);
    }
}

leds_t keypad_GetLedState (void)
{
    return leds_state;
}

void keypad_forward (bool on)
{
    i2c_s.forward = on;
    i2c_s.claimed = false;
    i2c_s.keycode = 0;
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);
}

jogmode_t keypadJogModeNext (void) {

    jogMode = jogMode == JogMode_Step ? JogMode_Fast : (jogMode == JogMode_Fast ? JogMode_Slow : JogMode_Step);

    i2c_s.claimed = true;
    i2c_s.keycode = '0' + (char)jogMode;;
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);

    if(jogModeChangedCallback)
        jogModeChangedCallback(jogMode);

    return jogMode;
}

jogmode_t keypadGetJogMode (void) {
    return jogMode;
}

static void slave_interrupt_handler (void)
{
    uint32_t ifg = I2CSlaveIntStatus(I2C3_BASE, false),
             req = I2CSlaveStatus(I2C3_BASE);

    I2CSlaveIntClear(I2C3_BASE);

    if(ifg == I2C_SLAVE_INT_DATA && req == I2C_SLAVE_ACT_TREQ) {
        I2CSlaveDataPut(I2C3_BASE, i2c_s.keycode);
        i2c_s.keycode = 0;
        if(!keyDown) {
            i2c_s.claimed = false;
            GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);
        }
    }
}

static void master_interrupt_handler (void)
{
    // based on code from https://e2e.ti.com/support/microcontrollers/tiva_arm/f/908/t/169882

    I2CMasterIntClear(I2C1_BASE);

//    if(I2CMasterErr(I2C1_BASE) == I2C_MASTER_ERR_NONE)

    switch(i2c_m.state) {

        case I2CState_Idle:
            break;

        case I2CState_SendNext:
            I2CMasterDataPut(I2C1_BASE, *i2c_m.data++);
            if(--i2c_m.count == 1)
                i2c_m.state = I2CState_SendLast;

            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
            break;

        case I2CState_SendLast:
            I2CMasterDataPut(I2C1_BASE, *i2c_m.data);
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);

            i2c_m.state = I2CState_AwaitCompletion;
            break;

        case I2CState_AwaitCompletion:
            i2c_m.count = 0;
            i2c_m.state = I2CState_Idle;
            break;

        case I2CState_ReceiveNext:
            *i2c_m.data++ = I2CMasterDataGet(I2C1_BASE);
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_RECEIVE_CONT);

            if(--i2c_m.count == 2)
                i2c_m.state = I2CState_ReceiveNextToLast;
            break;

        case I2CState_ReceiveNextToLast:
            *i2c_m.data++ = I2CMasterDataGet(I2C1_BASE);
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);

            i2c_m.count--;
            i2c_m.state = I2CState_ReceiveLast;
            break;

        case I2CState_ReceiveLast:
            *i2c_m.data = I2CMasterDataGet(I2C1_BASE);
            i2c_m.count = 0;
            i2c_m.state = I2CState_Idle;

            if(i2c_m.getKeycode && *i2c_m.data != 0) {
                //  if(GPIOIntStatus(KEYINTR_PORT, KEYINTR_PIN) != 0) { // only add keycode when key is still pressed
                enqueue_keycode(*i2c_m.data);
                i2c_m.getKeycode = false;
            }
            break;
    }
}

static void keyclick_int_handler (void)
{
	uint32_t iflags = GPIOIntStatus(KEYINTR_PORT, KEYINTR_PIN);

	if(iflags & KEYINTR_PIN) {
        if(GPIOPinRead(KEYINTR_PORT, KEYINTR_PIN) != 0) {
            keyDown = true;
            I2CGetSWKeycode();
        } else {
            if(i2c_s.claimed) {
                i2c_s.claimed = false;
                GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);
            } else if(keyclickCallback)
                keyclickCallback(false); // fire key up event
            keyDown = false;
        }
	} else
	    navigator_sw_int_handler();

	GPIOIntClear(KEYINTR_PORT, iflags);
}
