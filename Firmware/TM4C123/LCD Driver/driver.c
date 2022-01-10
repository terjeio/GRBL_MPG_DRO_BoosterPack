/*
 * tiva.c
 *
 *  Created on: Mar 20, 2012
 *      Author: RobG
 */

/*
 * Mods by Terje Io (pointer based API, optimizations, touch support etc)
 */

#include <stdint.h>
#include <stdbool.h>

#include "inc/tm4c123gh6pm.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/ssi.h"

#define TARGET_IS_BLIZZARD_RB1  //Rom.h definition
//#define   PREF(x) x   //Use for debugging purposes to trace problems in driver.lib
#define PREF(x) MAP_ ## x   //Use to reduce code size

#include "driverlib/rom.h"
#include "driverlib/rom_map.h"

#include "driver.h"

#include "lcd.h"
#include "touch/quickselect.h"

#define F_LCD       8000000
#define F_TOUCH     150000
#define FTOUCH_ENABLECAL 50000

static uint32_t sysclk = 0;
static lcd_driver_t *driver;
static volatile uint16_t ms_delay;
static volatile bool pendown = false;

//TODO: check real clock freq with scope!!
inline static void dpi_clk (uint32_t f)
{
    PREF(SSIDisable(SPI_BASE));
    MAP_SSIConfigSetExpClk(SPI_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, f, 8);
    PREF(SSIEnable(SPI_BASE));
}

inline static uint8_t readByte (uint8_t cmd)
{
    while (!(SPI_SR & SSI_SR_RNE));
    SPI_DR;
    SPI_DR = cmd;
    while (SPI_SR & SSI_SR_BSY);
    _delay_cycles(20);
    return (uint8_t)SPI_DR;
}

/*
 * long delay
 */
void lcd_delayms (uint16_t ms)
{
    ms_delay = ms;

    SysTickEnable();

    while(ms_delay);
}

void lcd_writePixels (uint16_t *pixels, uint32_t length)
{
    LCD_SELECT;

    while(length--) {
        while (SPI_SR & SSI_SR_BSY);
//        while (!(SPI_SR & SSI_SR_TNF));
        SPI_DR = *pixels & 0xFF;
        while (SPI_SR & SSI_SR_BSY);
//        while (!(SPI_SR & SSI_SR_TNF));
        SPI_DR = *pixels++ >> 8;
    }

    while (SPI_SR & SSI_SR_BSY);

    LCD_DESELECT;
}

void lcd_writePixel (colorRGB565 color, uint32_t count)
{
    LCD_SELECT;

    while(count--) {
        while (SPI_SR & SSI_SR_BSY);
//        while (!(SPI_SR & SSI_SR_TNF));
        SPI_DR = color.lowByte;
        while (SPI_SR & SSI_SR_BSY);
//        while (!(SPI_SR & SSI_SR_TNF));
        SPI_DR = color.highByte;
    }

    while (SPI_SR & SSI_SR_BSY);

    LCD_DESELECT;
}

void lcd_writeData (uint8_t data)
{
    LCD_SELECT;

    SPI_DR = data;

    while (SPI_SR & SSI_SR_BSY);

    LCD_DESELECT;
}

void lcd_writeCommand (uint8_t command)
{
    LCD_SELECT;
    LCD_DC_CMD;
    SPI_DR = command;

    while (SPI_SR & SSI_SR_BSY);

    LCD_DESELECT;
    LCD_DC_DATA;
}

#if TOUCH_ENABLE

/* touch functions: ADS7843 */

static bool lcd_isPenDown (void)
{
    return pendown;
}

/* get position - uses 16 bit mode */

static uint16_t lcd_getPosition (bool xpos, uint8_t samples) {

    static uint32_t buffer[TOUCH_ENABLE];

    uint8_t cmd = xpos ? 0xD0 : 0x90;
    uint32_t sampling = samples, sample;
    uint32_t *data = &buffer[0];

    if(xpos) {
        TOUCH_SELECT;
        dpi_clk(samples == TOUCH_ENABLE ? FTOUCH_ENABLECAL : F_TOUCH);
        sample = 8;
         while (sample--)
             if(SPI_SR & SSI_SR_RNE)
                 buffer[0] = SPI_SR;

         sample = readByte(cmd);
    }

    sample = readByte(0);
    sample = readByte(0);

    while(pendown && sampling) {

        sample = readByte(cmd);
        sample = readByte(0) << 5;
        sample |= readByte(0) >> 3;
        sample &= 0x0FFF;

        *data++ = sample;
        sampling--;
    }

    if(!xpos || !pendown) {
//        sample = readByte(0);
//        sample = readByte(0);
        TOUCH_DESELECT;
        dpi_clk(F_LCD);
    }

    return sampling ? 0 : quick_select(buffer, samples);
}


