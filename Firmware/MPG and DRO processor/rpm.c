/*
 * rpm.c - RPM encoder interface (can be used if RPM is not provided by grbl real-time message)
 *
 * v0.0.1 (alpha) / 2018-04-26
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

#include "tiva.h"

volatile uint32_t rpm = 0;

static void TIMER0IntHandler (void)
{
    static uint32_t last = 0;
    uint32_t count, status = TimerIntStatus(TIMER0_BASE, false);

    count = TimerValueGet(TIMER1_BASE, TIMER_A);

    rpm = (count - last) * 600 / 40;
    last = count;

//    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, pon ? GPIO_PIN_3 : 0);
//    pon = !pon;

//    TimerLoadSet(TIMER1_BASE, TIMER_A, 0);

    TimerIntClear(TIMER0_BASE, status);
}

uint32_t RPM_Get (void)
{
    return rpm;
}

void RPM_Init (void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlDelay(3);

    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);

//    clk = SysCtlClockGet()) / 1000000;

    GPIOPinConfigure(GPIO_PF2_T1CCP0);
    GPIOPinTypeTimer(GPIO_PORTF_BASE, GPIO_PIN_2);

    TimerClockSourceSet(TIMER1_BASE, TIMER_CLOCK_SYSTEM);
//    TimerPrescaleSet(TIMER1_BASE, TIMER_A, clk));
    TimerConfigure(TIMER1_BASE, TIMER_CFG_SPLIT_PAIR|TIMER_CFG_A_CAP_COUNT_UP);
    TimerControlEvent(TIMER1_BASE, TIMER_A, TIMER_EVENT_NEG_EDGE);
//    TimerUpdateMode(TIMER1_BASE, TIMER_A, TIMER_UP_LOAD_IMMEDIATE);
    TimerEnable(TIMER1_BASE, TIMER_A);

    TimerIntRegister(TIMER0_BASE, TIMER_A, TIMER0IntHandler);
    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_A_PERIODIC);
    TimerUpdateMode(TIMER0_BASE, TIMER_A, TIMER_UP_LOAD_IMMEDIATE);
    TimerIntClear(TIMER0_BASE, 0xFFFF);
//    IntPendClear(TIMER0_BASE);
//    TimerPrescaleSet(TIMER0_BASE, TIMER_A, 790); // configure for 1us per count
    TimerLoadSet(TIMER0_BASE, TIMER_A, 7999999);  // and for a total of 32ms
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);
}
