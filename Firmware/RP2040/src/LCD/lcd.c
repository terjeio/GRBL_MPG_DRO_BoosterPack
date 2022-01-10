/*
 * lcd.c
 *
 *  Created on: Jul 12, 2013
 *      Author: RobG
 */

/*
 * Modified by Terje Io to use weakly defined functions for hw driver layer
 */

#include "lcd.h"
#include "config.h"
#include "Touch/touch.h"

static const unsigned char gamma1[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
static const unsigned char gamma2[] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};

void lcd_setArea (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
    lcd_writeCommand(CASETP);

    lcd_writeData(xStart >> 8);
    lcd_writeData(xStart);

    lcd_writeData(xEnd >> 8);
    lcd_writeData(xEnd);

    lcd_writeCommand(PASETP);

    lcd_writeData(yStart >> 8);
    lcd_writeData(yStart);

    lcd_writeData(yEnd >> 8);
    lcd_writeData(yEnd);

    lcd_writeCommand(RAMWRP);
    // data to follow
}

///////////////////////////////
// gamma, lut, and other inits
///////////////////////////////

/////////////////////////
// ILI9340 based display
/////////////////////////

#ifdef ILI9340

void lcd_displayState (bool on)
{
    lcd_writeCommand(on ? DISPON : DISPOFF);
}

void lcd_changeOrientation (uint8_t orientation)
{
    lcd_writeCommand(ILIMAC);

    switch (orientation) {
        case Orientation_Horizontal:
            lcd_writeData(0xE8);
            break;
        case Orientation_VerticalRotated:
            lcd_writeData(0x88);
            break;
        case Orientation_HorizontalRotated:
            lcd_writeData(0x28);
            break;
        default:
            lcd_writeData(0x48);
    }
}

uint32_t getGolor (uint32_t RGBcolor)
{
    return ((((RGBcolor) & 0x00F80000) >> 8) | (((RGBcolor) & 0x0000FC00) >> 5) | (((RGBcolor) & 0x000000F8) >> 3));
}

void lcd_panelInit (lcd_driver_t *driver)
{
    driver->display.Width  = SHORT_EDGE_PIXELS;
    driver->display.Height = LONG_EDGE_PIXELS;
    driver->display.Orientation = Orientation_Vertical;

    lcd_driverInit(driver);
#ifdef TOUCH_MAXSAMPLES
    TOUCH_Init(driver);
#endif

    lcd_delayms(20); // wait

    lcd_writeCommand(SWRESET);

    lcd_delayms(20); // wait

    lcd_writeCommand(PWRCTRLA);
    lcd_writeData(0x39);
    lcd_writeData(0x2C);
    lcd_writeData(0x00);
    lcd_writeData(0x34);
    lcd_writeData(0x02);

    lcd_writeCommand(PWRCTRLB);
    lcd_writeData(0x00);
    lcd_writeData(0XC1);
    lcd_writeData(0X30);

    lcd_writeCommand(DTCTRLA1);
    lcd_writeData(0x85);
    lcd_writeData(0x00);
    lcd_writeData(0x78);

    lcd_writeCommand(DTCTRLB);
    lcd_writeData(0x00);
    lcd_writeData(0x00);

    lcd_writeCommand(POSC);
    lcd_writeData(0x64);
    lcd_writeData(0x03);
    lcd_writeData(0X12);
    lcd_writeData(0X81);

    lcd_writeCommand(PRC);
    lcd_writeData(0x20);

    lcd_writeCommand(ILIPC1);
    lcd_writeData(0x23);
    lcd_writeCommand(ILIPC2);
    lcd_writeData(0x10);
    lcd_writeCommand(ILIVC1);
    lcd_writeData(0x3e);
    lcd_writeData(0x28);
    lcd_writeCommand(ILIVC2);
    lcd_writeData(0x86);

    setOrientation(driver->display.Orientation);

    lcd_writeCommand(COLMOD);
    lcd_writeData(0x55);

    lcd_writeCommand(ILIFCNM);
    lcd_writeData(0x00);
    lcd_writeData(0x18);

    lcd_writeCommand(ILIDFC);
    lcd_writeData(0x08);
    lcd_writeData(0x82);
    lcd_writeData(0x27);

    lcd_writeCommand(ILIGFD);
    lcd_writeData(0x00);
    lcd_writeCommand(ILIGS);
    lcd_writeData(0x01);

    uint_fast8_t i = 0;

    lcd_writeCommand(ILIPGC);
    for (i = 0; i < sizeof(gamma1); i++)
        lcd_writeData(gamma1[i]);

    lcd_writeCommand(ILINGC);
     for (i = 0; i < sizeof(gamma2); i++)
        lcd_writeData(gamma2[i]);

    lcd_writeCommand(SLEEPOUT);
    lcd_delayms(120);
    lcd_writeCommand(DISPON);
    lcd_delayms(120);

    lcd_writeCommand(RAMWRP);
}

uint16_t lcd_readID()
{
    uint16_t id;

    lcd_writeCommand(GER4SPI);
    lcd_writeData(0x12);
    lcd_readDataBegin(RDID4);
    id = lcd_readData() << 8;
    lcd_readDataEnd();

    lcd_writeCommand(GER4SPI);
    lcd_writeData(0x13);
    lcd_readDataBegin(RDID4);
    id |= lcd_readData();
    lcd_readDataEnd();

    return id;
}

#endif
