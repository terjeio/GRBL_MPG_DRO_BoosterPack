/*
 * canvas/menu.c - main menu
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2022-01-28 / (c)Io Engineering / Terje
 */

/*

Copyright (c) 2018-2022, Terje Io
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

#include "../UILib/uilib.h"

#include "sdcard.h"
#include "threading.h"
#include "grblutils.h"
#include "../grbl/parser.h"

static bool modeMPG;
static Canvas *canvasConfig = 0, *canvasPrevious;

/*
 * Event handlers
 *
 */

static void handlerConfig (Widget *self, Event *event)
{
    if(self) switch(event->reason) {

        case EventPointerUp:
            event->claimed = true;
            UILibButtonFlash((Button *)self);
            UILibWidgetDeselect(self);
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->yMax;
            break;
    }
}

static void handlerUtilities (Widget *self, Event *event)
{
    if(event->reason == EventPointerUp) {
        event->claimed = true;
        UILibButtonFlash((Button *)self);
        UILibWidgetDeselect(self);
        GRBLUtilsShowCanvas();
    }
}

static void handlerSDCard (Widget *self, Event *event)
{
    if(event->reason == EventPointerUp) {
        event->claimed = true;
        UILibButtonFlash((Button *)self);
        UILibWidgetDeselect(self);
        SDCardShowCanvas(canvasPrevious);
    }
}

static void handlerThreading (Widget *self, Event *event)
{
    if(event->reason == EventPointerUp) {
        event->claimed = true;
        UILibButtonFlash((Button *)self);
        UILibWidgetDeselect(self);
        ThreadingShowCanvas();
    }
}

static void handlerExit (Widget *self, Event *event)
{
    if(self) switch(event->reason) {

        case EventPointerUp:
            UILibWidgetDeselect(self);
            UILibCanvasDisplay(canvasPrevious);
            break;

        case EventPointerLeave:
            event->claimed = event->y < self->y;
            break;
    }
}

static void handlerCanvas (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventWidgetPainted:
            setColor(White);
            drawStringAligned(font_23x16, 0, 22, "Main menu", Align_Center, self->width, false);
            Widget *widget = canvasConfig->widget.firstChild;
            while(widget) {
                if(widget->privateData == (void *)1)
                    UILibWidgetEnable(widget, modeMPG);
                else if(widget->privateData == (void *)2)
                    UILibWidgetEnable(widget, modeMPG && grblGetOptions().sd_card && grblAwaitACK("$FM", 200));
                else if(widget->privateData == (void *)3)
                    UILibWidgetEnable(widget, modeMPG && grblGetOptions().lathe);
                widget = widget->nextSibling;
            }
            break;
    }
}

/*
 *  end event handlers
 *
 */

/*
 * Public functions
 *
 */

void MenuShowCanvas (bool mpgMode)
{
    modeMPG = mpgMode;

    if(!canvasConfig) {

        Widget *btn;
        uint16_t ypos = 40;

        canvasConfig = UILibCanvasCreate(0, 0, 320, 240, handlerCanvas);

        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfig, 35, ypos, "Exit", handlerExit), 250);
        btn = UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfig, 35, (ypos += 25), "SD Card", handlerSDCard), 250);
        btn->privateData = (void *)2;
        btn = UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfig, 35, (ypos += 25), "Threading", handlerThreading), 250);
        btn->privateData = (void *)3;
        btn = UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfig, 35, (ypos += 25), "grbl Utilities", handlerUtilities), 250);
        btn->privateData = (void *)1;
/*        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfig, 35, (ypos += 25), "Configure", handlerConfig), 250); */
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasConfig);
}
