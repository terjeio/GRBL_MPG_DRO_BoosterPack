/*
 * touch.c - touch panel high level driver
 *
 * v1.00 / 2021-03-05 / Terje Io
 *
 */

/*

Copyright (c) 2015-2021, Terje Io
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

#include <stdint.h>
#include <stdbool.h>

#include "../config.h"
#include "../colorRGB.h"
#include "../graphics.h"
#include "../../UILib/uilib.h"

#include "calibrate.h"
#include "quickselect.h"

#if TOUCH_ENABLE

static uint8_t samples = 25;
static volatile bool penirq = false;
static int32_t (*touchEventHandler)(uint32_t ulMessage, int32_t lX, int32_t lY) = 0;
static int32_t xMax, yMax;
static POINT result = {-1, -1};
static MATRIX matrix;
static lcd_driver_t *driver;

static void TouchIntHandler (void)
{
    if(penirq != lcd_touchIsPenDown()) {

        if(penirq = !penirq) {
            result.x = -1;
            result.y = -1;
        }

        if(touchEventHandler)
            touchEventHandler(penirq ? WIDGET_MSG_PTR_DOWN : WIDGET_MSG_PTR_UP, result.x, result.y);
    }
}

void TOUCH_Init (lcd_driver_t *drv)
{
    driver = drv;
    driver->touchIRQHandler = TouchIntHandler;
    xMax = driver->display.Width - 1;
    yMax = driver->display.Height - 1;
}

bool TOUCH_GetPosition (void) {

    bool ok = lcd_touchIsPenDown(), isMove = result.x != -1;
    POINT touch;

    if(ok)
        touch.x = lcd_touchGetPosition(true, samples);

    ok = ok && lcd_touchIsPenDown();

    if(ok)
        touch.y = lcd_touchGetPosition(false, samples);

    ok = ok && touch.x > 0 && touch.y > 0 && getDisplayPoint(&result, &touch, &matrix);

    if(ok && touchEventHandler && (result.x >= 0) && (result.x <= xMax) && (result.y >= 0) && (result.y <= yMax))
        touchEventHandler(isMove ? WIDGET_MSG_PTR_MOVE : WIDGET_MSG_PTR_DOWN, result.x, result.y);

    return ok;
}

static void plotCalPoint (POINT *scr) {
    drawLine(scr->x - 20, scr->y, scr->x - 3, scr->y);
    drawLine(scr->x + 3, scr->y, scr->x + 20, scr->y);
    drawLine(scr->x, scr->y - 20, scr->x, scr->y - 3);
    drawLine(scr->x, scr->y + 3, scr->x, scr->y + 20);
}

static void getCalPoint (POINT *scr, POINT *cal) {

    bool ok = false;

    plotCalPoint(scr);

    while(!ok) {

        while(!lcd_touchIsPenDown());

        ok = TOUCH_GetPosition();
//TODO: reject if too close to last point?
        while(lcd_touchIsPenDown());

    }

    cal->x = result.x;
    cal->y = result.y;
/*
    drawString((Font *)font23x16, 80, 100, itoa(cal->x), true);
    drawString((Font *)font23x16, 80, 120, itoa(cal->y), true);
*/
}

void TOUCH_Calibrate (void) {

    POINT scrpoint[3], calpoint[3];
    int32_t (*orgHandler)(uint32_t ulMessage, int32_t lX, int32_t lY);

    xMax = driver->display.Width - 1;
    yMax = driver->display.Height - 1;

    clearScreen(true);
    setColor(White);

    samples = TOUCH_MAXSAMPLES;
    orgHandler = touchEventHandler;
    touchEventHandler = 0;

    scrpoint[0].x = xMax * 15 / 100;
    scrpoint[0].y = yMax * 15 / 100;
    scrpoint[1].x = xMax / 2;
    scrpoint[1].y = yMax * 85 / 100;
    scrpoint[2].x = xMax * 85 / 100;
    scrpoint[2].y = yMax / 2;

    setCalibrationMatrix(&scrpoint[0], &scrpoint[0], &matrix);

    getCalPoint(&scrpoint[0], &calpoint[0]);
    lcd_delayms(200);
    getCalPoint(&scrpoint[1], &calpoint[1]);
    lcd_delayms(200);
    getCalPoint(&scrpoint[2], &calpoint[2]);

    samples = 31;

    setCalibrationMatrix(&scrpoint[0], &calpoint[0], &matrix);

    touchEventHandler = orgHandler;

    clearScreen(true);

}

void TOUCH_SetEventHandler (int32_t (*eventHandler)(uint32_t ulMessage, int32_t lX, int32_t lY))
{
    touchEventHandler = eventHandler;
}

/*
void Touchpt (void)
{
    POINT scrpoint[3];

    scrpoint[0].x = xMax * 15 / 100;
    scrpoint[0].y = yMax * 15 / 100;
    scrpoint[1].x = xMax / 2;
    scrpoint[1].y = yMax * 85 / 100;
    scrpoint[2].x = xMax * 85 / 100;
    scrpoint[2].y = yMax / 2;

    plotCalPoint(&scrpoint[0]);
    plotCalPoint(&scrpoint[1]);
    plotCalPoint(&scrpoint[2]);
}
*/
#endif
