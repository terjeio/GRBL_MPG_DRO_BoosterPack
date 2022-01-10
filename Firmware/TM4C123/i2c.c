/*
 * hal/i2c.c - MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2023-01-03 / (c) Io Engineering / Terje
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

#include "driver.h"

#include "i2c.h"
#include "src/interface.h"

#define i2cIsBusy ((i2c_m.state != I2CState_Idle) || I2CMasterBusy(I2C1_BASE))

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
    on_keyclick_ptr on_keyclick;
    uint8_t buffer[8];
} i2c_master_trans_t;

static volatile bool i2cBusy = false;
static i2c_master_trans_t i2c_m;

static void slave_interrupt_handler (void);
static void master_interrupt_handler (void);

void i2c_nb_init (void)
{
    /* Master - reads keypresses from keypad and sets LED status */

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C1);
    SysCtlPeripheralReset(SYSCTL_PERIPH_I2C1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    GPIOPinConfigure(GPIO_PA6_I2C1SCL);
    GPIOPinConfigure(GPIO_PA7_I2C1SDA);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_7, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_OD);

    GPIOPinTypeI2CSCL(GPIO_PORTA_BASE, GPIO_PIN_6);
    GPIOPinTypeI2C(GPIO_PORTA_BASE, GPIO_PIN_7);

    I2CMasterInitExpClk(I2C1_BASE, SysCtlClockGet(), false);
    I2CIntRegister(I2C1_BASE, master_interrupt_handler);

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
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_STRENGTH_6MA, GPIO_PIN_TYPE_OD);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, GPIO_PIN_2);

    I2CSlaveInit(I2C3_BASE, KEYPAD_I2CADDR);
    I2CIntRegister(I2C3_BASE, slave_interrupt_handler);
    I2CSlaveIntEnable(I2C3_BASE);
}

// get single byte - via interrupt
void i2c_getSWKeycode (on_keyclick_ptr callback)
{
    if(!i2cIsBusy) { // ignore if busy
        i2c_m.processed = false;
        i2c_m.getKeycode = !!callback;
        i2c_m.on_keyclick = callback;
        i2c_m.data  = i2c_m.buffer;
        i2c_m.count = 1;
        i2c_m.state = I2CState_ReceiveLast;
        I2CMasterSlaveAddrSet(I2C1_BASE, KEYPAD_I2CADDR, true);
        I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
    }
}

void i2c_nb_send (uint32_t i2cAddr, const uint8_t value)
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

static void slave_interrupt_handler (void)
{
    uint32_t ifg = I2CSlaveIntStatus(I2C3_BASE, false),
             req = I2CSlaveStatus(I2C3_BASE);

    I2CSlaveIntClear(I2C3_BASE);

    if(ifg == I2C_SLAVE_INT_DATA && req == I2C_SLAVE_ACT_TREQ) {

        keyevent_t *keypress = keypad_get_forward_key();

        I2CSlaveDataPut(I2C3_BASE, keypress->keycode);

        if(keypress->claimed) {
            keypress->keycode = 0;
            if(!keypad_isKeydown()) {
                keypress->claimed = false;
                keypad_setFwd(false);
            }
        } else
            keypad_setFwd(false);
    }

    if(ifg == I2C_SLAVE_INT_STOP) {
//        if(!(i2c_s.claimed && keyforward.tail != keyforward.head))
//            GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);
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
                i2c_m.on_keyclick(true, *i2c_m.data);
                i2c_m.getKeycode = false;
            }
            break;
    }
}
