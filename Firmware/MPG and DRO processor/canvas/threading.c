/*
 * canvas/threading.c - threading canvas
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "uilib/uilib.h"
#include "canvas/sender.h"
#include "canvas/common.h"
#include "canvas/confirm.h"
#include "fonts/msp.h"

#define font_freepixel_9x17 (Font*)freepixel_9x17_data
extern const uint8_t freepixel_9x17_data[];

#define COL1 120

typedef struct {
    common_t common;
    float start_x;
    float start_z;
    float target_z;
    float retract_x;
    float target_x;
} roughing_t;

static roughing_t thread = {
   .start_x = -1.0f,
   .start_z = 0.0f,
   .target_z = -9.999f,
   .retract_x = 1.0f,
   .target_x = -1.0f,
   .common.feed_rate = 100.0f,
   .common.doc = 0.5f,
   .common.rpm = 200.0f,
   .common.passes = 4
};

static gcode_t gcode;
static roughing_t exec_thread;
static uint_fast16_t state;
static Canvas *canvasConfig = 0, *canvasPrevious;
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

static confirm_line_t *getLine (void)
{
    static confirm_line_t *line = NULL;

    if(!line) {
        line          = malloc(sizeof(confirm_line_t));
        line->string  = malloc(50);
        line->counter = 0;
        line->font    = font_23x16;
        line->bgColor = PaleGreen;
        line->fgColor = Black;
        line->align   = Align_Center;
    }

    switch(line->counter++) {

        case 0:
            strcpy(line->string, " Threading ");
            break;

        case 1:
            line->font = font_msp;
            strcpy(line->string, "");
            break;

        case 2:
            line->bgColor = Transparent;
            line->align   = Align_Left;
            sprintf(line->string, "Passes: %ld, DOC: %.3f", thread.common.passes, thread.common.doc);
            break;

        case 3:
            sprintf(line->string, "Feed rate: %.1f, RPM: %.1f", thread.common.feed_rate, thread.common.rpm);
            break;

        case 4:
            sprintf(line->string, "Start X: %.3f, Z: %.3f", thread.start_x, thread.start_z);
            break;

        case 5:
            sprintf(line->string, "Target X: %.3f, Z: %.3f", thread.target_x, thread.target_z);
            break;

        case 6:
            sprintf(line->string, "Retract X: %.3f, xxx: %.3f", thread.retract_x, 0.0f);
            break;

        default:
            free(line->string);
            free(line);
            line = NULL;
            break;
    }

    return line;
}

static gcode_t *getGCode (bool init, char *line)
{
    static char buffer[50];
    static bool msg = true;

    if(init) {
        state = 0;
        memcpy(&exec_thread, &thread, sizeof(roughing_t));
     //   exec_thread.doc = (exec_thread.start_x - exec_thread.target_x) / (float)exec_thread.passes;
        gcode.pass = 0;
        gcode.passes = thread.common.passes;
        gcode.complete = false;
    }

    buffer[0] = '\0';

    gcode.block = buffer;
    gcode.pass  = gcode.passes - exec_thread.common.passes + 1;

    if(msg && state == 0) {
        if(++gcode.pass <= gcode.passes)
            sprintf(buffer, "(MSG,Pass %ld of %ld)", gcode.pass, gcode.passes);
        msg = false;
    } else

    switch(state) {

        case 0: // go to X retract position
            sprintf(buffer, "G0X%.3f", exec_thread.retract_x);
            state = exec_thread.common.passes ? 1 : 4;
            break;

        case 1: // go to Z start position
            sprintf(buffer, "G0Z%.3f", exec_thread.start_z);
            state = 2;
            break;

        case 2: // go to X start position
            sprintf(buffer, "G0X%.3f", exec_thread.start_x);
            state = 3;
            break;

        case 3: // execute cut
            exec_thread.common.passes--;
            if(exec_thread.target_x)
                sprintf(buffer, "G1Z%.3fF%f", exec_thread.target_z, exec_thread.common.feed_rate);
            else
                sprintf(buffer, "G1X%.3fZ%.3fF%f", exec_thread.target_x, exec_thread.target_z, exec_thread.common.feed_rate);
            exec_thread.start_x -= exec_thread.common.doc;
            if(exec_thread.target_x)
                exec_thread.target_x -= exec_thread.common.doc;
            state = 0;
            msg = true;
            break;

        case 4: // complete
            msg = true;
            gcode.complete = true;
            break;
    }

    // if failed then what?

    return &gcode;
}

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

static void handlerPrev (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibWidgetDeselect(self);
            CommonShowCanvas(canvasConfig, &thread.common);
            break;

        case EventPointerLeave:
            break;
    }
}

static void handlerNext (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibButtonFlash((Button *)self);
            UILibWidgetDeselect(self);
            MenuShowConfirm(getLine, getGCode);
            break;
    }
}

static void handlerExit (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibWidgetDeselect(self);
            UILibCanvasDisplay(canvasPrevious);
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->yMax || event->x > self->xMax;
            break;
    }
}

static void canvasHandler (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventWidgetPainted:;
            uint_fast8_t i;
            setColor(White);
            drawStringAligned(font_23x16, 0, 22,  "Threading", Align_Center, self->width, false);
            drawStringAligned(font_23x16, 0, 50,  "Passes:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 75,  "Start X:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 100, "Z:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 125, "Target X:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 150, "Z:", Align_Right, COL1 - 5, false);
            drawStringAligned(font_23x16, 0, 175, "Retract X:", Align_Right, COL1 - 5, false);
            for(i = 0; i <= 5; i++)
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

void ThreadingShowCanvas (void)
{
    static Button *btn;

    if(!canvasConfig) {

        uint8_t cw = getCharWidth(font_23x16, '0');

        canvasConfig = UILibCanvasCreate(0, 0, 320, 240, canvasHandler);

        field[0] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 50, cw * 3, handlerInput);
        field[0]->format = FormatUnsignedInteger;
        field[0]->widget.privateData = &thread.common.passes;
        UILibTextBoxEnable(field[0], 3);
//        field[0]->borderColor = Red;

        cw *= 8;

        field[1] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 75, cw, handlerInput);
        field[1]->widget.privateData = &thread.start_x;
        field[1]->format = FormatFloat;
        UILibTextBoxEnable(field[1], 8);

        field[2] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 100, cw, handlerInput);
        field[2]->widget.privateData = &thread.start_z;
        field[2]->format = FormatFloat;
        UILibTextBoxEnable(field[2], 8);

        field[3] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 125, cw, handlerInput);
        field[3]->widget.privateData = &thread.target_x;
        field[3]->format = FormatFloat;
        UILibTextBoxEnable(field[3], 8);

        field[4] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 150, cw, handlerInput);
        field[4]->widget.privateData = &thread.target_z;
        field[4]->format = FormatFloat;
        UILibTextBoxEnable(field[4], 8);

        field[5] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 175, cw, handlerInput);
        field[5]->widget.privateData = &thread.retract_x;
        field[5]->format = FormatFloat;
        UILibTextBoxEnable(field[5], 8);

        btn = UILibButtonCreate((Widget *)canvasConfig, 27, 210, "Next", handlerNext);
        btn = UILibButtonCreate((Widget *)canvasConfig, btn->widget.xMax + 10, 210, "Back", handlerPrev);
        UILibButtonCreate((Widget *)canvasConfig, btn->widget.xMax + 10, 210, "Exit", handlerExit);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    CommonShowCanvas(canvasConfig, &thread.common);
}
