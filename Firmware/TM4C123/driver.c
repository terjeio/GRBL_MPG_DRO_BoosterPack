/*
 * hal/driver.c - MPG/DRO for grbl on a secondary processor
 *
 * v0.0.3 / 2022-01-02 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2018-2022, Terje Io
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

#include "src/interface.h"
#include "src/keypad.h"

#include "driver.h"
#include "i2c.h"

#define MAX_POS (900000000)
#define MID_POS (MAX_POS / 2)

static qei_t qei = {0}, qei_mpg;
static mpg_t mpg = {0};

static leds_t leds_state = {
    .value = 255
};
static bool keyDown = false;

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
} nav = {0};

static void keyclick_int_handler (void);
static void navigator_int_handler (void);
static void debounce_int_handler (void);
static void MPG_X_IntHandler (void);
static void MPG_Z_IntHandler (void);

void hal_init (void)
{
    i2c_nb_init();

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

    mpg_reset();


    GPIOPinTypeGPIOInput(KEYINTR_PORT, KEYINTR_PIN);
    GPIOPadConfigSet(KEYINTR_PORT, KEYINTR_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    GPIOIntRegister(KEYINTR_PORT, keyclick_int_handler);
    GPIOIntTypeSet(KEYINTR_PORT, KEYINTR_PIN, GPIO_BOTH_EDGES);
    GPIOIntEnable(KEYINTR_PORT, KEYINTR_PIN);


    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    GPIOPinTypeGPIOInput(MPG_MODE_PORT, MPG_MODE_PIN);

    GPIOPinTypeGPIOOutput(GPIO1_PORT, GPIO1_PIN);
    GPIOPadConfigSet(GPIO1_PORT, GPIO1_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_OD);
    GPIOPinWrite(GPIO1_PORT, GPIO1_PIN, GPIO1_PIN);

    GPIOPinTypeGPIOInput(SPINDLEDIR_PORT, SPINDLEDIR_PIN);
    GPIOPadConfigSet(SPINDLEDIR_PORT, SPINDLEDIR_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN|SIGNAL_FEEDHOLD_PIN, 0);
    GPIOPinTypeGPIOOutput(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN|SIGNAL_FEEDHOLD_PIN);
    GPIOPadConfigSet(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN|SIGNAL_FEEDHOLD_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);


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

    nav.xPos = 320 / 2;
 }

bool keypad_isKeydown (void)
{
    return keyDown;
}

void keypad_setFwd (bool on)
{
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_2, on ? 0 : GPIO_PIN_2);
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
    if(on) {
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN, 0);
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_FEEDHOLD_PIN, SIGNAL_FEEDHOLD_PIN);
    } else
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_FEEDHOLD_PIN, 0);
}

void signal_setCycleStart (bool on)
{
    if(on) {
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_FEEDHOLD_PIN, 0);
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN, SIGNAL_CYCLESTART_PIN);
    } else
        GPIOPinWrite(SIGNALS_PORT, SIGNAL_CYCLESTART_PIN, 0);
}

void signal_setLimitsOverride (bool on)
{
    GPIOPinWrite(GPIO1_PORT, GPIO1_PIN, on ? 0 : GPIO1_PIN);
}

bool signal_getSpindleDir (void)
{
    return GPIOPinRead(SPINDLEDIR_PORT, SPINDLEDIR_PIN) != 0;
}

void signal_setMPGMode (bool on)
{
    static bool initOk = false;

    if(!initOk) {
        initOk = true;
        GPIOPinTypeGPIOOutput(MPG_MODE_PORT, MPG_MODE_PIN);
        GPIOPadConfigSet(MPG_MODE_PORT, MPG_MODE_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_OD);
    }

    // If last mode change request was not honoured then output a 1us pulse to reset
    // grbl interrupt that may have become out of sync.
    if((GPIOPinRead(MPG_MODE_PORT, MPG_MODE_PIN) == 0) == on) {
        GPIOPinWrite(MPG_MODE_PORT, MPG_MODE_PIN, on ? MPG_MODE_PIN : 0);
        _delay_cycles(80); // 1us on a 80MHz MCU
    }

    GPIOPinWrite(MPG_MODE_PORT, MPG_MODE_PIN, on ? 0 : MPG_MODE_PIN);
}

bool signal_getMPGMode (void)
{
    return false; //GPIOPinRead(MPG_MODE_PORT, MPG_MODE_PIN) == 0;
}

void mpg_setActiveAxis (uint_fast8_t axis)
{
    /*NOOP*/
}

