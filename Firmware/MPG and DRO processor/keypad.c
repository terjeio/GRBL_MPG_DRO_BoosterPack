/*
 * keypad.c - I2C keypad interface for Texas Instruments Tiva C (TM4C123) processor
 *
 * part MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 (alpha) / 2018-05-07
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
#include "keypad.h"

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
    uint8_t buffer[8];
} i2c_trans_t;

static i2c_trans_t i2c;

#define KEYBUF_SIZE 16
#define i2cIsBusy (i2cBusy || I2CMasterBusy(I2C1_BASE))
//#define i2cIsBusy ((i2c.state != I2CState_Idle) || I2CMasterBusy(I2C1_BASE))

static volatile bool i2cBusy = false;
static char keybuf_buf[16];
static volatile uint32_t keybuf_head = 0, keybuf_tail = 0;

static void I2C_interrupt_handler (void);
static void keyclick_int_handler (void);

static void (*keyclickCallback)(bool keydown) = 0;

void I2C_interrupt_handler (void);

void setKeyclickCallback (void (*fn)(bool keydown))
{
    keyclickCallback = fn;
}

void keypad_setup (void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C1);
	SysCtlPeripheralReset(SYSCTL_PERIPH_I2C1);
//	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);

	GPIOPinConfigure(GPIO_PA6_I2C1SCL);
	GPIOPinConfigure(GPIO_PA7_I2C1SDA);
	GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_OD);

	GPIOPinTypeI2CSCL(GPIO_PORTA_BASE, GPIO_PIN_6);
	GPIOPinTypeI2C(GPIO_PORTA_BASE, GPIO_PIN_7);

	I2CMasterInitExpClk(I2C1_BASE, SysCtlClockGet(), false);
	I2CIntRegister(I2C1_BASE, I2C_interrupt_handler);

	GPIOPinTypeGPIOInput(KEYINTR_PORT, KEYINTR_PIN);
	GPIOPadConfigSet(KEYINTR_PORT, KEYINTR_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD); // -> WPU

	GPIOIntRegister(KEYINTR_PORT, keyclick_int_handler);
	GPIOIntTypeSet(KEYINTR_PORT, KEYINTR_PIN, GPIO_BOTH_EDGES);
	GPIOIntEnable(KEYINTR_PORT, KEYINTR_PIN);

    i2c.count = 0;
    i2c.state = I2CState_Idle;

    I2CMasterIntClear(I2C1_BASE);
    I2CMasterIntEnable(I2C1_BASE);
}

static void enqueue_keycode (char cmd)
{
    uint32_t bptr = (keybuf_head + 1) & (KEYBUF_SIZE - 1);    // Get next head pointer

    if(bptr != keybuf_tail) {                       // If not buffer full
        keybuf_buf[keybuf_head] = cmd;              // add data to buffer
        keybuf_head = bptr;                         // and update pointer
    }
}

void keypad_flush (void)
{
    keybuf_tail = keybuf_head;
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

// get single byte - via interrupt
void I2CGetSWKeycode (void)
{
   if(!i2cIsBusy) { // ignore if busy
       i2c.getKeycode = keyclickCallback != 0;
       i2c.data  = i2c.buffer;
       i2c.count = 1;
       i2c.state = I2CState_ReceiveLast;
       I2CMasterSlaveAddrSet(I2C1_BASE, KEYPAD_I2CADDR, true);
       I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
   }
}

void I2CSend (uint32_t i2cAddr, const uint8_t value)
{
   while(i2cIsBusy);

   i2c.buffer[0] = value;
   i2c.count     = 1;
   i2c.data      = i2c.buffer;
   i2c.state     = I2CState_AwaitCompletion;

   I2CMasterSlaveAddrSet(I2C1_BASE, i2cAddr, false);
   I2CMasterDataPut(I2C1_BASE, *i2c.data);
   I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_SINGLE_SEND);
}

void I2C_interrupt_handler (void)
{
    // based on code from https://e2e.ti.com/support/microcontrollers/tiva_arm/f/908/t/169882

    I2CMasterIntClear(I2C1_BASE);

//    if(I2CMasterErr(I2C1_BASE) == I2C_MASTER_ERR_NONE)


    switch(i2c.state)
    {

        case I2CState_Idle:
            break;

        case I2CState_SendNext:
        {
            I2CMasterDataPut(I2C1_BASE, *i2c.data++);
            if(--i2c.count == 1)
                i2c.state = I2CState_SendLast;

            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
            break;
        }

        case I2CState_SendLast:
        {
            I2CMasterDataPut(I2C1_BASE, *i2c.data);
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);

            i2c.state = I2CState_AwaitCompletion;
            break;
        }

        case I2CState_AwaitCompletion:
        {
            i2c.count = 0;
            i2c.state = I2CState_Idle;
            break;
        }

        case I2CState_ReceiveNext:
        {

            *i2c.data++ = I2CMasterDataGet(I2C1_BASE);
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_RECEIVE_CONT);

            if(--i2c.count == 2)
                i2c.state = I2CState_ReceiveNextToLast;
            break;
        }

        case I2CState_ReceiveNextToLast:
        {
            *i2c.data++ = I2CMasterDataGet(I2C1_BASE);
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);

            i2c.count--;
            i2c.state = I2CState_ReceiveLast;
            break;
        }

        case I2CState_ReceiveLast:
        {
            *i2c.data = I2CMasterDataGet(I2C1_BASE);
            i2c.count = 0;
            i2c.state = I2CState_Idle;

            if(i2c.getKeycode) {
                //  if(GPIOIntStatus(KEYINTR_PORT, KEYINTR_PIN) != 0) { // only add keycode when key is still pressed
                enqueue_keycode(*i2c.data);
                if(keyclickCallback)
                    keyclickCallback(true); // fire key down event
                i2c.getKeycode = false;
            }
            break;
        }
    }
}

static void keyclick_int_handler (void)
{
	uint32_t iflags = GPIOIntStatus(KEYINTR_PORT, KEYINTR_PIN);

    GPIOIntClear(KEYINTR_PORT, iflags);

	if(iflags & KEYINTR_PIN) {
        if(GPIOPinRead(KEYINTR_PORT, KEYINTR_PIN) != 0)
            I2CGetSWKeycode();
        else if(keyclickCallback)
            keyclickCallback(false); // fire key up event
	}
}
