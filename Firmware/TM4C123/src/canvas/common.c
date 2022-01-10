/*
 * canvas/common.c - common data entry canvas
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-15 / (c)Io Engineering / Terje
 */

/*

Copyright (c) 2018, Terje Io
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../UILib/uilib.h"

#include "sender.h"
#include "common.h"

#define COLUMN 160

static Canvas *canvasNext = 0, *canvasCommon = NULL;

/*
 * Event handlers
 *
 */

static void handlerNext (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibWidgetDeselect(self);
            UILibCanvasDisplay(canvasNext);
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->yMax;
            break;
    }
}

static void handlerCanvas (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventWidgetPainted:
            setColor(White);
            drawStringAligned(font_23x16, 0, 22,  "Common parameters", Align_Center, self->width, false);
            drawStringAligned(font_23x16, 0, 50,  "Passes:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 75,  "DOC:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 100, "Feed rate:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 125, "Spindle RPM:", Align_Right, COLUMN - 5, false);
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

void CommonShowCanvas (Canvas *next, common_t *common)
{
    if(!canvasCommon) {

        uint16_t cw = getCharWidth(font_23x16, '0');
        TextBox *textbox;
        CheckBox *chkFlood;

        canvasCommon = UILibCanvasCreate(0, 0, 320, 240, handlerCanvas);

        textbox = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COLUMN, 50, cw * 3, NULL);
        UILibTextBoxBindValue(textbox, &common->passes, DataTypeUnsignedInteger, "%lu", 3);
  //      field[0]->borderColor = Red;

        cw *= 8;
        textbox = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COLUMN, 75, cw, NULL);
        UILibTextBoxBindValue(textbox, &common->doc, DataTypeFloat, "%.3f", 8);

        textbox = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COLUMN, 100, cw, NULL);
        UILibTextBoxBindValue(textbox, &common->feed_rate, DataTypeFloat, "%.3f", 8);

        textbox = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COLUMN, 125, cw, NULL);
        UILibTextBoxBindValue(textbox, &common->rpm, DataTypeFloat, "%.3f", 8);

        UILibCheckBoxCreate((Widget *)canvasCommon, White, 25, 150, "Spindle CCW", &common->ccw, NULL);
        chkFlood = UILibCheckBoxCreate((Widget *)canvasCommon, White, 25, 175, "Flood", &common->flood, NULL);
        UILibCheckBoxCreate((Widget *)canvasCommon, White, chkFlood->widget.xMax + 10, 175, "Mist", &common->mist, NULL);

        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasCommon, 35, 210, "Next", handlerNext), 250);
    }

    canvasNext = next;

    UILibCanvasDisplay(canvasCommon);
}
