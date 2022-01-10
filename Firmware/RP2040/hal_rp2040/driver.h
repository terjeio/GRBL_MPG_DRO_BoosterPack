/*
 * driver.h - HAL driver for Raspberry RP2040 ARM processor
 *
 * Part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2022-01-04 / (c) Io Engineering / Terje
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

#ifndef _DRIVER_H_
#define _DRIVER_H_

#define KEYFWD_PIN          11
#define KEYINTR_PIN         5
#define MASTER_I2C_PORT     1
#define MASTER_SDA_PIN      26
#define MASTER_SCL_PIN      27

#define SLAVE_I2C_PORT      0
#define SLAVE_SDA_PIN       8
#define SLAVE_SCL_PIN       9

#define NAVIGATOR_A         14
#define NAVIGATOR_B         15
#define NAVIGATOR_SW_PIN    10

#define MPG_A               20
#define MPG_B               21

// GPIO

#define SPINDLEDIR_PIN      4 // GPIO0 / Touch IRQ
#define MPG_MODE_PIN        3 // GPIO1
#define GPIO2_PIN           2 // GPIO2

// GRBL SIGNALS

#define CYCLESTART_PIN      0
#define FEEDHOLD_PIN        1

#define UART_TX_PIN         12
#define UART_RX_PIN         13

#ifndef UART_PORT
#define UART_PORT           uart0
#define UART_IRQ            UART0_IRQ
#endif

// MSP430 SWD

#define SWD_RESET           22
#define SWD_TEST            28

#endif
