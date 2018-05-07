/*
 * navigator.c - navigator interface for Texas Instruments Tiva C (TM4C123) processor
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
#include "navigator.h"

static void navigator_int_handler (void);

static struct {
    int32_t pos;
    int32_t min;
    int32_t max;
    bool select;
} nav;

static void (*posCallback)(bool keydown) = 0;

void setPosCallback (void (*fn)(bool keydown))
{
    posCallback = fn;
}

void navigator_setup (void)
{
    // Unlock PD7 (NMI)

    HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTD_BASE + GPIO_O_CR)|= GPIO_PIN_7;
//    HWREG(GPIO_PORTD_BASE + GPIO_O_AFSEL) |= 0x400;
//    HWREG(GPIO_PORTD_BASE + GPIO_O_DEN) |= GPIO_PIN_7;
    HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = 0;

    GPIOPinTypeGPIOInput(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);
    GPIOPadConfigSet(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    GPIOIntRegister(NAVIGATOR_PORT, navigator_int_handler);
    GPIOIntTypeSet(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B, GPIO_BOTH_EDGES);
    GPIOIntEnable(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);

    GPIOPinTypeGPIOInput(NAVIGATOR_SW_PORT, NAVIGATOR_SW);
    GPIOPadConfigSet(NAVIGATOR_SW_PORT, NAVIGATOR_SW, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOIntTypeSet(NAVIGATOR_SW_PORT, NAVIGATOR_SW, GPIO_BOTH_EDGES);
//    GPIOIntEnable(NAVIGATOR_SW_PORT, NAVIGATOR_SW);

    nav.pos = 0;
    nav.min = 0;
    nav.max = 320;
    nav.select = false;
}

void navigator_reset (void)
{
    nav.pos = nav.min;
}

void navigator_configure (int32_t pos, int32_t min, int32_t max)
{
    nav.pos = pos;
    nav.min = min;
    nav.max = max;
}

int32_t navigator_get_pos (void)
{
    return nav.pos;
}

static void navigator_int_handler (void)
{
    uint32_t iflags = GPIOIntStatus(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B),
             state  = GPIOPinRead(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);

    GPIOIntClear(NAVIGATOR_PORT, iflags);

    if(iflags & NAVIGATOR_A) {

        if(state & NAVIGATOR_A)
            nav.pos = nav.pos + (state & NAVIGATOR_B ? 1 : -1);
        else
            nav.pos = nav.pos + (state & NAVIGATOR_B ? -1 : 1);
    }

    if(iflags & NAVIGATOR_B) {

        if(state & NAVIGATOR_B)
            nav.pos = nav.pos + (state & NAVIGATOR_A ? -1 : 1);
        else
            nav.pos = nav.pos + (state &  NAVIGATOR_A ? 1 : -1);

    }

    if(nav.pos < nav.min)
        nav.pos = nav.min;
    else if(nav.pos > nav.max)
        nav.pos = nav.max;
}
