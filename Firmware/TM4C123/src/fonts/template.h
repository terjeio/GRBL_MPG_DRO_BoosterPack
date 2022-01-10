/*
    created with FontEditor written by H. Reddmann
    HaReddmann at t-online dot de

    Template by Terje Io

    File Name           : %0:s.h
    Date                : %1:s
    Font size in bytes  : 0x%2:0.2x%3:0.2x, %9:d
    Font width          : %4:d
    Font height         : %5:d
    Font first char     : 0x%7:0.2x
    Font last char      : 0x%8:0.2x
    Font bits per pixel : %10:d
    Font is compressed  : %11:s

    The font data are defined as

    struct _FONT_ {
     // common shared fields
       uint16_t   font_Size_in_Bytes_over_all_included_Size_it_self;
       uint8_t    font_Width_in_Pixel_for_fixed_drawing;
       uint8_t    font_Height_in_Pixel_for_all_Characters;
       uint8_t    font_Bits_per_Pixels;
                    // if MSB are set then font is a compressed font
       uint8_t    font_First_Char;
       uint8_t    font_Last_Char;
       uint8_t    font_Char_Widths[font_Last_Char - font_First_Char +1];
                    // for each character the separate width in pixels,
                    // characters < 128 have an implicit virtual right empty row
                    // characters with font_Char_Widths[] == 0 are undefined

     // if compressed font then additional fields
       uint8_t    font_Byte_Padding;
                    // each Char in the table are aligned in size to this value
       uint8_t    font_RLE_Table[3];
                    // Run Length Encoding Table for compression
       uint8_t    font_Char_Size_in_Bytes[font_Last_Char - font_First_Char +1];
                    // for each char the size in (bytes / font_Byte_Padding) are stored,
                    // this get us the table to seek to the right beginning of each char
                    // in the font_data[].

     // for compressed and uncompressed fonts
       uint8_t    font_data[];
                    // bit field of all characters
    }
*/

#ifndef __%0:s_h__
#define __%0:s_h__

#ifndef __font_h__
#include "font.h"
#endif

#define %0:s_width %4:d
#define %0:s_height %5:d

#define font_%0:s (Font*)%0:s_data

const uint8_t %0:s_data[] = {
    0x%2:0.2x, 0x%3:0.2x, 0x%4:0.2x, 0x%5:0.2x, 0x%6:0.2x, 0x%7:0.2x, 0x%8:0.2x,
%12:s
};

#endif