mpg_t *mpg_getPosition (void)
{
    static mpg_t mpg_cur;

    memcpy(&mpg_cur, &mpg, sizeof(mpg_t));

    return &mpg_cur;
}

void mpg_reset (void)
{
    mpg.x.position = mpg.y.position = mpg.z.position = 0;

    memset(&mpg, 0, sizeof(mpg_t));

    QEIPositionSet(QEI0_BASE, MID_POS);
    QEIPositionSet(QEI1_BASE, MID_POS);
}

void mpg_setCallback (on_mpgChanged_ptr fn)
{
    interface.on_mpgChanged = fn;
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
        if(interface.on_mpgChanged)
            interface.on_mpgChanged(mpg);
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
        if(interface.on_mpgChanged)
            interface.on_mpgChanged(mpg);
    }
}

// Leds

void keypad_setLeds (leds_t leds)
{
    if(leds_state.value != leds.value) {
        leds_state.value = leds.value;
        i2c_nb_send(KEYPAD_I2CADDR, leds.value);
    }
}

leds_t keypad_getLeds (void)
{
    return leds_state;
}

// Navigator

void navigator_setLimits (int16_t min, int16_t max)
{
    nav.min = min;
    nav.max = max;
}

bool NavigatorSetPosition (uint32_t xPos, uint32_t yPos, bool callback)
{
    nav.xPos = xPos;
    nav.yPos = yPos;

    if(callback && interface.on_navigator_event)
        interface.on_navigator_event(WIDGET_MSG_PTR_MOVE, nav.xPos, nav.yPos);

    return true;
}

extern uint32_t NavigatorGetYPosition (void)
{
    return (uint32_t)nav.yPos;
}

extern void NavigatorSetEventHandler (on_navigator_event_ptr on_navigator_event)
{
    interface.on_navigator_event = on_navigator_event;
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

            if(interface.on_navigator_event)
                interface.on_navigator_event(WIDGET_MSG_PTR_MOVE, nav.xPos, nav.yPos);
        }

        GPIOIntClear(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);
        GPIOIntEnable(NAVIGATOR_PORT, NAVIGATOR_A|NAVIGATOR_B);
    }

    if(nav.debounce & 0x01) {

        nav.debounce &= ~0x01;

        if(nav.select == (GPIOPinRead(NAVIGATOR_SW_PORT, NAVIGATOR_SW_PIN) == 0)) {
            if(interface.on_navigator_event)
                interface.on_navigator_event(nav.select ? WIDGET_MSG_PTR_DOWN : WIDGET_MSG_PTR_UP, nav.xPos, nav.yPos);
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


// Keypad


static void keyclick_int_handler (void)
{
    uint32_t iflags = GPIOIntStatus(KEYINTR_PORT, KEYINTR_PIN);

    if(iflags & KEYINTR_PIN) {
        if(GPIOPinRead(KEYINTR_PORT, KEYINTR_PIN) == 0) {
            keyDown = true;
            i2c_getSWKeycode(keypad_enqueue_keycode);
        } else {
            if(keypad_release()) {
                keypad_setFwd(false);
                if(!keypad_forward_queue_is_empty()) {
                    // delay...
                    keypad_setFwd(true);
                }
            } else if(interface.on_keyclick2)
                interface.on_keyclick2(false, 0); // fire key up event
            keyDown = false;
        }
    } else
        navigator_sw_int_handler();

    GPIOIntClear(KEYINTR_PORT, iflags);
}
