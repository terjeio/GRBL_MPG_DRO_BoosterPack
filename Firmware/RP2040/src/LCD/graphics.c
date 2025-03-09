/*
 * graphics.c
 *
 *  Created on: Mar 20, 2012
 *      Author: RobG
 */

/*
 *
 * Terje Io, 2018-01-01 : optimized code
 *           2018-07-01 : Added drawStringAligned function
 *
 */

/*
 * Terje Io, 2015-08-02 : replaced font functions, switched to RGB colors in API
 *
 * For fonts created by FontEditor written by H. Reddmann
 *
 */

#include <stdlib.h>
#include <string.h>

#include "graphics.h"
#include "colorRGB.h"
#include "config.h"

#ifdef ILI9486
#include "ili9486.h"
#elif defined(ILI9340) || defined(ILI9341)
#include "ili934x.h"
#elif defined(ST7789)
#include "st7789.h"
#endif

#ifdef TJPGD_BUFSIZE
#include "TJpegDec/tjpgd.h"
#endif

colorRGB565 fgColor;
colorRGB565 bgColor;
static lcd_driver_t driver;

/**/

bool setSysTickCallback (systick_callbak_ptr callback)
{
    delayms_attach(callback);
    lcd_delayms(1); // start systick timer
    return true; // for now
}

void setColor (RGBColor_t color)
{
    fgColor.value = DPYCOLORTRANSLATE(color.value);
}

void setBackgroundColor (RGBColor_t color)
{
    bgColor.value = DPYCOLORTRANSLATE(color.value);
}

void initGraphics (void)
{
    memset(&driver, 0, sizeof(driver));

    lcd_panelInit(&driver);
}

void displayOn (bool on)
{
    lcd_displayOn(on);
}

void setOrientation (orientation_t orientation)
{
    uint16_t tmp = 0;
//TODO: inform touch driver about changes!
    switch (orientation) {
        case Orientation_Horizontal:
        case Orientation_HorizontalRotated:
            if(driver.display.Orientation == Orientation_Vertical || driver.display.Orientation == Orientation_VerticalRotated)
                tmp = driver.display.Width;
            break;
        case Orientation_VerticalRotated:
            if(driver.display.Orientation == Orientation_Horizontal || driver.display.Orientation == Orientation_HorizontalRotated)
                tmp = driver.display.Width;
            break;
        default:
            orientation = Orientation_Vertical;
            if(driver.display.Orientation == Orientation_Horizontal || driver.display.Orientation == Orientation_HorizontalRotated)
                tmp = driver.display.Width;
    }

    if(tmp) {
        driver.display.Width = driver.display.Height;
        driver.display.Height = tmp;
    }

    driver.display.Orientation = orientation;
    lcd_changeOrientation(orientation);
}

lcd_display_t *getDisplayDescriptor (void)
{
    return &driver.display;
}

void clearScreen (bool blackWhite)
{
    setColor(blackWhite ? (RGBColor_t)White : (RGBColor_t)Black);
    setBackgroundColor(blackWhite ? (RGBColor_t)Black : (RGBColor_t)White);
    lcd_setArea(0, 0, driver.display.Width - 1, driver.display.Height - 1);
    lcd_writePixel(bgColor, (uint32_t)driver.display.Width * (uint32_t)driver.display.Height);
}

__attribute__((optimize(0))) inline static uint16_t getoffset (Font *font, uint8_t c)
{
    uint16_t offset = 0;

    if(c > font->firstChar) {
        c -= font->firstChar;
        while(c--)
            offset += font->charWidths[c];
    }

    return offset;
}

uint8_t getFontWidth (Font *font)
{
    return font->width;
}

uint8_t getFontHeight (Font *font)
{
    return font->height;
}

uint8_t getSpaceWidth (Font *font)
{
    return ('0' < font->firstChar || '0' > font->lastChar ? font->width / 4 : font->charWidths['0' - font->firstChar]) + 2;
}

uint8_t getCharWidth (Font *font, char c)
{
    return c != ' ' && (c < font->firstChar || c > font->lastChar) ? 0 : (c == ' ' || font->charWidths[c - font->firstChar] == 0 ? getSpaceWidth(font) : font->charWidths[c - font->firstChar] + 2);
}

