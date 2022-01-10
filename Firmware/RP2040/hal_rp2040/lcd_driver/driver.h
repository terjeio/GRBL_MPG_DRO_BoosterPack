/*
 * rp2040.h
 *
 *  Created on: Apr 4, 2014
 *      Author: RobG
 */

/*
 * Mods by Terje Io (added touch support etc)
 */

#ifndef __LCD_DRIVER_H__
#define __LCD_DRIVER_H__

#include "../src/LCD/config.h"

#include "hardware/spi.h"

#define LCD_RESET_PIN

#define LCD_PIN_MISO    16
#define LCD_PIN_CS      17
#define LCD_PIN_SCK     18
#define LCD_PIN_MOSI    19
#define LCD_PIN_DC      7
//#define LCD_PIN_RESET   21

#define SPI_PORT spi0

#define LCD_SELECT gpio_put(LCD_PIN_CS, 0);
#define LCD_DESELECT gpio_put(LCD_PIN_CS, 1);

#define LCD_DC_CMD gpio_put(LCD_PIN_DC, 0);
#define LCD_DC_DATA gpio_put(LCD_PIN_DC, 1);

#ifdef TOUCH_MAXSAMPLES

#define TOUCH_CS_PIN GPIO_PIN_2 // T_CS -> PE2
#define TOUCH_CS_PORT GPIO_PORTA_BASE
#define TOUCH_SELECT GPIO_PORTA_DATA_R &= ~TOUCH_CS_PIN
#define TOUCH_DESELECT GPIO_PORTA_DATA_R |= TOUCH_CS_PIN

#define TOUCH_IRQ_PIN GPIO_PIN_3 // T_IRQ -> PF1
#define TOUCH_IRQ_PORT GPIO_PORTA_BASE
#define TOUCH_IRQ_INT_PIN GPIO_INT_PIN_3
#define TOUCH_IRQ_PERIPH SYSCTL_PERIPH_GPIOA

#endif

#endif /* __LCD_DRIVER_H__ */
