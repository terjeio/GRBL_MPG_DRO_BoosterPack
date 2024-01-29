/*
 * st7789.h - middle layer driver for ST7789 240x135 LCD display
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

#ifndef LCD_H_
#define LCD_H_

#include "config.h"

#if defined(ST7789)

#include <stdint.h>
#include <stdbool.h>

#include "graphics.h"

//#define DPYCOLORTRANSLATE(c) ((((c) & 0x00f80000) >> 8) | (((c) & 0x0000fc00) >> 5) | (((c) & 0x000000f8) >> 3))
#define DPYCOLORTRANSLATE(c) ((((c) & 0x00001C00) << 3)| (((c) & 0x000000f8) << 5) | (((c) & 0x00f80000) >> 16) | (((c) & 0x0000e000) >> 13))

#define LCD_OFFSET_HEIGHT 0
#define LCD_OFFSET_WIDTH 0

// panel size
#define LONG_EDGE_PIXELS  240
#define SHORT_EDGE_PIXELS 135

// driver specific
#define NOP         0x00
#define SWRESET     0x01
#define RDDID       0x04
#define RDDST       0x09
#define RDDPM       0x0A
#define RDDMADCTL   0x0B
#define RDDCOLMOD   0x0C
#define RDDIM       0x0D
#define RDDSM       0x0E
#define RDDSDR      0x0F
#define SLPIN       0x10
#define SLPOUT      0x11
#define PTLON       0x12
#define NORON       0x13
#define INVOFF      0x20
#define INVON       0x21
#define GAMSET      0x26
#define DISPOFF     0x28
#define DISPON      0x29
#define CASET       0x2A
#define RASET       0x2B
#define RAMWR       0x2C
#define RAMRD       0x2E
#define PTLAR       0x30
#define VSCRDEF     0x33
#define TEOFF       0x34
#define TEON        0x35
#define MADCTL      0x36
#define VSCSAD      0x37
#define IDMOFF      0x38
#define IDMON       0x39
#define COLMOD      0x3A
#define WRMEMC      0x3C
#define RDMEMC      0x3E
#define STE         0x44
#define GSCAN       0x45
#define WRDISBV     0x51
#define RDDISBV     0x52
#define WRCTRLD     0x53
#define RDCTRLD     0x54
#define WRCACE      0x55
#define RDCABC      0x56
#define WRCABCMB    0x5E
#define RDCABCMB    0x5F
#define RDABCSDR    0x68
#define RDID1       0xDA
#define RDID2       0xDB
#define RDID3       0xDC

#define RAMCTRL     0xB0
#define RGBCTRL     0xB1
#define PORCTRL     0xB2
#define FRCTRL1     0xB3
#define PARCTRL     0xB5
#define GCTRL       0xB7
#define GTADJ       0xB8
#define DGMEN       0xBA
#define VCOMS       0xBB
#define POWSAVE     0xBC
#define DLPOFFSAVE  0xBD
#define LCMCTRL     0xC0
#define IDSET       0xC1
#define VDVVRHEN    0xC2
#define VRHS        0xC3
#define VDVS        0xC4
#define VCMOFSET    0xC5
#define FRCTRL2     0xC6
#define CABCCTRL    0xC7
#define REGSEL1     0xC8
#define REGSEL2     0xCA
#define PWMFRSEL    0xCC
#define PWCTRL1     0xD0
#define VAPVANEN    0xD2
#define CMD2EN      0xDF
#define PVGAMCTRL   0xE0
#define NVGAMCTRL   0xE1
#define DGMLUTR     0xE2
#define DGMLUTB     0xE3
#define GATECTRL    0xE4
#define SPI2EN      0xE7
#define PWCTRL2     0xE8
#define EQCTRL      0xE9
#define PROMCTRL    0xEC
#define PROMEN      0xFA
#define NVMSET      0xFC
#define PROMACT     0xFE

#endif
#endif /* LCD_H_ */
