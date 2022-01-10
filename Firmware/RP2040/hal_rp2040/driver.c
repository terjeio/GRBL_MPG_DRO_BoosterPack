/*
 * driver.c - HAL driver for Raspberry RP2040 ARM processor
 *
 * Part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2022-01-07 / (c) Io Engineering / Terje
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
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "encoder.pio.h"

#include "../src/config.h"
#include "../src/interface.h"
#include "../src/keypad.h"

#include "i2c_nb.h"
#include "driver.h"

static qei_t qei = {0}, qei_mpg;
static mpg_t mpg = {0};
static mpg_axis_t *mpg_axis;

static leds_t leds_state = {
    .value = 255
};
static bool keyDown = false;
static void encoder_int_handler (void);
static void mpg_int_handler (void);
static void keyclick_int_handler (uint gpio, uint32_t events);
static void nav_sw_int_handler (uint gpio, uint32_t events);
static void gpio_int_handler (uint gpio, uint32_t events);

static int enc_sm, mpg_sm;

#define LAST_STATE(state)  ((state) & 0b0011)
#define CURR_STATE(state)  (((state) & 0b1100) >> 2)

static const uint32_t STATE_A_MASK      = 0x80000000;
static const uint32_t STATE_B_MASK      = 0x40000000;
static const uint32_t STATE_A_LAST_MASK = 0x20000000;
static const uint32_t STATE_B_LAST_MASK = 0x10000000;

#define STATES_MASK (STATE_A_MASK | STATE_B_MASK | STATE_A_LAST_MASK | STATE_B_LAST_MASK);

static const uint32_t TIME_MASK   = 0x0fffffff;

static const uint8_t MICROSTEP_0  = 0b00;
static const uint8_t MICROSTEP_1  = 0b10;
static const uint8_t MICROSTEP_2  = 0b11;
static const uint8_t MICROSTEP_3  = 0b01;

#define SHIFT_KEY 0b1000010000

static const keypad_key_t kmap[] = {
/*    { .key = '4', .type = Keytype_SingleEvent, .scanCode = SHIFT_KEY|0b0100000001 },
    { .key = '5', .type = Keytype_SingleEvent, .scanCode = SHIFT_KEY|0b0100000010 },
    { .key = '6', .type = Keytype_SingleEvent, .scanCode = SHIFT_KEY|0b0100000100 } */
};

