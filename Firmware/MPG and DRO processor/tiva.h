/*
 * tiva.h - includes and definitions for Texas Instruments Tiva C (TM4C123) processor
 *
 * part MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 (alpha) / 2018-05-08
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

#define TARGET_IS_BLIZZARD_RB1	//Rom.h definition
//#define	PREF(x)	x	//Use for debugging purposes to trace problems in driver.lib
#define	PREF(x)	MAP_ ## x	//Use to reduce code size

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "driverlib/fpu.h"
#include "driverlib/eeprom.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/timer.h"
#include "driverlib/pwm.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "driverlib/systick.h"
#include "driverlib/adc.h"
#include "driverlib/qei.h"

//#include "driverlib/rom.h"
//#include "driverlib/rom_map.h"

#define KEYINTR_PIN  GPIO_PIN_4
#define KEYINTR_PORT GPIO_PORTC_BASE

#define NAVIGATOR_PORT GPIO_PORTD_BASE
#define NAVIGATOR_A GPIO_PIN_6
#define NAVIGATOR_B GPIO_PIN_7

#define NAVIGATOR_SW_PORT GPIO_PORTC_BASE
#define NAVIGATOR_SW GPIO_PIN_7

#define MPG_MODE_PERIPH    SYSCTL_PERIPH_GPIOB
#define MPG_MODE_PORT      GPIO_PORTB_BASE
#define MPG_MODE_PIN       GPIO_PIN_3

#define MPG_GPIO0_PORT     GPIO_PORTB_BASE
#define MPG_GPIO0_PIN      GPIO_PIN_2

#define SIGNALS_PORT            GPIO_PORTE_BASE
#define SIGNAL_CYCLESTART_PIN   GPIO_PIN_0
#define SIGNAL_FEEDHOLD_PIN     GPIO_PIN_1

