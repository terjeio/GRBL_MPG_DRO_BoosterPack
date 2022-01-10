/*
 * i2c_nb.c - HAL non-blocking I2C driver for Raspberry RP2040 ARM processor
 *
 * Part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2022-01-05 / (c) Io Engineering / Terje
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

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"

#include "i2c_nb.h"
#include "driver.h"

#define i2cIsBusy (i2cBusy || false)
//#define i2cIsBusy ((i2c_m.state != I2CState_Idle) || I2CMasterBusy(I2C1_BASE))

#define I2C_PORT(port) I2Cn(port)
#define I2Cn(port) i2c ## port
#define I2C_IRQ(port) I2Ci(port)
#define I2Ci(port) I2C ## port ## _IRQ

#define I2C_MASTER I2C_PORT(MASTER_I2C_PORT)
#define I2C_SLAVE I2C_PORT(SLAVE_I2C_PORT)
#define I2C_SLAVE_IRQ I2C_IRQ(SLAVE_I2C_PORT)

//#define SLAVE ((i2c_hw_t *)SLAVE_I2C_PORT)

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
static i2c_hw_t *slave;
static i2c_master_trans_t i2c_m;

static void slave_interrupt_handler (void);

void i2c_nb_init (void)
{
    i2c_init(I2C_MASTER, 100 * 1000);
    gpio_set_function(MASTER_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MASTER_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MASTER_SDA_PIN);
    gpio_pull_up(MASTER_SCL_PIN);

    i2c_init(I2C_SLAVE, 100 * 1000);
    i2c_set_slave_mode(I2C_SLAVE, true, KEYPAD_I2CADDR);
    gpio_set_function(SLAVE_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SLAVE_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SLAVE_SCL_PIN);
    gpio_pull_up(SLAVE_SCL_PIN);

    slave = I2C_SLAVE->hw;
    slave->intr_mask = (I2C_IC_INTR_MASK_M_RD_REQ_BITS);
    irq_set_exclusive_handler(I2C_SLAVE_IRQ, slave_interrupt_handler);
    irq_set_enabled(I2C_SLAVE_IRQ, true);
}

// get single byte - via interrupt
void i2c_getSWKeycode (on_keyclick_ptr callback)
{
    uint8_t c;

   if(!i2cIsBusy) { // ignore if busy
       i2c_m.processed = false;
       i2c_m.on_keyclick = callback;
       i2c_m.data  = i2c_m.buffer;
       i2c_m.count = 1;
       i2c_m.state = I2CState_ReceiveLast;

       if(i2c_read_blocking(I2C_MASTER, KEYPAD_I2CADDR, &c, 1, false) == 1)
            i2c_m.on_keyclick(true, c);
   }
}

void i2c_nb_send (uint32_t i2cAddr, const uint8_t value)
{
    while(i2cIsBusy);

    i2c_write_blocking(I2C_MASTER, i2cAddr, &value, 1, false);
}

void i2c_nb_send_n (uint32_t i2cAddr, const uint8_t *data, uint32_t bytes)
{
    while(i2cIsBusy);

    i2c_write_blocking(I2C_MASTER, i2cAddr, data, bytes, false);
}

static void slave_interrupt_handler (void)
{
    uint32_t iflags = slave->intr_stat;

    if (iflags & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {

        keyevent_t *keypress = keypad_get_forward_key();

        slave->data_cmd = keypress->keycode;

        if(keypress->claimed) {
            keypress->keycode = 0;
            if(!keypad_isKeydown()) {
                keypress->claimed = false;
                keypad_setFwd(false);
            }
        } else
            keypad_setFwd(false);
    }

    slave->clr_rd_req;

  /*  

    if(ifg == I2C_SLAVE_INT_STOP) {
        if!(i2c_s.claimed && keyforward.tail != keyforward.head)
            GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, 0);
    }*/
}

static void master_interrupt_handler (void)
{
    // based on code from https://e2e.ti.com/support/microcontrollers/tiva_arm/f/908/t/169882

  //  I2CMasterIntClear(I2C1_BASE);

//    if(I2CMasterErr(I2C1_BASE) == I2C_MASTER_ERR_NONE)
/*
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
    } */
}
