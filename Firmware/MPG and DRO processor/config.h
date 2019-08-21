/*
 * config.h - MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2019-06-11 / ©Io Engineering / Terje
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

#ifndef _MPG_CONFIG_H_
#define _MPG_CONFIG_H_

#include "tiva.h"

#define KEYINTR_PIN  GPIO_PIN_4
#define KEYINTR_PORT GPIO_PORTC_BASE

#define KEYPAD_I2CADDR 0x49

#define NAVIGATOR_PORT  GPIO_PORTD_BASE
#define NAVIGATOR_A     GPIO_PIN_6
#define NAVIGATOR_B     GPIO_PIN_7

#define NAVIGATOR_SW_PORT   GPIO_PORTC_BASE
#define NAVIGATOR_SW_PIN    GPIO_PIN_7

#define DEBOUNCE_TIM            TIMER4
#define DEBOUNCE_TIMER_PERIPH   timerPeriph(DEBOUNCE_TIM)
#define DEBOUNCE_TIMER_BASE     timerBase(DEBOUNCE_TIM)
#define DEBOUNCE_TIMER_INT      timerINT(DEBOUNCE_TIM, A)

// GPIO

#define SPINDLEDIR_PORT     GPIO_PORTF_BASE     // GPIO1
#define SPINDLEDIR_PIN      GPIO_PIN_3

#define MPG_MODE_PERIPH     SYSCTL_PERIPH_GPIOB
#define MPG_MODE_PORT       GPIO_PORTB_BASE     // GPIO2
#define MPG_MODE_PIN        GPIO_PIN_3

#define GPIO1_PORT          GPIO_PORTB_BASE     // GPIO3
#define GPIO1_PIN           GPIO_PIN_2

// GRBL SIGNALS

#define SIGNALS_PORT            GPIO_PORTE_BASE
#define SIGNAL_CYCLESTART_PIN   GPIO_PIN_0
#define SIGNAL_FEEDHOLD_PIN     GPIO_PIN_1

#endif