static void TouchIntHandler (void)
{
    MAP_GPIOIntClear(TOUCH_IRQ_PORT, TOUCH_IRQ_INT_PIN);

    pendown = MAP_GPIOPinRead(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN) == 0;

    if(driver->touchIRQHandler)
        driver->touchIRQHandler();
}

#endif

/* IRQ Handlers */

// Interrupt handler for 1 ms interval timer
void systick_isr (void)
{
    if(ms_delay)
        ms_delay--;

    if(driver->systickCallback)
        driver->systickCallback();
    else if(!ms_delay)
        SysTickDisable();
}

/* MCU peripherals init */

void lcd_driverInit (lcd_driver_t *drv)
{
    driver = drv;

    PREF(SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA));
    PREF(SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB));
    PREF(SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD));
    PREF(SysCtlPeripheralEnable(SPI_PERIPH));

#ifdef LCD_RESET_PIN
    PREF(SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF));
    PREF(GPIOPinTypeGPIOOutput(LCD_RESET_PORT, LCD_RESET_PIN));
    GPIO_PORTF_DATA_R &= ~LCD_RESET_PIN;
#endif

    PREF(GPIOPinTypeGPIOOutput(LCD_CS_PORT, LCD_CS_PIN));
    PREF(GPIOPinTypeGPIOOutput(LCD_DC_PORT, LCD_DC_PIN));
    GPIOPadConfigSet(LCD_CS_PORT, LCD_CS_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);
    GPIOPadConfigSet(LCD_DC_PORT, LCD_DC_PIN, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);

/*
    GPIO_PORTD_DATA_R |= GPIO_PIN_0;
    PREF(GPIOPinTypeGPIOOutput(LCD_CS_PORT, GPIO_PIN_0));
    GPIO_PORTD_DATA_R |= GPIO_PIN_0;

    PREF(GPIOPinTypeGPIOOutput(LCD_CS_PORT, GPIO_PIN_1));
    GPIO_PORTD_DATA_R |= GPIO_PIN_1;
*/
    PREF(GPIOPinConfigure(SPI_CLK));
    PREF(GPIOPinTypeSSI(SPI_PORT, LCD_SCLK_PIN));

    PREF(GPIOPinConfigure(SPI_TX));
    PREF(GPIOPinTypeSSI(SPI_PORT, LCD_MOSI_PIN));

    PREF(GPIOPinConfigure(SPI_RX));
    PREF(GPIOPinTypeSSI(SPI_PORT, LCD_MISO_PIN));

    LCD_DESELECT;
    LCD_DC_DATA;

    sysclk = PREF(SysCtlClockGet());

    // ILI9340's max SPI clk is 15MHz, ILI9341's 10MHz
    // however, 320x240 LCD (ILI9341) works good @16M or higher
    SSIClockSourceSet(SPI_BASE, SSI_CLOCK_SYSTEM);
    PREF(SSIConfigSetExpClk(SPI_BASE, sysclk, SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, F_LCD, 8));
    PREF(SSIEnable(SPI_BASE));

    SysTickPeriodSet(sysclk / 1000 - 1);
    SysTickIntRegister(systick_isr);
    SysTickIntEnable();

#ifdef TOUCH_CS_PORT
    MAP_GPIOPinTypeGPIOOutput(TOUCH_CS_PORT, TOUCH_CS_PIN);
    TOUCH_DESELECT;
#endif

#if TOUCH_ENABLE

    driver->touchGetPosition = getPosition;
    driver->touchIsPenDown = isPenDown;

    MAP_GPIOPinTypeGPIOInput(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN);
    GPIOIntRegister(TOUCH_IRQ_PORT, TouchIntHandler);
    MAP_GPIOIntTypeSet(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN, GPIO_BOTH_EDGES);
    MAP_GPIOIntEnable(TOUCH_IRQ_PORT, TOUCH_IRQ_INT_PIN);
    MAP_GPIOPadConfigSet(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    TOUCH_SELECT;

    _delay_cycles(500); // wait a  bit
    dpi_clk(F_TOUCH);

    /* dummy position read to put ADS7843 in a known state */
    readByte(0x90);
    readByte(0);
    readByte(0);

    dpi_clk(F_LCD);

    TOUCH_DESELECT;

#endif

#ifdef LCD_RESET_PIN
    GPIO_PORTF_DATA_R |= LCD_RESET_PIN;
#endif
}