uint16_t getStringWidth (Font *font, const char *string)
{

    char c;
    uint16_t width = 0;

    while((c = *string++))
        width += getCharWidth(font, c);

    return width;
}

uint8_t drawChar (Font *font, uint16_t x, uint16_t y, char c, bool opaque)
{
    uint8_t width = getCharWidth(font, c);

    if(width) {

        uint8_t *fontData = font->charWidths + font->lastChar - font->firstChar + 1;
        uint_fast16_t fontRow, fontColumn, displayRow, bitOffset, dataIndex, preShift;
        uint64_t pixels;
        bool paintSpace;

        bitOffset = getoffset(font, (uint8_t)c) * font->height;
        dataIndex = bitOffset >> 3;
        preShift = bitOffset - (dataIndex << 3);
        fontColumn = width;
        width -= 2;

        paintSpace = c == ' ' || !font->charWidths[c - font->firstChar];

        if(!paintSpace || opaque) {

            while(fontColumn--) {

                fontRow = font->height;
                displayRow = y - fontRow;

                if(opaque)
                    lcd_setArea(x, displayRow, x, y);

                if(!((fontColumn == 0) || (fontColumn > width)) && !paintSpace) {

#ifdef FONT_WORD_READ
                    pixels = ((uint64_t)*((uint32_t *)&fontData[dataIndex + 4])) << 32;
                    pixels |= *((uint32_t *)&fontData[dataIndex]);
#else
                    uint8_t *ptr = &fontData[dataIndex + 7];
                    uint32_t data = *ptr-- << 24;
                    data |= *ptr-- << 16;
                    data |= *ptr-- << 8;
                    data |= *ptr--;

                    pixels = data;

                    data = *ptr-- << 24;
                    data |= *ptr-- << 16;
                    data |= *ptr-- << 8;
                    data |= *ptr;
                    pixels = (pixels << 32) | data;
#endif
                    pixels >>= preShift;

//#define FONT_SLOW_PLOT
#ifdef FONT_SLOW_PLOT
                    while(fontRow--) {

                        if(pixels & 0x01) {
                            if(!opaque)
                                lcd_setArea(x, displayRow, x, displayRow);
                            lcd_writePixel(fgColor, 1);
                        } else if(opaque)
                            lcd_writePixel(bgColor, 1);
                        pixels >>= 1;
                        displayRow++;

                    }
#else
                    uint_fast16_t count;
                    bool paint;

                    count = 1;
                    paint = pixels & 0x01;

                    while(fontRow--) {
                        pixels >>= 1;
                        if((pixels & 0x01) != paint || !fontRow) {
                            if(paint) {
                                if(!opaque)
                                    lcd_setArea(x, displayRow, x, displayRow + count - 1);
                                lcd_writePixel(fgColor, count);
                            } else if(opaque)
                                lcd_writePixel(bgColor, count);
                            displayRow += count;
                            count = 1;
                            paint = pixels & 0x01;
                        } else
                            count++;
                    }
#endif
                    bitOffset += font->height;
                    dataIndex = bitOffset >> 3;
                    preShift = bitOffset - (dataIndex << 3);

                } else if(opaque)
                    lcd_writePixel(bgColor, fontRow);
                x++;
            }
        }
    }

    return width == 0 ? 0 : width + 2;
}

void drawString (Font *font, uint16_t x, uint16_t y, const char *string, bool opaque)
{
    char c;

    while((c = *string++))
        x += drawChar(font, x, y, c, opaque);
}

bool drawStringAligned (Font *font, uint16_t x, uint16_t y, const char *string, align_t align, uint16_t maxWidth, bool opaque)
{
    char c;
    uint16_t width = getStringWidth(font, string);

    if((align == Align_Right || align == Align_Center) && width > maxWidth)
        return false;

    switch(align) {

        case Align_Left:
            maxWidth += x;
            break;

        case Align_Right:
            x += (maxWidth - width);
            break;

        case Align_Center:
            x += ((maxWidth - width) >> 1);
            break;
    }

    while((c = *string++)) {
        if(align == Align_Left && x + getCharWidth(font, c) > maxWidth)
            break;
        x += drawChar(font, x, y, c, opaque);
    }

    return true;
}


