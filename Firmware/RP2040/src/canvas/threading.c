/*
 * canvas/threading.c - threading canvas
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

#include "../fonts/msp.h"
#include "../UILib/uilib.h"

#include "sender.h"
#include "common.h"
#include "confirm.h"

#define font_freepixel_9x17 (Font*)freepixel_9x17_data
extern const uint8_t freepixel_9x17_data[];

#define COLUMN 120

typedef struct {
    common_t common;
    float pitch;
    float start_x;
    float start_z;
    float target_z;
    float retract_x;
    float target_x;
} roughing_t;

static roughing_t thread = {
   .pitch = 1.0,
   .start_x = 0.0f,
   .start_z = 0.0f,
   .target_z = -1.0f,
   .retract_x = 1.0f,
   .target_x = -1.0f,
   .common.feed_rate = 100.0f,
   .common.doc = 0.5f,
   .common.rpm = 300.0f,
   .common.passes = 1
};

static gcode_t gcode;
static roughing_t exec_thread;
static uint_fast16_t state, substate;
static Canvas *canvasThreading = 0, *canvasPrevious;

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
            strcpy(line->string, " Threading (G33) ");
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
        state = substate = 0;
        memcpy(&exec_thread, &thread, sizeof(roughing_t));
     //   exec_thread.doc = (exec_thread.start_x - exec_thread.target_x) / (float)exec_thread.passes;
        gcode.pass     = 0;
        gcode.passes   = thread.common.passes;
        gcode.complete = false;
    }

    buffer[0] = '\0';

    gcode.block = buffer;
    gcode.pass  = gcode.passes - exec_thread.common.passes + 1;

    if(msg && state == 0) {
        if(++gcode.pass <= gcode.passes)
            sprintf(buffer, "(MSG,Pass %lu of %lu)", gcode.pass, gcode.passes);
        msg = false;
    } else

    switch(state) {

        case 0: // initialize
            switch(substate) {

                case 0:
                    sprintf(buffer, "M%uS%.1f", exec_thread.common.ccw ? 4U : 5U, exec_thread.common.feed_rate);
                    if(!exec_thread.common.flood)
                        substate++;
                    if(!exec_thread.common.mist)
                        substate++;
                    break;
//G4P delay here? From config?
                case 1:
                    strcat(buffer, "M8");
                    if(!exec_thread.common.mist)
                        substate++;
                    break;

                case 2:
                    strcat(buffer, "M7");
                    break;
            }
            if(++substate == 3)
                state = 1;
            break;

        case 1: // go to X retract position
            sprintf(buffer, "G0X%.3f", exec_thread.retract_x);
            state++;
            break;

        case 2: // go to Z start position
            sprintf(buffer, "G0Z%.3f", exec_thread.start_z);
            state = exec_thread.common.passes ? 3 : 5;
            break;

        case 3: // go to X start position
            sprintf(buffer, "G0X%.3f", exec_thread.start_x);
            state++;
            break;

        case 4: // execute cut
            exec_thread.common.passes--;
            if(exec_thread.target_x == exec_thread.start_x)
                sprintf(buffer, "G1Z%.3fF%f", exec_thread.target_z, exec_thread.common.feed_rate);
            else
                sprintf(buffer, "G1X%.3fZ%.3fF%f", exec_thread.target_x, exec_thread.target_z, exec_thread.common.feed_rate);
            exec_thread.start_x -= exec_thread.common.doc;
            if(exec_thread.target_x)
                exec_thread.target_x -= exec_thread.common.doc;
            state = 1; // Loop
            msg = true;
            break;

        case 5: // complete
            msg = true;
            gcode.complete = true;
            break;
    }

    // if failed then what?

    return &gcode;
}

static void handlerPrev (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            UILibWidgetDeselect(self);
            CommonShowCanvas(canvasThreading, &thread.common);
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

static void handlerCanvas (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventWidgetPainted:
            setColor(White);
            drawStringAligned(font_23x16, 0, 22,  "Threading (G33)", Align_Center, self->width, false);
            drawStringAligned(font_23x16, 0, 50,  "Pitch:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 75,  "Start X:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 100, "Z:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 125, "Target X:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 150, "Z:", Align_Right, COLUMN - 5, false);
            drawStringAligned(font_23x16, 0, 175, "Retract X:", Align_Right, COLUMN - 5, false);
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
    if(!canvasThreading) {

        uint16_t cw = getCharWidth(font_23x16, '0') * 8, row = 50, i;
        Button *btn;
        TextBox *field[6];

        canvasThreading = UILibCanvasCreate(0, 0, 320, 240, handlerCanvas);

        for(i = 0; i < 6; i++) {
            field[i] = UILibTextBoxCreate((Widget *)canvasThreading, font_23x16, White, COLUMN, row, cw, NULL);
            row += 25;
        }

        UILibTextBoxBindValue(field[0], &thread.pitch, DataTypeFloat, "%.3f", 8);
        UILibTextBoxBindValue(field[1], &thread.start_x, DataTypeFloat, "%.3f", 8);
        UILibTextBoxBindValue(field[2], &thread.start_z, DataTypeFloat, "%.3f", 8);
        UILibTextBoxBindValue(field[3], &thread.target_x, DataTypeFloat, "%.3f", 8);
        UILibTextBoxBindValue(field[4], &thread.target_z, DataTypeFloat, "%.3f", 8);
        UILibTextBoxBindValue(field[5], &thread.retract_x, DataTypeFloat, "%.3f", 8);

        btn = UILibButtonCreate((Widget *)canvasThreading, 27, 210, "Next", handlerNext);
        btn = UILibButtonCreate((Widget *)canvasThreading, btn->widget.xMax + 10, 210, "Back", handlerPrev);
        UILibButtonCreate((Widget *)canvasThreading, btn->widget.xMax + 10, 210, "Exit", handlerExit);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    CommonShowCanvas(canvasThreading, &thread.common);
}
