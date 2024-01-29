/*
 * i2c_nb.c - HAL non-blocking I2C driver for Raspberry RP2040 ARM processor
 *
 * Part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.3 / 2023-02-26 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2021-2023, Terje Io
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
#include "pico/i2c_slave.h"

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

static void i2c_slave_handler (i2c_inst_t *i2c, i2c_slave_event_t event);

void i2c_nb_init (void)
{
    i2c_init(I2C_MASTER, 100 * 1000);
    gpio_set_function(MASTER_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MASTER_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MASTER_SDA_PIN);
    gpio_pull_up(MASTER_SCL_PIN);

    gpio_set_function(SLAVE_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SLAVE_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SLAVE_SDA_PIN);
    gpio_pull_up(SLAVE_SCL_PIN);

    i2c_slave_init(I2C_SLAVE, KEYPAD_I2CADDR, i2c_slave_handler);
}

#ifdef PARSER_I2C_ENABLE

static struct {
    bool packet_received;
    i2c_rxdata_t rx;
} i2c_s = {0};

i2c_rxdata_t *i2c_rx_poll (void)
{
    static i2c_rxdata_t rxdata;

    if(i2c_s.packet_received) {
        memcpy(&rxdata, &i2c_s.rx, i2c_s.rx.len + sizeof(size_t));
        i2c_s.rx.len = 0;
        i2c_s.packet_received = false;
    } else
        rxdata.len = 0;

    return rxdata.len ? &rxdata : NULL;
}

#endif

#if UILIB_KEYPAD_ENABLE

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

#endif

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

static void i2c_slave_handler (i2c_inst_t *i2c, i2c_slave_event_t event)
{
    switch(event) {

#ifdef PARSER_I2C_ENABLE

        case I2C_SLAVE_RECEIVE:
            if(!i2c_s.packet_received && i2c_s.rx.len < sizeof(i2c_s.rx.data))
                i2c_s.rx.data[i2c_s.rx.len++] = i2c_read_byte_raw(i2c);
            else
                i2c_read_byte_raw(i2c);
            break;

        case I2C_SLAVE_FINISH:
            i2c_s.packet_received = i2c_s.rx.len != 0;
            break;

#endif

#if UILIB_KEYPAD_ENABLE

        case I2C_SLAVE_REQUEST:;
            keyevent_t *keypress = keypad_get_forward_key();

            i2c_write_byte_raw(i2c, keypress->keycode);
            
            if(keypress->claimed) {
                keypress->keycode = 0;
                if(!keypad_isKeydown()) {
                    keypress->claimed = false;
                    keypad_setFwd(false);
                }
            } else
                keypad_setFwd(false);
            break;
#endif

        default:
            break;
    }
}