/**********************
 * Drawing primitives *
 **********************/

void drawPixel (uint16_t x, uint16_t y)
{
    lcd_setArea(x, y, x, y);
    lcd_writePixel(fgColor, 1);
}

void drawLine (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
    if (yStart == yEnd) { // check if horizontal
        if(xStart > xEnd) {
            yEnd = xEnd;
            xEnd = xStart;
            xStart = yEnd;
        }
        lcd_setArea(xStart, yStart, xEnd, yStart);
        lcd_writePixel(fgColor, xEnd - xStart + 1);
    } else if (xStart == xEnd) { // check if vertical
        if(yStart > yEnd) {
            xEnd = yEnd;
            yEnd = yStart;
            yStart = xEnd;
        }
        lcd_setArea(xStart, yStart, xStart, yEnd);
        lcd_writePixel(fgColor, yEnd - yStart + 1);
    } else { // angled
        int_fast16_t dx, dy, sx, sy;

        if (xStart < xEnd) {
            sx = 1;
            dx = xEnd - xStart;
        } else {
            sx = -1;
            dx = xStart - xEnd;
        }

        if (yStart < yEnd) {
            sy = 1;
            dy = yEnd - yStart;
        } else {
            sy = -1;
            dy = yStart - yEnd;
        }

        int_fast16_t e1 = dx - dy, e2;

        while (true) {
            drawPixel(xStart, yStart);
            if (xStart == xEnd && yStart == yEnd)
                break;
            e2 = e1 << 1;
            if (e2 > -dy) {
                e1 = e1 - dy;
                xStart = xStart + sx;
            }
            if (e2 < dx) {
                e1 = e1 + dx;
                yStart = yStart + sy;
            }
        }
    }
}

void drawRect (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
    drawLine(xStart, yStart, xEnd, yStart);
    drawLine(xStart, yEnd, xEnd, yEnd);
    drawLine(xStart, yStart, xStart, yEnd);
    drawLine(xEnd, yStart, xEnd, yEnd);
}

void fillRect (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
    lcd_setArea(xStart, yStart, xEnd, yEnd);
    lcd_writePixel(fgColor, (uint32_t)(xEnd - xStart + 1) * (uint32_t)(yEnd - yStart + 1));
}

void drawCircle (uint16_t x, uint16_t y, uint16_t radius)
{
    int_fast16_t dx = radius, dy = 0, xChange = (1 - radius) << 1, yChange = 1, radiusError = 0;

    while (dx >= dy) {
        drawPixel(x + dx, y + dy);
        drawPixel(x - dx, y + dy);
        drawPixel(x - dx, y - dy);
        drawPixel(x + dx, y - dy);
        drawPixel(x + dy, y + dx);
        drawPixel(x - dy, y + dx);
        drawPixel(x - dy, y - dx);
        drawPixel(x + dy, y - dx);
        dy++;
        radiusError += yChange;
        yChange += 2;
        if ((radiusError << 1) + xChange > 0) {
            dx--;
            radiusError += xChange;
            xChange += 2;
        }
    }
}

void fillCircle (uint16_t x, uint16_t y, uint16_t radius)
{
    int_fast16_t dx = radius, dy = 0, xChange = (1 - radius) << 1, yChange = 1, radiusError = 0;

    while (dx >= dy) {
        drawLine(x + dy, y + dx, x - dy, y + dx);
        drawLine(x - dy, y - dx, x + dy, y - dx);
        drawLine(x - dx, y + dy, x + dx, y + dy);
        drawLine(x - dx, y - dy, x + dx, y - dy);
        dy++;
        radiusError += yChange;
        yChange += 2;
        if ((radiusError << 1) + xChange > 0) {
            dx--;
            radiusError += xChange;
            xChange += 2;
        }
    }
}

/*******************
 *  Image plotting *
 *******************/

#ifdef TJPGD_BUFSIZE

