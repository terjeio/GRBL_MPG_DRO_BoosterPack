/*
 * mpg.c - QEI for Texas Instruments Tiva C (TM4C123) processor
 *
 * part MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 (alpha) / 2018-06-27
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

#include <stdbool.h>
#include <string.h>

#include "mpg.h"
#include "tiva.h"

#define MAX_POS (900000000)
#define MID_POS (MAX_POS / 2)

static mpg_t mpg;

static void (*mpgCallback)(mpg_t mpg) = 0;

void setMPGCallback (void (*fn)(mpg_t mpg))
{
    mpgCallback = fn;
}

static void MPG_X_IntHandler (void)
{
    uint32_t pos;
    static int32_t last = 0;

    QEIIntClear(QEI0_BASE, QEI_INTTIMER);

    if((pos = QEIPositionGet(QEI0_BASE)) != last) {
        last = pos;
        mpg.x.position = ((int32_t)pos - MID_POS);
        mpg.x.velocity = QEIVelocityGet(QEI0_BASE);
        if(mpgCallback)
            mpgCallback(mpg);
    }
}

static void MPG_Z_IntHandler (void)
{
    uint32_t pos;
    static int32_t last = 0;

    QEIIntClear(QEI1_BASE, QEI_INTTIMER);

    if((pos = QEIPositionGet(QEI1_BASE)) != last) {
        last = pos;
        mpg.z.position = ((int32_t)pos - MID_POS);
        mpg.z.velocity = QEIVelocityGet(QEI1_BASE);
        if(mpgCallback)
            mpgCallback(mpg);
    }
}

void MPG_SetActiveAxis (uint_fast8_t axis)
{
    // NOOP in this implementation
}

mpg_t *MPG_GetPosition (void)
{
    static mpg_t data;

    memcpy(&data, &mpg, sizeof(mpg_t)); // return a snapshot of current position!

    return &data;
}

void MPG_Reset (void)
{
    memset(&mpg, 0, sizeof(mpg_t));

    QEIPositionSet(QEI0_BASE, MID_POS);
    QEIPositionSet(QEI1_BASE, MID_POS);
}

void MPG_Init (void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI1);

    // Unlock PF0 (NMI)

    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
    HWREG(GPIO_PORTF_BASE + GPIO_O_AFSEL) |= 0x400;
    HWREG(GPIO_PORTF_BASE + GPIO_O_DEN) |= 0x01;
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;

    //

    GPIOPinConfigure(GPIO_PF0_PHA0);
    GPIOPinConfigure(GPIO_PF1_PHB0);
    GPIOPinTypeQEI(GPIO_PORTF_BASE, GPIO_PIN_0|GPIO_PIN_1);

    QEIDisable(QEI0_BASE);
    QEIIntDisable(QEI0_BASE, QEI_INTERROR|QEI_INTDIR|QEI_INTTIMER|QEI_INTINDEX);
    QEIConfigure(QEI0_BASE, (QEI_CONFIG_CAPTURE_A_B|QEI_CONFIG_NO_RESET|QEI_CONFIG_QUADRATURE|QEI_CONFIG_SWAP), MAX_POS);
    QEIVelocityConfigure(QEI0_BASE, QEI_VELDIV_1, SysCtlClockGet() / 100);
    QEIFilterEnable(QEI0_BASE);
    QEIEnable(QEI0_BASE);
    QEIVelocityEnable(QEI0_BASE);
    QEIIntRegister(QEI0_BASE, MPG_X_IntHandler);
    QEIIntEnable(QEI0_BASE, QEI_INTTIMER);


    GPIOPinConfigure(GPIO_PC5_PHA1);
    GPIOPinConfigure(GPIO_PC6_PHB1);
    GPIOPinTypeQEI(GPIO_PORTC_BASE, GPIO_PIN_5|GPIO_PIN_6);

    QEIDisable(QEI1_BASE);
    QEIIntDisable(QEI1_BASE, QEI_INTERROR|QEI_INTDIR|QEI_INTTIMER|QEI_INTINDEX);
    QEIConfigure(QEI1_BASE, (QEI_CONFIG_CAPTURE_A_B|QEI_CONFIG_NO_RESET|QEI_CONFIG_QUADRATURE|QEI_CONFIG_NO_SWAP), MAX_POS);
    QEIVelocityConfigure(QEI1_BASE, QEI_VELDIV_1, SysCtlClockGet() / 100);
    QEIFilterEnable(QEI1_BASE);
    QEIEnable(QEI1_BASE);
    QEIVelocityEnable(QEI1_BASE);
    QEIIntRegister(QEI1_BASE, MPG_Z_IntHandler);
    QEIIntEnable(QEI1_BASE, QEI_INTTIMER);

    MPG_Reset();
}