void hal_init (void)
{
    int offset;

    mpg_axis = &mpg.x;

    gpio_init(SWD_RESET);
    gpio_set_dir(SWD_RESET, GPIO_OUT);
    gpio_put(SWD_RESET, 0); // Halt MSP430 keypad controller

    i2c_nb_init();

    gpio_set_irq_enabled_with_callback(NAVIGATOR_SW_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false, gpio_int_handler);
    gpio_pull_up(NAVIGATOR_SW_PIN);
    gpio_set_irq_enabled(NAVIGATOR_SW_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    gpio_pull_up(KEYINTR_PIN);
    gpio_set_irq_enabled_with_callback(KEYINTR_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_int_handler);

    gpio_init(KEYFWD_PIN);
    gpio_set_oeover(KEYFWD_PIN, GPIO_OVERRIDE_LOW); // > to OD // GPIO_OVERRIDE_INVERT does not work!
    gpio_set_dir(KEYFWD_PIN, GPIO_OUT);
    gpio_set_pulls(KEYFWD_PIN, false, false);
    gpio_put(KEYFWD_PIN, 0); // > to OD

    gpio_init(CYCLESTART_PIN);
    gpio_set_dir(CYCLESTART_PIN, GPIO_OUT);
    gpio_set_oeover(CYCLESTART_PIN, GPIO_OVERRIDE_LOW); // > to OD
    gpio_put(CYCLESTART_PIN, 0);

    gpio_init(FEEDHOLD_PIN);
    gpio_set_dir(FEEDHOLD_PIN, GPIO_OUT);
    gpio_set_oeover(FEEDHOLD_PIN, GPIO_OVERRIDE_LOW); // > to OD
    gpio_put(FEEDHOLD_PIN, 0); // > to OD

    gpio_init(MPG_MODE_PIN);

    enc_sm = pio_claim_unused_sm(pio0, true);
    uint pio_idx = pio_get_index(pio0);

    offset = pio_add_program(pio0, &encoder_program);
    encoder_program_init(pio0, enc_sm, offset, NAVIGATOR_A, NAVIGATOR_B, 250);
    hw_set_bits(&pio0->inte0, PIO_IRQ0_INTE_SM0_RXNEMPTY_BITS << enc_sm);
    encoder_program_start(pio0, enc_sm, gpio_get(NAVIGATOR_A), gpio_get(NAVIGATOR_B));

    irq_set_exclusive_handler(PIO0_IRQ_0, encoder_int_handler);
    irq_set_enabled(PIO0_IRQ_0, true);

    mpg_sm = pio_claim_unused_sm(pio1, true);
    offset = pio_add_program(pio1, &encoder_program);
    encoder_program_init(pio1, mpg_sm, offset, MPG_A, MPG_B, 250);
    hw_set_bits(&pio1->inte0, PIO_IRQ0_INTE_SM0_RXNEMPTY_BITS << mpg_sm);
    encoder_program_start(pio1, mpg_sm, gpio_get(MPG_A), gpio_get(MPG_B));

    irq_set_exclusive_handler(PIO1_IRQ_0, mpg_int_handler);
    irq_set_enabled(PIO1_IRQ_0, true);

 // Boot MSP430 keypad controller (the RP2040 does not support open drain outputs?)
    gpio_set_pulls(SWD_RESET, false, false);
    gpio_set_dir(SWD_RESET, GPIO_IN);
    delay(5); // Wait for keypad controller startup

 // Select a different key mapping than the default
 //   uint8_t map[] = {0, 3};
 //   i2c_nb_send_n(KEYPAD_I2CADDR, map, sizeof(map));

 // Add any additional key mappings to keypad controller
    for(offset = 0; offset < sizeof(kmap) / sizeof(keypad_key_t); offset++)
        i2c_nb_send_n(KEYPAD_I2CADDR, (uint8_t *)&kmap[offset], sizeof(keypad_key_t));
  }

bool keypad_isKeydown (void)
{
    return keyDown;
}

void keypad_setFwd (bool on)
{
    gpio_set_oeover(KEYFWD_PIN, on ? GPIO_OVERRIDE_HIGH : GPIO_OVERRIDE_LOW);
}

void leds_setState (leds_t leds)
{
    if(leds_state.value != leds.value) {
        leds_state.value = leds.value;
        i2c_nb_send(KEYPAD_I2CADDR, leds.value);
    }
}

leds_t leds_getState (void)
{
    return leds_state;
}

void signal_setFeedHold (bool on)
{
    if(on)
        gpio_set_oeover(CYCLESTART_PIN, GPIO_OVERRIDE_LOW);
 
    gpio_set_oeover(FEEDHOLD_PIN, on ? GPIO_OVERRIDE_HIGH : GPIO_OVERRIDE_LOW);
}

void signal_setCycleStart (bool on)
{
    if(on)
        gpio_set_oeover(FEEDHOLD_PIN, GPIO_OVERRIDE_LOW);

    gpio_set_oeover(CYCLESTART_PIN, on ? GPIO_OVERRIDE_HIGH : GPIO_OVERRIDE_LOW);
}

void signal_setMPGMode (bool on)
{
    static bool initOk = false;

    if(!initOk) {
        initOk = true;
        gpio_set_dir(MPG_MODE_PIN, GPIO_OUT);
        gpio_set_pulls(MPG_MODE_PIN, false, false);
        gpio_set_oeover(MPG_MODE_PIN, GPIO_OVERRIDE_LOW); // > to OD
        gpio_put(MPG_MODE_PIN, 0);
    }

    // If last mode change request was not honoured then output a 1us pulse to reset
    // grbl interrupt that may have become out of sync.
    if(gpio_get(MPG_MODE_PIN) == on) {
        gpio_set_oeover(MPG_MODE_PIN, on ? GPIO_OVERRIDE_HIGH : GPIO_OVERRIDE_LOW);
        sleep_us(1);
    }
}

bool signal_getMPGMode (void)
{
#ifdef UART_MODE
    return false;
#else
    return !gpio_get(MPG_MODE_PIN);
#endif
}

static uint32_t qei_xPos = 0, ymax = 0;

void navigator_setLimits (int16_t min, int16_t max)
{
    ymax = max;
}

bool NavigatorSetPosition (uint32_t xPos, uint32_t yPos, bool callback)
{
    qei_xPos = xPos;
    qei.count = yPos;

    if(callback && interface.on_navigator_event)
        interface.on_navigator_event(WIDGET_MSG_PTR_MOVE, qei_xPos, qei.count);
}

extern uint32_t NavigatorGetYPosition (void)
{
    return qei.count >= 0 ? (uint32_t)qei.count : 0;
}

extern void NavigatorSetEventHandler (on_navigator_event_ptr on_navigator_event)
{
    interface.on_navigator_event = on_navigator_event;
}

void mpg_setActiveAxis (uint_fast8_t axis)
{
    switch(axis) {

        case 0:
            mpg_axis = &mpg.x;
            break;

        case 1:
            mpg_axis = &mpg.y;
            break;

        case 2:
            mpg_axis = &mpg.z;
            break;

        default:
            break;
    }
}

mpg_t *mpg_getPosition (void)
{
    static mpg_t mpg_cur;

    memcpy(&mpg_cur, &mpg, sizeof(mpg_t));

    return &mpg_cur;
}

void mpg_reset (void)
{
    mpg.x.position =
    mpg.y.position =
    mpg.z.position =
    qei_mpg.count = 0;
}

void mpg_setCallback (on_mpgChanged_ptr fn)
{
    interface.on_mpgChanged = fn;
}

//For now...
static void gpio_int_handler (uint gpio, uint32_t events)
{
    switch(gpio) {

        case KEYINTR_PIN:
            keyclick_int_handler(gpio, events);
            break;

        case NAVIGATOR_SW_PIN:
            nav_sw_int_handler(gpio, events);
            break;

        default:
            break;
    }

}

static void keyclick_int_handler (uint gpio, uint32_t events)
{
    if(events & (GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE)) {
        if(gpio_get(KEYINTR_PIN) == 0) {
            keyDown = true;
            i2c_getSWKeycode(keypad_enqueue_keycode);
        } else {
            if(keypad_release()) {
                keypad_setFwd(false);
                if(!keypad_forward_queue_is_empty()) {
                    sleep_us(50);
                    keypad_setFwd(true);
                }
            } else if(interface.on_keyclick2)
                interface.on_keyclick2(false, 0); // fire key up event
            keyDown = false;
        }
    }
}

static int64_t debounce_callback (alarm_id_t id, void *state)
{
    if(interface.on_navigator_event && gpio_get(NAVIGATOR_SW_PIN) == *(bool *)state)
        interface.on_navigator_event(gpio_get(NAVIGATOR_SW_PIN) ? WIDGET_MSG_PTR_UP : WIDGET_MSG_PTR_DOWN, qei_xPos, qei.count);

    gpio_set_irq_enabled(NAVIGATOR_SW_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    return 0;
}

static void nav_sw_int_handler (uint gpio, uint32_t events)
{
    static bool state;

    state = events == GPIO_IRQ_EDGE_RISE;

    gpio_set_irq_enabled(NAVIGATOR_SW_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    add_alarm_in_ms(40, debounce_callback, &state, false);
}

const uint8_t encoder_valid_state[] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};

static void encoder_int_handler (void)
{
    uint32_t received, idx;
    qei_state_t state = {0};

    while(pio0->ints0 & (PIO_IRQ0_INTS_SM0_RXNEMPTY_BITS << enc_sm)) {

        received = pio_sm_get(pio0, enc_sm);

        state.a = !!(received & STATE_A_MASK);
        state.b = !!(received & STATE_B_MASK);

        idx = (((qei.state << 2) & 0x0F) | state.pins);

        if(encoder_valid_state[idx]) {

            qei.state = ((qei.state << 4) | idx) & 0xFF;

            if (qei.state == 0x42 || qei.state == 0xD4 || qei.state == 0x2B || qei.state == 0xBD) {
                if(qei.count > 0) {
                    qei.count--;
                    if(interface.on_navigator_event)
                        interface.on_navigator_event(WIDGET_MSG_PTR_MOVE, qei_xPos, qei.count);
                }
            } else if(qei.state == 0x81 || qei.state == 0x17 || qei.state == 0xE8 || qei.state == 0x7E) {
                if(qei.count < ymax) {
                    qei.count++;
                    if(interface.on_navigator_event)
                        interface.on_navigator_event(WIDGET_MSG_PTR_MOVE, qei_xPos, qei.count);
                }
            }
        }
    }
}

static void mpg_int_handler (void)
{
    static uint32_t microstep_time = 0;

    uint32_t received, idx;
    qei_state_t state = {0};

    while(pio1->ints0 & (PIO_IRQ0_INTS_SM0_RXNEMPTY_BITS << mpg_sm)) {

        received = pio_sm_get(pio1, mpg_sm);

        state.a = !!(received & STATE_A_MASK);
        state.b = !!(received & STATE_B_MASK);

        int32_t time_received = (received & TIME_MASK) + ENC_DEBOUNCE_TIME;

        // For rotary encoders, only every fourth transition is cared about, causing an inaccurate time value
        // To address this we accumulate the times received and zero it when a transition is counted
        if(true) {
            if(time_received + microstep_time < time_received)  //Check to avoid integer overflow
                time_received = INT32_MAX;
            else
                time_received += microstep_time;
            microstep_time = time_received;
        }

        idx = (((qei_mpg.state << 2) & 0x0F) | state.pins);

        if(encoder_valid_state[idx]) {

            qei_mpg.state = ((qei_mpg.state << 4) | idx) & 0xFF;

            if (qei_mpg.state == 0x42 || qei_mpg.state == 0xD4 || qei_mpg.state == 0x2B || qei_mpg.state == 0xBD) {
                qei_mpg.count--;
            } else if(qei_mpg.state == 0x81 || qei_mpg.state == 0x17 || qei_mpg.state == 0xE8 || qei_mpg.state == 0x7E) {
                qei_mpg.count++;
            }

            if(mpg_axis->position != qei_mpg.count) {
                mpg_axis->position = qei_mpg.count;
                mpg_axis->velocity = 100;
                if(interface.on_mpgChanged)
                    interface.on_mpgChanged(mpg);
            }
        }
    }
}