typedef struct {
    uint8_t *data;
    uint16_t size;
    uint16_t pos;
    uint16_t x;
    uint16_t y;
} img_fp_t;

static img_fp_t ifp;

static uint16_t jpgGetData (JDEC *jdec, uint8_t *buffer, uint16_t bytes) {

    uint16_t pos = ifp.pos;
    uint8_t *data = &ifp.data[pos];

    while(bytes--) {
        *buffer++ = *data++;
        if(++ifp.pos == ifp.size)
            break;
    }

    return ifp.pos - pos;
}

static bool jpgBitBlt (JDEC* jd, void* bitmap, JRECT* rect)
{
// TODO: implement clipping
    jd = jd; // Suppress compiler warning

    lcd_setArea(ifp.x + rect->left, ifp.y + rect->top, ifp.x + rect->right, ifp.y + rect->bottom);
    lcd_writePixels((uint16_t *)bitmap, (rect->right - rect->left + 1) * (rect->bottom - rect->top + 1));

    return true;
}

void drawJPG (const uint16_t x, const uint16_t y, const uint8_t *data, const uint16_t length)
{
    uint8_t *work = malloc(TJPGD_BUFSIZE);

    if(work) {

        JDEC jd;
        uint8_t scale;

        ifp.data = (uint8_t *)data;
        ifp.size = length;
        ifp.pos = 0;
        ifp.x = x;
        ifp.y = y;

        if(jd_prepare(&jd, jpgGetData, work, TJPGD_BUFSIZE, 0) == JDR_OK) {

            /* Determine scale factor */
            for (scale = 0; scale < 3; scale++)
                if ((jd.width >> scale) <= driver.display.Width && (jd.height >> scale) <= driver.display.Height)
                    break;

            jd_decomp(&jd, jpgBitBlt, scale); /* Start to decompress */

        }
        free(work);
    }
}
#else
void drawJPG (const uint16_t x, const uint16_t y, const uint8_t *data, const uint16_t length)
{
}
#endif

void drawImage (uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t *data)
{
}

// lut is used, ?0 means skip, sort of a mask?
void drawImageLut (uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data, uint32_t *lut)
{

}

// each bit represents color, fg and bg colors are used, ?how about 0 as a mask?
void drawImageMono (uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
    uint16_t xpos;
    uint8_t pixels, mask;

    lcd_setArea(x, y, x + w - 1, y + h - 1);

    xpos = w = (w / 8);

    while(h) {
        while(xpos) {
            pixels = *data++;
            mask = 0x80;
            while(mask) {
                if (pixels & mask)
                    //background
                    lcd_writePixel(bgColor, 1);
                else
                    //foreground
                    lcd_writePixel(fgColor, 1);
                mask >>= 1;
            }
            xpos--;
        }
        xpos = w;
        h--;
    }
}

/*******************
 * Read pixel data *
 *******************/

/*
RGBColor_t getPixel (uint16_t x, uint16_t y) {
    setArea(x, y, x, y);
    readDataBegin(0x2E);
    readData();
    RGBColor_t color;
    unsigned char temp = 0;
    color.R = readData();
    color.G = readData();
    color.B = readData();
    color &= 0x00F8;
    color <<= 5;
    color |= readData();
    color &= 0xFFFC;
    color <<= 3;
    temp = readData();
    temp >>= 3;
    color |= temp;
    readDataEnd();
    return color;
}

void getPixels (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint32_t *data) {
    setArea(xStart, yStart, xEnd, yEnd);
    uint16_t x = xEnd - xStart + 1;
    uint16_t y = yEnd - yStart + 1;
    uint16_t z = y;
    readDataBegin(0x2E);
    readData();
    while (x != 0) {
        while (z != 0) {
            uint32_t temp1 = 0;
            unsigned char temp2 = 0;
            temp1 = readData();
            temp1 &= 0x00F8;
            temp1 <<= 5;
            temp1 |= readData();
            temp1 &= 0xFFFC;
            temp1 <<= 3;
            temp2 = readData();
            temp2 >>= 3;
            temp1 |= temp2;
            *data++ = temp1;
            z--;
        }
        z = y;
        x--;
    }
    readDataEnd();
}
*/
