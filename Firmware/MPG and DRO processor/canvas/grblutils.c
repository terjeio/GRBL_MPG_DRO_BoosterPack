/*
 * canvas/grblutils.c - grbl utilities menu
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-07 / ©Io Engineering / Terje
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

#include <string.h>

#include "grblcomm.h"
#include "uilib/uilib.h"

static Canvas *canvasUtils = 0, *canvasPrevious;
static TextBox *txtResponse = NULL;

/*
 * Event handlers
 *
 */

static void handlerReset (Widget *self, Event *event)
{
    if(event->reason == EventPointerUp) {
        UILibButtonFlash((Button *)self);
        serialPutC(CMD_RESET);
    }
}

static void handlerUnlock (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibButtonFlash((Button *)self);
            serialWriteLn("$X");
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->yMax;
            break;
    }
}

static void handlerExit (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            event->claimed = true;
            UILibCanvasDisplay(canvasPrevious);
            break;

        case EventPointerLeave:
            event->claimed = event->y < self->y;
            break;
    }
}

/*
 *  end event handlers
 *
 */

static void showResponse (char *line)
{
    UILibTextBoxDisplay(txtResponse, strncmp(line, "[MSG:", 5) ? line : &line[5]);
}

/*
 * Public functions
 *
 */

void GRBLUtilsShowCanvas (void)
{
    if(!canvasUtils) {

        canvasUtils = UILibCanvasCreate(0, 0, 320, 240, NULL);

        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasUtils, 35, 40, "Exit", handlerExit), 250);
        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasUtils, 35, 65, "Reset", handlerReset), 250);
        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasUtils, 35, 90, "Unlock", handlerUnlock), 250);
        txtResponse = UILibTextBoxCreate((Widget *)canvasUtils, font_23x16, White, 0, 239, 320, NULL);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasUtils);
    setGrblReceiveCallback(showResponse);
    setColor(White);
    drawStringAligned(font_23x16, 0, 22, "grbl Utilities", Align_Center, 320, false);
}
