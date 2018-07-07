/*
 * canvas/common.c - common data entry canvas
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-05 / ©Io Engineering / Terje
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "uilib/uilib.h"
#include "canvas/sender.h"
#include "canvas/common.h"

#define COL1 160

static common_t *data;
static Canvas *canvasNext = 0, *canvasCommon = NULL;
static TextBox *field[8];

static void setValue (TextBox *textbox, bool refresh)
{
    if(textbox->string) {

        switch(textbox->format) {
            case FormatFloat:
                sprintf(textbox->string, "%.3f", *(float *)textbox->widget.privateData);
                break;
            case FormatUnsignedInteger:
                sprintf(textbox->string, "%ld", *(uint32_t *)textbox->widget.privateData);
                break;
        }
        if(refresh)
            UILibTextBoxDisplay(textbox, textbox->string);
    }
}

/*
 * Event handlers
 *
 */

static void handlerInput (Widget *self, Event *event)
{
    char c, *end = NULL;

    switch(event->reason) {

        case EventPointerLeave:; // validate and assign value

            switch(((TextBox *)self)->format) {

                case FormatFloat:
                    *(float *)self->privateData = strtof(((TextBox *)self)->string, &end);
                    break;

                case FormatUnsignedInteger:
                    *(uint32_t *)self->privateData = strtoul(((TextBox *)self)->string, &end, 10);
                    break;
            }

            if(!(event->claimed = end != NULL && *end != '\0'))
                event->claimed = event->y < field[0]->widget.y;
            break;

        case EventKeyDown:;
            c = ((keypress_t *)event->data)->key;
            if(((keypress_t *)event->data)->caret)
                ((keypress_t *)event->data)->caret->insert = !(c >= '0' && c <= '9');
            event->claimed = !UILib_ValidateKeypress((TextBox *)self, c);
            break;
    }
}

static void handlerNext (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            event->claimed = true;
            UILibWidgetDeselect(self);
            UILibCanvasDisplay(canvasNext);
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->yMax;
            break;
    }
}

static void canvasHandler (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventWidgetPainted:;
            uint_fast8_t i;
            setColor(White);
            drawStringAligned(font_23x16, 0, 22,  "Common parameters", Align_Center, self->width, false);
            drawStringAligned(font_23x16, 0, 50,  "Passes:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 75,  "DOC:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 100, "Feed rate:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 125, "Spindle RPM:", Align_Right, COL1 - 5, false);
            /*
            drawStringAligned(font_23x16, 0, 150, "Z:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 175, "Retract X:", Align_Right, COL1 - 5, false);
            */
            for(i = 0; i <= 3; i++)
                setValue(field[i], true);
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

        uint8_t cw = getCharWidth(font_23x16, '0');

        canvasCommon = UILibCanvasCreate(0, 0, 320, 240, canvasHandler);

        field[0] = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COL1, 50, cw * 3, handlerInput);
        field[0]->format = FormatUnsignedInteger;

        UILibTextBoxEnable(field[0], 3);
  //      field[0]->borderColor = Red;

        cw *= 8;

        field[1] = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COL1, 75, cw, handlerInput);
        field[1]->format = FormatFloat;
        UILibTextBoxEnable(field[1], 8);

        field[2] = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COL1, 100, cw, handlerInput);
        field[2]->format = FormatFloat;
        UILibTextBoxEnable(field[2], 8);

        field[3] = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COL1, 125, cw, handlerInput);
        field[3]->format = FormatFloat;
        UILibTextBoxEnable(field[3], 8);
/*
        field[4] = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COL1, 150, cw, handlerInput);
        field[4]->format = FormatFloat;
        UILibTextBoxEnable(field[4], 8);

        field[5] = UILibTextBoxCreate((Widget *)canvasCommon, font_23x16, White, COL1, 175, cw, handlerInput);
        field[5]->format = FormatFloat;
        UILibTextBoxEnable(field[5], 8);
*/
        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasCommon, 35, 210, "Next", handlerNext), 250);
    }

    data = common;
    field[0]->widget.privateData = &data->passes;
    field[1]->widget.privateData = &data->doc;
    field[2]->widget.privateData = &data->feed_rate;
    field[3]->widget.privateData = &data->rpm;
    /*
    field[4]->widget.privateData = &thread.target_z;
    field[5]->widget.privateData = &thread.retract_x;
*/
    canvasNext = next;

    UILibCanvasDisplay(canvasCommon);
}
