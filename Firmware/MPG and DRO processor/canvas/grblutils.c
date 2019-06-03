/*
 * canvas/grblutils.c - grbl utilities menu
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2019-06-03 / ©Io Engineering / Terje
 */

/*

Copyright (c) 2018-2019, Terje Io
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

#include <stdio.h>
#include <string.h>

#include "fonts.h"
#include "grblcomm.h"
#include "uilib/uilib.h"

static Canvas *canvasUtils = 0, *canvasPrevious;
static Label *lblResponseL = NULL, *lblResponseR = NULL;
static grbl_data_t *grbl_data = NULL;

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
    if(grbl_data->changed.flags) {

        if(grbl_data->changed.alarm) {
            if(grbl_data->alarm) {
                sprintf(line, "ALARM:%d", grbl_data->alarm);
                UILibLabelDisplay(lblResponseR, line);
            } else
                UILibLabelClear(lblResponseR);
        }

        if(grbl_data->changed.error) {
            if(grbl_data->error) {
                sprintf(line, "ERROR:%d", grbl_data->error);
                UILibLabelDisplay(lblResponseR, line);
            } else
                UILibLabelClear(lblResponseR);
        }

        if(grbl_data->changed.message)
             UILibLabelDisplay(lblResponseL, grbl_data->message);

        grbl_data->changed.flags = 0;
    }
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
        lblResponseL = UILibLabelCreate((Widget *)canvasUtils, font_freepixel_9x17, White, 5, 239, 200, NULL);
        lblResponseR = UILibLabelCreate((Widget *)canvasUtils, font_23x16, Red, 210, 239, 108, NULL);
        lblResponseR->widget.flags.alignment = Align_Right;
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasUtils);
    grbl_data = setGrblReceiveCallback(showResponse);
    setColor(White);
    drawStringAligned(font_23x16, 0, 22, "grbl Utilities", Align_Center, 320, false);
}
