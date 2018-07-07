/*
 * canvas/sender.c - GCode sender canvas
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

#include <grbl.h>
#include <grblcomm.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "uilib/uilib.h"
#include "canvas/grblutils.h"
#include "sender.h"
#include "serial.h"

#define PASSROW 55
#define XROW 85
#define ZROW 110

typedef enum {
    JobInit = 0,
    JobRun,
    JobComplete
} job_state_t;

static bool keyDownEvent, grblReady;
static volatile uint32_t awaitResponse;
static uint_fast8_t rqdly;
static Canvas *canvasSender = NULL, *canvasPrevious;
static Button *btnCancel;
static TextBox *txtXPos, *txtZPos, *txtPass;
static grbl_t grbl;
static leds_t leds;
static job_state_t jobState;
static gcode_t *(*getGCode)(bool ok, char *string) = NULL;

/*
 * Event handlers
 *
 */

static void keypressEventHandler (bool keyDown)
{
    if(keyDown)
        keyDownEvent = keyDown;
}

static void sendGCode (bool ok, char *line)
{
    if(ok && jobState != JobComplete) {

        gcode_t *gcode = getGCode(jobState == JobInit, line);

        jobState = gcode->complete ? JobComplete : JobRun;

        if(!gcode->complete)
            grblSendSerial(gcode->block);

        return;
    }

    if(line[0] == '<') {

        line = strtok(&line[1], "|");

        if(line && grblParseState(line, &grbl)) {

            leds.run = grbl.state == Run || grbl.state == Jog;
            leds.hold = grbl.state == Hold0 || grbl.state == Hold1;
            keypad_leds(leds);

            if(jobState == JobComplete && grbl.state == Idle)
                UILibCanvasDisplay(canvasPrevious);

//            UILibTextBoxDisplay(txtStatus, grbl.state_text);

        }

        if(grbl.state == Run) {

            line = strtok(NULL, "|");

            while(line) {

                if(!strncmp(line, "WPos:", 5)) {

                    char *next = strchr(line + 5, ',');
                    *next++ = '\0';
                    UILibTextBoxDisplay(txtXPos, line + 5);
                    next = strchr(next, ',');
                    UILibTextBoxDisplay(txtZPos, next + 1);

                } else if(!strncmp(line, "A:", 2)) {

                    char c;

                    line = &line[2];
                    leds.flood = 0;
                    leds.mist = 0;
                    leds.spindle = 0;

                    while((c = *line++)) {
                        switch(c) {

                            case 'M':
                                leds.mist = 1;
                                break;

                            case 'F':
                               leds.flood = 1;
                               break;

                            case 'S':
                            case 'C':
                               leds.spindle = 1;
                               break;
                        }
                    }
                    keypad_leds(leds);
                }

                line = strtok(NULL, "|");
            }
        }
    } else if(!strncmp(line, "[MSG:", 5))
        UILibTextBoxDisplay(txtPass, &line[5]);
}

static void checkGRBL (bool ok, char *line)
{
    if(!(grblReady = ok))
        UILibTextBoxDisplay(txtPass, line);
}

static void handlerCancel (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventPointerUp:
            event->claimed = true;
            serialPutC(CMD_STOP); // TODO: grbl need a graceful way to cancel a job after hold is complete, claim until Hold?
            UILibCanvasDisplay(canvasPrevious); // TODO: add code! if running do a hold and then a graceful stop?
            break;

        case EventPointerLeave:
            event->claimed = event->y > self->y;
            break;
    }
}

static void canvasHandler (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventNullEvent:

            if(!rqdly--) {
                if(awaitResponse) { // Sync protocol
                    if(!--awaitResponse) {
                        if(!grblReady)
                            grblSendSerial(""); // Send another empty string to check if ready
                    } else {
                        UILibTextBoxClear(txtPass);
                        setGrblTransmitCallback(sendGCode);
                        setKeyclickCallback(keypressEventHandler);
                        jobState = JobInit;
                        keyDownEvent = false;
                        leds = keypad_GetLedState();
                        sendGCode(true, ""); // Send first block to start transmission
                    }
                } else if(grblReady)
                    serialPutC('?'); // Request realtime status from grbl
                rqdly = 20;
            }

            if(keyDownEvent) {

                keyDownEvent = false;

                switch(keypad_get_keycode()) {

                    case '!':                                   //Feed hold TODO: disable when threading? In grbl?
                        serialPutC(CMD_FEED_HOLD);
                        break;

                    case '~':                                   // Cycle start
                        serialPutC(CMD_CYCLE_START);
                        break;

                    case 'M':                                   // Mist override
                        serialPutC(CMD_COOLANT_MIST_OVR_TOGGLE);
                        break;

                    case 'C':                                   // Coolant override
                        serialPutC(CMD_COOLANT_FLOOD_OVR_TOGGLE);
                        break;
                }
            }
            break;

        case EventWidgetPainted:
            setColor(White);
            drawStringAligned(font_23x16, 0, 22, "GCode Sender", Align_Center, self->width, false);
            drawString(font_23x16, 10, XROW, "X:", false);
            drawString(font_23x16, 10, ZROW, "Z:", false);
            rqdly = 20;
            grblReady = false;
            awaitResponse = 5; // Try 5 times (5 x 20 x 10ms = 1 second) to see if grbl is responsive
            grbl.state = Unknown;
            setGrblTransmitCallback(checkGRBL);
            grblSendSerial(""); // Send empty string to trigger response
            break;

        case EventWidgetClose:
            setGrblTransmitCallback(NULL);
            setKeyclickCallback(NULL);
            if(grblReady)
                grblSendSerial("M2"); // End program
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

void SenderShowCanvas (gcode_t *(*fn)(bool ok, char *line))
{
    getGCode = fn;

    if(!canvasSender) {
        canvasSender = UILibCanvasCreate(0, 0, 320, 240, canvasHandler);

        txtPass = UILibTextBoxCreate((Widget *)canvasSender, font_23x16, White, 10, PASSROW, 300, NULL);

        txtXPos = UILibTextBoxCreate((Widget *)canvasSender, font_23x16, White, 40, XROW, 100, NULL);
        txtXPos->widget.flags.alignment = Align_Right;

        txtZPos = UILibTextBoxCreate((Widget *)canvasSender, font_23x16, White, 40, ZROW, 100, NULL);
        txtZPos->widget.flags.alignment = Align_Right;

        btnCancel = UILibButtonCreate((Widget *)canvasSender, 35, 130, "Cancel", handlerCancel);
        UILibWidgetSetWidth((Widget *)btnCancel, 250);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasSender);
}
