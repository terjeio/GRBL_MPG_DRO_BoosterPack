/*
 * ili9486.c - middle layer driver for ILI9486 320x480 LCD display
 *
 * v0.0.1 / 2023-03-08 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2023, Terje Io
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

#include "config.h"

#ifdef ILI9486

#include "ili9486.h"
#include "Touch/touch.h"

static const unsigned char gamma1[] = {0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98, 0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00};
static const unsigned char gamma2[] = {0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75, 0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00};

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

void lcd_displayState (bool on)
{
    lcd_writeCommand(on ? DISPON : DISPOFF);
}

void lcd_changeOrientation (uint8_t orientation)
{
    lcd_writeCommand(MADCTL);

    switch (orientation) {
        case Orientation_Horizontal:
            lcd_writeData(MAD_BGR | MAD_MV);
            break;
        case Orientation_VerticalRotated:
            lcd_writeData(MAD_BGR | MAD_MY);
            break;
        case Orientation_HorizontalRotated:
            lcd_writeData(MAD_BGR | MAD_MV | MAD_MY);
            break;
        default:
            lcd_writeData(MAD_BGR | MAD_MX);
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

    lcd_writeCommand(SLEEPOUT);

    lcd_delayms(120); // wait

    lcd_writeCommand(COLMOD);
    lcd_writeData(0x55);

    lcd_writeCommand(PWCTRL3);
    lcd_writeData(0x44);

    lcd_writeCommand(VMCTRL);
    lcd_writeData(0x00);
    lcd_writeData(0x00);
    lcd_writeData(0x00);
    lcd_writeData(0x00);

    uint_fast8_t i = 0;

    lcd_writeCommand(NGAMCTRL);
    for (i = 0; i < sizeof(gamma1); i++)
        lcd_writeData(gamma1[i]);

    lcd_writeCommand(DGAMCTRL);
     for (i = 0; i < sizeof(gamma2); i++)
        lcd_writeData(gamma2[i]);

    lcd_writeCommand(INVOFF);

    setOrientation(driver->display.Orientation);

    lcd_writeCommand(DISPON);
    lcd_delayms(120);

    lcd_writeCommand(RAMWRP);
}

uint16_t lcd_readID()
{
    uint16_t id = 0;
/*
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
*/
    return id;
}

#endif
