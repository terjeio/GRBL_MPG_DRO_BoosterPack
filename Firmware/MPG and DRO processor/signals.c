/*
 * signals.c - signals interface for Texas Instruments Tiva C (TM4C123) processor
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-01 / ©Io Engineering / Terje
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

#include "config.h"
#include "signals.h"
#include "keypad.h"

void signalsInit (void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    GPIOPinTypeGPIOOutput(MPG_MODE_PORT, MPG_MODE_PIN);
    GPIOPinWrite(MPG_MODE_PORT, MPG_MODE_PIN, MPG_MODE_PIN);
    GPIOPadConfigSet(MPG_MODE_PORT, MPG_MODE_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_OD);
    GPIOPinWrite(MPG_MODE_PORT, MPG_MODE_PIN, 0);

    GPIOPinTypeGPIOOutput(GPIO1_PORT, GPIO1_PIN);
    GPIOPadConfigSet(GPIO1_PORT, GPIO1_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);
    GPIOPinWrite(GPIO1_PORT, GPIO1_PIN, 0);

    GPIOPinTypeGPIOInput(SPINDLEDIR_PORT, SPINDLEDIR_PIN);
    GPIOPadConfigSet(SPINDLEDIR_PORT, SPINDLEDIR_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN|SIGNAL_FEEDHOLD_PIN, 0);
    GPIOPinTypeGPIOOutput(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN|SIGNAL_FEEDHOLD_PIN);
    GPIOPadConfigSet(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN|SIGNAL_FEEDHOLD_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);

    //    Interrupt_disableSleepOnIsrExit();
}

void signalFeedHold (bool on)
{
    if(on) {
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN, 0);
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_FEEDHOLD_PIN, SIGNAL_FEEDHOLD_PIN);
    } else
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_FEEDHOLD_PIN, 0);
}

void signalCycleStart (bool on)
{
    if(on) {
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_FEEDHOLD_PIN, 0);
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN, SIGNAL_CYCLESTART_PIN);
    } else
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN, 0);
}

void signalMPGMode (bool on)
{
    GPIOPinWrite(MPG_MODE_PORT, MPG_MODE_PIN, on ? 0 : MPG_MODE_PIN);
}

bool signalSpindleDir (void)
{
    return GPIOPinRead(SPINDLEDIR_PORT, SPINDLEDIR_PIN) != 0;
}
