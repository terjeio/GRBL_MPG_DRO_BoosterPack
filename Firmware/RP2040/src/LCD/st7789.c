/*
 * st7789.c - middle layer driver for ST7789 240x135 LCD display
 *
 * v0.0.1 / 2023-09-27 / (c) Io Engineering / Terje
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

#if defined(ST7789)

#include "st7789.h"

static const unsigned char gamma1[] = { 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23 };
static const unsigned char gamma2[] = { 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23 };

static orientation_t orient = Orientation_Horizontal;

void lcd_displayState (bool on)
{
    lcd_writeCommand(on ? DISPON : DISPOFF);
}

void lcd_changeOrientation (orientation_t orientation)
{
    lcd_writeCommand(MADCTL);

    switch (orientation) {
        case Orientation_Horizontal:
            lcd_writeData(0x70);
            break;
        case Orientation_VerticalRotated:
            lcd_writeData(0x88);
            break;
        case Orientation_HorizontalRotated:
            lcd_writeData(0x28);
            break;
        default:
            lcd_writeData(0x00);
    }

    orient = orientation;
}

uint32_t getGolor (uint32_t RGBcolor)
{
    return ((((RGBcolor) & 0x00F80000) >> 8) | (((RGBcolor) & 0x0000FC00) >> 5) | (((RGBcolor) & 0x000000F8) >> 3));
}

void lcd_panelInit (lcd_driver_t *driver)
{
    driver->display.Width  = LONG_EDGE_PIXELS;
    driver->display.Height = SHORT_EDGE_PIXELS;
    driver->display.Orientation = Orientation_Horizontal;

    lcd_driverInit(driver);
/*
    DEV_SET_PWM(90);
    //Hardware reset
    LCD_1IN14_Reset();
*/
    //Set the resolution and scanning method of the screen
    lcd_changeOrientation(driver->display.Orientation);

    lcd_writeCommand(COLMOD);
    lcd_writeData(0x05);

    lcd_writeCommand(PORCTRL);
    lcd_writeData(0x0C);
    lcd_writeData(0x0C);
    lcd_writeData(0x00);
    lcd_writeData(0x33);
    lcd_writeData(0x33);

    lcd_writeCommand(GCTRL);  //Gate Control
    lcd_writeData(0x35);

    lcd_writeCommand(VCOMS);  //VCOM Setting
    lcd_writeData(0x19);

    lcd_writeCommand(LCMCTRL); //LCM Control
    lcd_writeData(0x2C);

    lcd_writeCommand(VDVVRHEN);  //VDV and VRH Command Enable
    lcd_writeData(0x01);
    lcd_writeCommand(VRHS);  //VRH Set
    lcd_writeData(0x12);
    lcd_writeCommand(VDVS);  //VDV Set
    lcd_writeData(0x20);

    lcd_writeCommand(FRCTRL2);  //Frame Rate Control in Normal Mode
    lcd_writeData(0x0F);

    lcd_writeCommand(PWCTRL1);  // Power Control 1
    lcd_writeData(0xA4);
    lcd_writeData(0xA1);

    uint_fast8_t i;

    lcd_writeCommand(PVGAMCTRL);  //Positive Voltage Gamma Control
    for (i = 0; i < sizeof(gamma1); i++)
        lcd_writeData(gamma1[i]);

    lcd_writeCommand(NVGAMCTRL);  //Negative Voltage Gamma Control
    for (i = 0; i < sizeof(gamma2); i++)
        lcd_writeData(gamma2[i]);

    lcd_writeCommand(INVON);  //Display Inversion On

    lcd_writeCommand(SLPOUT);  //Sleep Out

    lcd_writeCommand(DISPON);  //Display On
}

void lcd_setArea (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{

    xStart += orient == Orientation_Horizontal ? 40 : 53;
    xEnd += orient == Orientation_Horizontal ? 40 : 53;
    yStart += orient == Orientation_Horizontal ? 53 : 40;
    yEnd += orient == Orientation_Horizontal ? 53 : 40;

    lcd_writeCommand(CASET);

    lcd_writeData(xStart >> 8);
    lcd_writeData(xStart);

    lcd_writeData(xEnd >> 8);
    lcd_writeData(xEnd);

    lcd_writeCommand(RASET);

    lcd_writeData(yStart >> 8);
    lcd_writeData(yStart);

    lcd_writeData(yEnd >> 8);
    lcd_writeData(yEnd);

    lcd_writeCommand(RAMWR);
}

#endif
