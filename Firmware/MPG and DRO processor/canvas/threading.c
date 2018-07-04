/*
 * canvas/threading.c - threading canvas
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-07-01 / ©Io Engineering / Terje
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

#define COL1 120

typedef struct {
    float start_x;
    float start_z;
    float target_z;
    float retract_x;
    float target_x;
    float feed_rate;
    float doc;
    uint32_t passes;
} roughing_t;

static roughing_t thread = {
   .start_x = -1.0f,
   .start_z = 0.0f,
   .target_z = -9.999f,
   .retract_x = 1.0f,
   .target_x = -1.0f,
   .feed_rate = 100.0f,
   .doc = 0.5f,
   .passes = 4
};

static gcode_t gcode;
static roughing_t exec_thread;
static uint_fast16_t state;
static bool reviewed;
static Canvas *canvasConfig = 0, *canvasPrevious;
static TextBox *field[8];
static Button *btnExecute;

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

static gcode_t *sendGCode (bool ok, char *line)
{
    static char buffer[50];
    static bool msg = true;

    buffer[0] = '\0';

    gcode.block  = buffer;
    gcode.pass   = gcode.passes - exec_thread.passes + 1;

    if(msg && state == 0) {
        if(++gcode.pass <= gcode.passes)
            sprintf(buffer, "(MSG,Pass %ld of %ld)", gcode.pass, gcode.passes);
        msg = false;
    } else

    if(ok)
      switch(state) {

        case 0: // go to X retract position
            sprintf(buffer, "G0X%.3f", exec_thread.retract_x);
            state = exec_thread.passes ? 1 : 4;
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
            exec_thread.passes--;
            if(exec_thread.target_x)
                sprintf(buffer, "G1Z%.3fF%f", exec_thread.target_z, exec_thread.feed_rate);
            else
                sprintf(buffer, "G1X%.3fZ%.3fF%f", exec_thread.target_x, exec_thread.target_z, exec_thread.feed_rate);
            exec_thread.start_x -= exec_thread.doc;
            if(exec_thread.target_x)
                exec_thread.target_x -= exec_thread.doc;
            state = 0;
            msg = true;
            break;

        case 4: // complete
            msg = true;
            UILibPublishEvent((Widget *)UILibCanvasGetCurrent(), EventWidgetClose, (position_t){0, 0}, NULL);
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

            if(!(event->claimed = end != NULL && *end != '\0')) {
                if(self->nextSibling)
                    UILibApplyEnter(self->nextSibling);
                else
                    event->claimed = event->y < 50;
            }
            break;

        case EventKeyDown:;
            c = ((keypress_t *)event->data)->key;
            if(((keypress_t *)event->data)->caret)
                ((keypress_t *)event->data)->caret->insert = !(c >= '0' && c <= '9');
            event->claimed = !UILib_ValidateKeypress((TextBox *)self, c);
            break;
    }
}

static void handlerExecute (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibButtonFlash((Button *)self);
            if(!reviewed) {
                uint_fast8_t i;
                UILibButtonSetLabel(btnExecute, "Review & confirm");
                reviewed = true;
                for(i = 0; i <= 5; i++)
                    setValue(field[i], true); // redisplay entered values!
            } else {
                state = 0;
                memcpy(&exec_thread, &thread, sizeof(roughing_t));
                exec_thread.doc = (exec_thread.start_x - exec_thread.target_x) / (float)exec_thread.passes;
                gcode.pass = 0;
                gcode.passes = thread.passes;
                SenderShowCanvas(sendGCode);
            }
            break;

        case EventPointerLeave:
            UILibButtonSetLabel(btnExecute, "Execute");
            reviewed = false;
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
            event->claimed = event->y > self->y;
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
            UILibButtonSetLabel(btnExecute, "Execute");
            reviewed = false;
            for(i = 0; i <= 5; i++)
                setValue(field[i], true);
            UILibApplyEnter((Widget *)field[0]);
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
    if(!canvasConfig) {

        uint8_t cw = getCharWidth(font_23x16, '0');

        canvasConfig = UILibCanvasCreate(0, 0, 320, 240, canvasHandler);

        field[0] = UILibTextBoxCreate((Widget *)canvasConfig, font_23x16, White, COL1, 50, cw * 3, handlerInput);
        field[0]->format = FormatUnsignedInteger;
        field[0]->widget.privateData = &thread.passes;
        UILibTextBoxEnable(field[0], 3);
        field[0]->borderColor = Red;

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

        btnExecute = UILibButtonCreate((Widget *)canvasConfig, 35, 185, "Execute", handlerExecute);
        UILibWidgetSetWidth((Widget *)btnExecute, 250);
        UILibWidgetSetWidth((Widget *)UILibButtonCreate((Widget *)canvasConfig, 35, 210, "Exit", handlerExit), 250);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasConfig);

    UILibApplyEnter(canvasConfig->widget.firstChild);
}
