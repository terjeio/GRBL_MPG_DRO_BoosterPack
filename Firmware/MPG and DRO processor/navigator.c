/*
 * navigator.c - navigator interface for Texas Instruments Tiva C (TM4C123) processor
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 (alpha) / 2018-06-25
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

#include <string.h>

#include "config.h"
#include "navigator.h"
#include "UILib/uilib.h"

static void debounce_int_handler (void);
static void navigator_int_handler (void);
//static void navigator_sw_int_handler (void);

static struct {
    int32_t xPos;
    int32_t yPos;
    int32_t min;
    int32_t max;
    uint32_t sel;
    uint32_t state;
    uint32_t iflags;
    bool select;
    volatile uint32_t debounce;
} nav;

static int32_t (*eventHandler)(uint32_t ulMessage, int32_t lX, int32_t lY) = 0;

void NavigatorSetEventHandler (int32_t (*handler)(uint32_t ulMessage, int32_t lX, int32_t lY)) {
    eventHandler = handler;
}

void NavigatorInit (uint32_t xSize, uint32_t ySize)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

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

    GPIOPinTypeGPIOInput(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN);
//    GPIOIntRegister(NAVIGATOR_SW_PORT, navigator_sw_int_handler); // entry handler in keypad.c
    GPIOPadConfigSet(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOIntTypeSet(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN, GPIO_BOTH_EDGES);
    GPIOIntEnable(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN); // Enable Pin Change Interrupt

    SysCtlPeripheralEnable(DEBOUNCE_TIMER_PERIPH);
    SysCtlDelay(26); // wait a bit for peripherals to wake up
    IntPrioritySet(DEBOUNCE_TIMER_INT, 0x40); // lower priority than for Timer2 (which resets the step-dir signal)
    TimerConfigure(DEBOUNCE_TIMER_BASE, TIMER_CFG_SPLIT_PAIR|TIMER_CFG_A_ONE_SHOT);
    TimerControlStall(DEBOUNCE_TIMER_BASE, TIMER_A, true); //timer2 will stall in debug mode
    TimerIntRegister(DEBOUNCE_TIMER_BASE, TIMER_A, debounce_int_handler);
    TimerIntClear(DEBOUNCE_TIMER_BASE, 0xFFFF);
    IntPendClear(DEBOUNCE_TIMER_INT);
    TimerPrescaleSet(DEBOUNCE_TIMER_BASE, TIMER_A, 79); // configure for 1us per count
    TimerLoadSet(DEBOUNCE_TIMER_BASE, TIMER_A, 32000);  // and for a total of 32ms
    TimerIntEnable(DEBOUNCE_TIMER_BASE, TIMER_TIMA_TIMEOUT);

    nav.sel = 0;
    nav.xPos = xSize >> 2;
    nav.yPos = 0;
    nav.min = 0;
    nav.max = ySize;
    nav.select = false;
    nav.debounce = 0;
}

bool NavigatorSetPosition (uint32_t xPos, uint32_t yPos, bool callback)
{
    nav.xPos = xPos;
    nav.yPos = yPos;

    if(callback && eventHandler)
        eventHandler(WIDGET_MSG_PTR_MOVE, xPos, yPos);

    return true;
}

uint32_t NavigatorGetYPosition (void)
{
    return (uint32_t)nav.yPos;
}

static void debounce_int_handler (void)
{
    TimerIntClear(DEBOUNCE_TIMER_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag

    if(nav.debounce & 0x02) {

        nav.debounce &= ~0x02;

        if(nav.state == GPIOPinRead(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B)) {

            if(nav.iflags & NAVIGATOR_A) {

               if(nav.state & NAVIGATOR_A)
                   nav.yPos = nav.yPos + (nav.state & NAVIGATOR_B ? 1 : -1);
               else
                   nav.yPos = nav.yPos + (nav.state & NAVIGATOR_B ? -1 : 1);
            }

            if(nav.iflags & NAVIGATOR_B) {

               if(nav.state & NAVIGATOR_B)
                   nav.yPos = nav.yPos + (nav.state & NAVIGATOR_A ? -1 : 1);
               else
                   nav.yPos = nav.yPos + (nav.state &  NAVIGATOR_A ? 1 : -1);
            }

            if(nav.yPos < nav.min)
               nav.yPos = nav.min;
            else if(nav.yPos > nav.max)
               nav.yPos = nav.max;

            if(eventHandler)
               eventHandler(WIDGET_MSG_PTR_MOVE, nav.xPos, nav.yPos);
        }

        GPIOIntClear(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);
        GPIOIntEnable(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);
    }

    if(nav.debounce & 0x01) {

        nav.debounce &= ~0x01;

        if(nav.select == (GPIOPinRead(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN) == 0)) {
            if(eventHandler)
                eventHandler(nav.select ? WIDGET_MSG_PTR_DOWN : WIDGET_MSG_PTR_UP, nav.xPos, nav.yPos);
        }

        GPIOIntClear(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN); // Enable Pin Change Interrupt
        GPIOIntEnable(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN); // Enable Pin Change Interrupt
    }
}

void navigator_sw_int_handler (void)
{
    if(GPIOIntStatus(NAVIGATOR_SW_PORT, true) & NAVIGATOR_SW_PIN) {

        nav.select = GPIOPinRead(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN) == 0; // !debounce
        GPIOIntDisable(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN); // Enable Pin Change Interrupt

        nav.debounce |= 0x01;

        TimerLoadSet(DEBOUNCE_TIMER_BASE, TIMER_A, 20000);  // 20ms
        TimerEnable(DEBOUNCE_TIMER_BASE, TIMER_A);
    }
}

static void navigator_int_handler (void)
{
    nav.state  = GPIOPinRead(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);
    nav.iflags = GPIOIntStatus(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B),
    GPIOIntDisable(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);

    nav.debounce |= 0x02;

    TimerLoadSet(DEBOUNCE_TIMER_BASE, TIMER_A, 2000);  // 2ms
    TimerEnable(DEBOUNCE_TIMER_BASE, TIMER_A);
}
