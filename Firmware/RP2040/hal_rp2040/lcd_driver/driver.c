/*
 * rp2040.c
 *
 *  Created on: Mar 20, 2012
 *      Author: RobG
 */

/*
 * Mods by Terje Io (pointer based API, optimizations, touch support etc)
 */

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/structs/systick.h"

#include "../src/LCD/lcd.h"

#include "driver.h"

//#include "../LCD/touch/quickselect.h"

#define F_LCD       15000000
#define F_TOUCH     150000
#define F_TOUCH_CAL 50000

static uint32_t sysclk = 0;
static lcd_driver_t *driver;
static volatile uint16_t ms_delay;
static volatile bool pendown = false;

inline static uint8_t readByte (uint8_t cmd)
{
    uint8_t buf = 0;

    spi_read_blocking(SPI_PORT, cmd, &buf, 1);

    return buf;
}

/*
 * long delay
 */
void lcd_delayms (uint16_t ms)
{
    ms_delay = ms;

    systick_hw->csr |= M0PLUS_SYST_CSR_ENABLE_BITS;

    while(ms_delay);
}

void lcd_writePixels (uint16_t *pixels, uint32_t length)
{
    LCD_SELECT;

    uint8_t *data = (uint8_t *)pixels;

    while(length--) {
        spi_write_blocking(SPI_PORT, data + 1, 1);
        spi_write_blocking(SPI_PORT, data, 1);
        data += 2;
   //     SPI_DR = *pixels & 0xFF;
   //     SPI_DR = *pixels++ >> 8;
    }

    LCD_DESELECT;
}

void lcd_writePixel (colorRGB565 color, uint32_t count)
{
    LCD_SELECT;

    while(count--) {
        spi_write_blocking(SPI_PORT, &color.lowByte, 1);
        spi_write_blocking(SPI_PORT, &color.highByte, 1);
    }

    LCD_DESELECT;
}

void lcd_writeData (uint8_t data)
{
    LCD_SELECT;

    spi_write_blocking(SPI_PORT, &data, 1);

    LCD_DESELECT;
}

void lcd_writeCommand (uint8_t command)
{
    LCD_SELECT;
    LCD_DC_CMD;

    spi_write_blocking(SPI_PORT, &command, 1);

    LCD_DESELECT;
    LCD_DC_DATA;
}

#ifdef TOUCH_MAXSAMPLES

/* touch functions: ADS7843 */

static bool lcd_isPenDown (void)
{
    return pendown;
}

/* get position - uses 16 bit mode */

static uint16_t lcd_getPosition (bool xpos, uint8_t samples) {

    static uint32_t buffer[TOUCH_MAXSAMPLES];

    uint8_t cmd = xpos ? 0xD0 : 0x90;
    uint32_t sampling = samples, sample;
    uint32_t *data = &buffer[0];

    if(xpos) {
        TOUCH_SELECT;
        dpi_clk(samples == TOUCH_MAXSAMPLES ? F_TOUCH_CAL : F_TOUCH);
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
void isr_systick (void)
{
   if(ms_delay)
        ms_delay--;

    if(driver->systickCallback)
        driver->systickCallback();
    else if(!ms_delay)
        systick_hw->csr &= ~M0PLUS_SYST_CSR_ENABLE_BITS;
}

/* MCU peripherals init */

void lcd_driverInit (lcd_driver_t *drv)
{
    driver = drv;

#ifdef LCD_PIN_RESET
    gpio_init(LCD_PIN_RESET);
    gpio_set_dir(LCD_PIN_RESET, GPIO_OUT);
    gpio_put(LCD_PIN_RESET, 0);
#endif

    spi_init(SPI_PORT, F_LCD);
//    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(LCD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(LCD_PIN_CS);
    gpio_set_dir(LCD_PIN_CS, GPIO_OUT);
    gpio_put(LCD_PIN_CS, 1);
    gpio_init(LCD_PIN_DC);
    gpio_set_dir(LCD_PIN_DC, GPIO_OUT);
    gpio_put(LCD_PIN_DC, 1);

    LCD_DESELECT;
    LCD_DC_DATA;

    systick_hw->rvr = 999;
    systick_hw->cvr = 0;
    systick_hw->csr = M0PLUS_SYST_CSR_TICKINT_BITS|M0PLUS_SYST_CSR_ENABLE_BITS;
    // M0PLUS_SYST_CSR_CLKSOURCE_BITS - set to use processor clock

#ifdef TOUCH_CS_PORT
    MAP_GPIOPinTypeGPIOOutput(TOUCH_CS_PORT, TOUCH_CS_PIN);
    TOUCH_DESELECT;
#endif

#ifdef TOUCH_MAXSAMPLES

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

#ifdef LCD_PIN_RESET
    gpio_put(LCD_PIN_RESET, 1);
#endif
}
