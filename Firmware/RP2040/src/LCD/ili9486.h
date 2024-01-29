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

#ifndef _ILI9486_H_
#define _ILI9486_H_

#include "config.h"

#ifdef ILI9486

#include <stdint.h>
#include <stdbool.h>

#include "graphics.h"

//#define DPYCOLORTRANSLATE(c) ((((c) & 0x00f80000) >> 8) | (((c) & 0x0000fc00) >> 5) | (((c) & 0x000000f8) >> 3))
#define DPYCOLORTRANSLATE(c) ((((c) & 0x00001C00) << 3)| (((c) & 0x000000f8) << 5) | (((c) & 0x00f80000) >> 16) | (((c) & 0x0000e000) >> 13))

#define LCD_OFFSET_HEIGHT 0
#define LCD_OFFSET_WIDTH 0

// panel size
#define LONG_EDGE_PIXELS  480
#define SHORT_EDGE_PIXELS 320

// driver specific
#define NOP         0x00
#define SWRESET     0x01
#define BSTRON      0x03
#define RDDIDIF     0x04
#define RDDST       0x09
#define SLEEPIN     0x10
#define SLEEPOUT    0x11
#define NORON       0x13
#define INVOFF      0x20
#define INVON       0x21
#define SETCON      0x25
#define DISPOFF     0x28
#define DISPON      0x29
#define CASETP      0x2A
#define PASETP      0x2B
#define RAMWRP      0x2C
#define RGBSET      0x2D
#define RAMRD       0x2E
#define MADCTL      0x36
#define SEP         0x37
#define COLMOD      0x3A
#define DISCTR      0xB9
#define DOR         0xBA
#define EC          0xC0
#define RDID1       0xDA
#define RDID2       0xDB
#define RDID3       0xDC

#define SETOSC      0xB0
#define SETPWCTR4   0xB4
#define SETPWCTR5   0xB5
#define SETEXTCMD   0xC1
#define PWCTRL3     0xC2
#define PWCTRL4     0xC3
#define VMCTRL      0xC5

#define NGAMCTRL    0xE0
#define DGAMCTRL    0xE1

#define MAD_MY      0x80
#define MAD_MX      0x40
#define MAD_MV      0x20
#define MAD_ML      0x10
#define MAD_BGR     0x08
#define MAD_MH      0x04
#define MAD_SS      0x02
#define MAD_GS      0x01
#define MAD_RGB     0x00

#endif /* _ILI9486_H_ */
#endif
