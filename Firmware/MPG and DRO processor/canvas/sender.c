/*
 * canvas/sender.c - GCode sender canvas
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
static Label *lblXPos, *lblZPos, *lblPass;
static leds_t leds;
static job_state_t jobState;
static gcode_t *(*getGCode)(bool ok, char *string) = NULL;

/*
 * Event handlers
 *
 */

static void keypressEventHandler (bool keyDown, char key)
{
    if(keyDown)
        keyDownEvent = keyDown;
}

static void sendGCode (bool ok, grbl_data_t *grbl_data)
{
    if(ok && jobState != JobComplete) {

        gcode_t *gcode = getGCode(jobState == JobInit, grbl_data->block);

        jobState = gcode->complete ? JobComplete : JobRun;

        if(!gcode->complete)
            grblSendSerial(gcode->block);

        return;
    }

    if(grbl_data->changed.state) {

        leds.run = grbl_data->grbl.state == Run || grbl_data->grbl.state == Jog;
        leds.hold = grbl_data->grbl.state == Hold0 || grbl_data->grbl.state == Hold1;
        keypad_leds(leds);

        if(jobState == JobComplete && grbl_data->grbl.state == Idle)
            UILibCanvasDisplay(canvasPrevious);

//            UILibLabelDisplay(txtStatus,  grbl_data->grbl.state_text);

    }

    if(grbl_data->grbl.state == Run) {

        if(grbl_data->changed.msg && !strncmp(grbl_data->block, "[MSG:", 5))
            UILibLabelDisplay(lblPass, &grbl_data->block[5]);

        if(grbl_data->changed.xpos) {
            sprintf(grbl_data->block, "% 9.3f", grbl_data->position[X_AXIS]);
            UILibLabelDisplay(lblXPos, grbl_data->block);
        }

        if(grbl_data->changed.zpos) {
            sprintf(grbl_data->block, "% 9.3f", grbl_data->position[Z_AXIS]);
            UILibLabelDisplay(lblZPos, grbl_data->block);
        }

        if(grbl_data->changed.leds) {
            leds.mist = grbl_data->coolant.mist;
            leds.flood = grbl_data->coolant.flood;
            leds.spindle = grbl_data->spindle.on;
            keypad_leds(leds);
        }

        grbl_data->changed.flags = 0;
    }
}

static void checkGRBL (bool ok, grbl_data_t *grbl_data)
{
    if(!(grblReady = ok))
        UILibLabelDisplay(lblPass, grbl_data->block);
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

static void handlerCanvas (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventNullEvent:

            if(!rqdly--) {
                if(awaitResponse) { // Sync protocol
                    if(--awaitResponse) {
                        if(!grblReady)
                            grblSendSerial(""); // Send another empty string to check if ready
                    } else {
                        UILibLabelDisplay(lblPass, "Sending...");
                        setGrblTransmitCallback(sendGCode);
                        setKeyclickCallback2(keypressEventHandler, false);
                        jobState = JobInit;
                        keyDownEvent = false;
                        leds = keypad_GetLedState();
                        sendGCode(true, NULL); // Send first block to start transmission
                    }
                } else if(grblReady)
                    serialPutC('?'); // Request realtime status from grbl
                rqdly = 20;
            }

            if(keyDownEvent) {

                keyDownEvent = false;

                switch(keypad_get_keycode()) {

                    case CMD_FEED_HOLD:                         //Feed hold TODO: disable when threading? In grbl?
                        serialPutC(CMD_FEED_HOLD);
                        break;

                    case CMD_CYCLE_START:                       // Cycle start
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
            UILibLabelDisplay(lblPass, "Syncing...");
            rqdly = 20;
            grblReady = false;
            awaitResponse = 5; // Try 5 times (5 x 20 x 10ms = 1 second) to see if grbl is responsive
            setGrblTransmitCallback(checkGRBL);
            grblSendSerial(""); // Send empty string to trigger response
            break;

        case EventWidgetClose:
            UILibLabelDisplay(lblXPos, "");
            UILibLabelDisplay(lblZPos, "");
            setGrblTransmitCallback(NULL);
            setKeyclickCallback(NULL, false);
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
        canvasSender = UILibCanvasCreate(0, 0, 320, 240, handlerCanvas);

        lblPass = UILibLabelCreate((Widget *)canvasSender, font_23x16, White, 10, PASSROW, 300, NULL);

        lblXPos = UILibLabelCreate((Widget *)canvasSender, font_23x16, White, 40, XROW, 100, NULL);
        lblXPos->widget.flags.alignment = Align_Right;

        lblZPos = UILibLabelCreate((Widget *)canvasSender, font_23x16, White, 40, ZROW, 100, NULL);
        lblZPos->widget.flags.alignment = Align_Right;

        btnCancel = UILibButtonCreate((Widget *)canvasSender, 35, 130, "Cancel", handlerCancel);
        UILibWidgetSetWidth((Widget *)btnCancel, 250);
    }

    canvasPrevious = UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasSender);
}
