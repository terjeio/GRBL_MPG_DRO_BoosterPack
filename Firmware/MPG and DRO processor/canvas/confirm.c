/*
 * canvas/confirm.h - review & confirm canvas
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-15 / ©Io Engineering / Terje
 */

/*

Copyright (c) 2018, Terje Io
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

· Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

· Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

· Neither the name of the copyright holder nor the names of its contributors may
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

#include "uilib/uilib.h"
#include "canvas/grblutils.h"
#include "canvas/sender.h"
#include "canvas/confirm.h"

static Canvas *canvasConfirm = NULL, *canvasPrevious;
static confirm_line_t *(*getLineCallback)(void);
static gcode_t *(*getGCodeCallback)(bool ok, char *line);

/*
 * Event handlers
 *
 */

static void handlerExecute (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            SenderShowCanvas(getGCodeCallback);
            break;

        case EventPointerLeave:
            event->claimed = event->y < self->y;
            break;
    }
}

static void handlerCancel (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibWidgetDeselect(self);
            UILibCanvasDisplay(canvasPrevious);
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->y;
            break;
    }
}

static void handlerFrame (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventWidgetPainted:;
            confirm_line_t *line;
            uint16_t ypos = 25;
            while((line = getLineCallback())) {
                setColor(line->fgColor);
                setBackgroundColor(line->bgColor.value == Transparent.value ? self->bgColor : line->bgColor);
                drawStringAligned(line->font, 10, ypos, line->string, line->align, 300, line->bgColor.value != Transparent.value);
                ypos += (getFontHeight(line->font) + 2) / (strlen(line->string) == 0 ? 2 : 1);
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

void MenuShowConfirm (confirm_line_t *(*getLine)(void), gcode_t *(*getGCode)(bool ok, char *line))
{
    Frame *frm;

    getGCodeCallback = getGCode;
    getLineCallback = getLine;

    if(!canvasConfirm) {
        canvasConfirm = UILibCanvasCreate(0, 0, 320, 240, NULL);

        frm = UILibFrameCreate((Widget *)canvasConfirm, 5, 5, 310, 175, handlerFrame);
        frm->bgColor = WhiteSmoke;
        frm->fgColor = Black;
        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfirm, 35, 185, "Execute", handlerExecute), 250);
        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfirm, 35, 210, "Back", handlerCancel), 250);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasConfirm);
}
