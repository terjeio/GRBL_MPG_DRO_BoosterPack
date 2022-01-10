
#include <stdlib.h>
#include <string.h>

#include "graphics.h"

// Panel interface
__attribute__((weak)) void lcd_panelInit (lcd_driver_t *driver) {}
__attribute__((weak)) void lcd_setArea (uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd) {}
__attribute__((weak)) void lcd_displayOn (bool on) {}
__attribute__((weak)) void lcd_changeOrientation (orientation_t orientation) {}
__attribute__((weak)) uint16_t lcd_readID (void) { return 0; }

// Hardware interface
__attribute__((weak)) void lcd_driverInit (lcd_driver_t *driver) {}
__attribute__((weak)) void lcd_delayms (uint16_t ms) {}
__attribute__((weak)) void lcd_writeData (uint8_t data) {}
__attribute__((weak)) void lcd_writePixel (colorRGB565 color, uint32_t count) {}
__attribute__((weak)) void lcd_writePixels (uint16_t *pixels, uint32_t length) {}
__attribute__((weak)) void lcd_writeCommand (uint8_t command) {}
__attribute__((weak)) void lcd_readDataBegin (uint8_t command) {}
__attribute__((weak)) uint8_t lcd_readData (void) { return 0; }
__attribute__((weak)) void lcd_readDataEnd (void) {}
__attribute__((weak)) bool lcd_touchIsPenDown (void) { return false; }
__attribute__((weak)) uint16_t lcd_touchGetPosition (bool xpos, uint8_t samples) { return 0; }
