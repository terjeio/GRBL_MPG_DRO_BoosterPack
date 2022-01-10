#ifndef __font_h__
#define __font_h__

#include <stdint.h>

typedef struct {
    // common shared fields
    uint16_t size;          // size in bytes over all included size itself
    uint8_t width;          // width in pixels for fixed drawing
    uint8_t height;         // font height in pixel for all characters
    uint8_t bpp;            // bits per pixel, if MSB are set then font is a compressed font
    uint8_t firstChar;
    uint8_t lastChar;
    uint8_t charWidths[1];  // size dependent on number of glyphs
    uint8_t charData[];     // bit field of all characters
} Font;

#endif
