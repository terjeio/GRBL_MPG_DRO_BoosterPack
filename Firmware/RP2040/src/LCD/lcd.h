/*
 * lcd.h
 *
 *  Created on: Jul 12, 2013
 *      Author: RobG
 */

/*
 * Mods by Terje Io
 */

#ifndef LCD_H_
#define LCD_H_

#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "graphics.h"

//#define DPYCOLORTRANSLATE(c) ((((c) & 0x00f80000) >> 8) | (((c) & 0x0000fc00) >> 5) | (((c) & 0x000000f8) >> 3))
#define DPYCOLORTRANSLATE(c) ((((c) & 0x00001C00) << 3)| (((c) & 0x000000f8) << 5) | (((c) & 0x00f80000) >> 16) | (((c) & 0x0000e000) >> 13))

#if defined ILI9340
//
#define LCD_OFFSET_HEIGHT 0
#define LCD_OFFSET_WIDTH 0
//
#endif

// panel size
#define LONG_EDGE_PIXELS 320
#define SHORT_EDGE_PIXELS 240

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
#define SETGAMMAP   0xC2
#define SETGAMMAN   0xC3

// ILI9340 specific
#define ILIGS           0x26
#define ILIMAC          0x36
#define WRDISBV         0x51
#define RDDISBV         0x52
#define WRCTRLD         0x53
#define RDCTRLD         0x53
#define WRCABC          0x55
#define RDCABC          0x56
#define ILIFCNM         0xB1
#define ILIFCIM         0xB2
#define ILIFCPM         0xB3
#define ILIDFC          0xB6
#define ILIPC1          0xC0
#define ILIPC2          0xC1
#define ILIVC1          0xC5
#define ILIVC2          0xC7
#define PWRCTRLA        0xCB
#define PWRCTRLB        0xCF
#define RDID4           0xD3
#define GER4SPI         0xD9
#define ILIPGC          0xE0
#define ILINGC          0xE1
#define DTCTRLA1        0xE8
#define DTCTRLA2        0xE9
#define DTCTRLB         0xEA
#define POSC            0xED
#define ILIGFD          0xF2
#define PRC             0xF7

#endif /* LCD_H_ */
