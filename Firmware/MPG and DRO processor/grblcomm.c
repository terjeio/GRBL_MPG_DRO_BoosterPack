/*
 * grblcomm.c - collects, parses and dispatches lines of data from grbls serial output stream
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2018-09-04 / ©Io Engineering / Terje
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "grblcomm.h"

#define SERIAL_NO_DATA -1
#define NUMSTATES 13

// This array must match the grbl_state_t enum above!
const char *const grblState[NUMSTATES] = {
    "Unknown",
    "Idle",
    "Run",
    "Jog",
    "Hold:0",
    "Hold:1",
    "Alarm",
    "Check",
    "Door:0",
    "Door:1",
    "Door:2",
    "Door:3",
    "Tool"
};

// This array must match the grbl_state_t enum above!
const RGBColor_t grblStateColor[NUMSTATES] = {
    Black,
    White,
    LightGreen,
    Yellow,
    Coral,
    Coral,
    Red,
    Yellow,
    Coral,
    Coral,
    Coral,
    Coral,
    Yellow
};

static grbl_data_t grbl_data = {
    .changed          = 0xFF,
    .position         = {0.0f, 0.0f, 0.0f},
    .offset           = {0.0f, 0.0f, 0.0f},
    .absDistance      = true,
    .grbl.state       = Unknown,
    .grbl.state_color = White,
    .grbl.state_text  = "",
    .mpgMode          = false,
    .pins             = "",
    .useWPos          = false,
    .awaitWCO         = true,
    .spindle.on       = false,
    .spindle.ccw      = false,
    .coolant.mist     = false,
    .coolant.flood    = false
};

static bool flush = false;
static volatile bool await = false;
static void (*grblReceiveCallback)(char *string) = 0;
static void (*grblTransmitCallback)(bool ok, grbl_data_t *grbl_data) = 0;

grbl_data_t *setGrblReceiveCallback (void (*fn)(char *line))
{
    grblReceiveCallback = fn;
    grblTransmitCallback = 0;
    grbl_data.changed.flags = 0xFF;

    return &grbl_data;
}

static void processReply (char *line)
{
    grblTransmitCallback(strcmp(line, "ok") == 0, &grbl_data);
}

void setGrblTransmitCallback (void (*fn)(bool ok, grbl_data_t *grbl_data))
{
    grblTransmitCallback = fn;
    grblReceiveCallback = fn ? processReply : 0;
}

void grblSerialFlush (void)
{
    flush = true;
}

static bool parseDecimal (float *value, char *data)
{
    bool changed;

    float val = (float)atof(data);
    if((changed = val != *value))
        *value = val;

    return changed;
}

static void parsePositions (char *data)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.position[X_AXIS], data))
        grbl_data.changed.xpos = true;

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.position[Y_AXIS], data))
        grbl_data.changed.ypos = true;

    if(parseDecimal(&grbl_data.position[Z_AXIS], next))
        grbl_data.changed.zpos = true;
}

static void parseOffsets (char *data)
{

    if(grbl_data.useWPos) {

        grbl_data.offset[X_AXIS] =
        grbl_data.offset[Y_AXIS] =
        grbl_data.offset[Z_AXIS] = 0.0f;

    } else {

        char *next;

        next = strchr(data, ',');
        *next++ = '\0';

        if(parseDecimal(&grbl_data.offset[X_AXIS], data))
            grbl_data.changed.offset = true;

        data = next;
        next = strchr(data, ',');
        *next++ = '\0';

        if(parseDecimal(&grbl_data.offset[Y_AXIS], data))
            grbl_data.changed.offset = true;

        if(parseDecimal(&grbl_data.offset[Z_AXIS], next))
            grbl_data.changed.offset = true;
    }

    grbl_data.awaitWCO = false;
}

static void parseFeedSpeed (char *data)
{
    char *next;

    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.feed_rate, data))
        grbl_data.changed.feed = true;

    data = next;
    next = strchr(data, ',');
    *next++ = '\0';

    if(parseDecimal(&grbl_data.spindle.rpm_programmed, data))
        grbl_data.changed.rpm = true;

    if(parseDecimal(&grbl_data.spindle.rpm_actual, next))
        grbl_data.changed.rpm = true;
}

static void parseData (char *block)
{
    static char buf[255];

    uint32_t c;
    char *line = &buf[0];

    strcpy(line, block);

    if(line[0] == '<') {

        line = strtok(&line[1], "|");

        if(line) {

            if(grblParseState(line, &grbl_data.grbl))
                grbl_data.changed.state = true;

            line = strtok(NULL, "|");
        }

        while(line) {

            if(!strncmp(line, "WPos:", 5)) {
                if(!grbl_data.useWPos) {
                    grbl_data.useWPos = true;
                    grbl_data.changed.offset = true;
                }
                parsePositions(line + 5);

            } else if(!strncmp(line, "MPos:", 5)) {
                if(grbl_data.useWPos) {
                    grbl_data.useWPos = false;
                    grbl_data.changed.offset = true;
                }
                parsePositions(line + 5);

            } else if(!strncmp(line, "FS:", 3))
                parseFeedSpeed(line + 3);

            else if(!strncmp(line, "WCO:", 4))
                parseOffsets(line + 4);

            else if(!strncmp(line, "Pn:", 3))
                strcpy(grbl_data.pins, line + 3);

            else if(!strncmp(line, "A:", 2)) {

                line = &line[2];
                grbl_data.spindle.on =
                grbl_data.spindle.ccw =
                grbl_data.coolant.flood =
                grbl_data.coolant.mist = false;
                grbl_data.changed.leds = true;

                while((c = *line++)) {
                    switch(c) {

                        case 'M':
                            grbl_data.coolant.mist = true;
                            break;

                        case 'F':
                            grbl_data.coolant.flood = true;
                           break;

                        case 'S':
                        case 'C':
                           grbl_data.spindle.on = true;
                           break;
                    }
                }

            } else if(!strncmp(line, "MPG:", 4)) {
                if(grbl_data.mpgMode != (line[4] == '1')) {
                    grbl_data.mpgMode = !grbl_data.mpgMode;
                    grbl_data.changed.mpg = true;
                }
            }

            line = strtok(NULL, "|");
        }

    } else if(!strncmp(line, "[GC:", 4)) {

        line = strtok(&line[4], " ");

        while(line) {

            if(!strncmp(line, "F", 1) && parseDecimal(&grbl_data.feed_rate, line + 1))
                grbl_data.changed.feed = true;

            if(!strncmp(line, "S", 1) && parseDecimal(&grbl_data.spindle.rpm_programmed, line + 1))
                grbl_data.changed.rpm = true;

            if(!strncmp(line, "G90", 3) && !grbl_data.absDistance) {
                grbl_data.absDistance = true;
                grbl_data.changed.dist = true;
            }

            if(!strncmp(line, "G91", 3) && grbl_data.absDistance) {
                grbl_data.absDistance = false;
                grbl_data.changed.dist = true;
            }

            if(!strncmp(line, "M5", 2)) {
                if(grbl_data.spindle.on)
                    grbl_data.changed.leds = true;
                grbl_data.spindle.on = false;
            } else if(!strncmp(line, "M3", 2)) {
                if(!grbl_data.spindle.on || grbl_data.spindle.ccw)
                    grbl_data.changed.leds = true;
                grbl_data.spindle.on = true;
                grbl_data.spindle.ccw = false;
            } else if(!strncmp(line, "M4", 2)) {
                if(!grbl_data.spindle.on || !grbl_data.spindle.ccw)
                    grbl_data.changed.leds = true;
                grbl_data.spindle.on = true;
                grbl_data.spindle.ccw = true;
            }

            if(!strncmp(line, "M9", 2)) {
                if(grbl_data.coolant.mist || grbl_data.coolant.flood)
                    grbl_data.changed.leds = true;
                grbl_data.coolant.mist = false;
                grbl_data.coolant.flood = false;
            } else {
                if(!strncmp(line, "M7", 2)) {
                    if(!grbl_data.coolant.mist)
                        grbl_data.changed.leds = true;
                    grbl_data.coolant.mist = true;
                }
                if(!strncmp(line, "M8", 2)) {
                    if(!grbl_data.coolant.mist)
                        grbl_data.changed.leds = true;
                    grbl_data.coolant.flood = true;
                }
            }
            line = strtok(NULL, " ");
        }
    } else if(!strncmp(line, "[MSG:", 5))
        grbl_data.changed.msg = true;
    else if(!strncmp(line, "error:", 6) || !strncmp(line, "ALARM:", 6))
        grbl_data.changed.msg = true;
}

void grblPollSerial (void) {

    static int_fast16_t c;
    static uint_fast16_t char_counter = 0;

    if((c = serialGetC()) != SERIAL_NO_DATA) {

        if(((c == '\n') || (c == '\r'))) { // End of block reached

            if(char_counter > 0) {

                grbl_data.block[grbl_data.block[0] == '<' || grbl_data.block[0] =='[' ? char_counter - 1 : char_counter] = '\0'; // strip last character from long messages

                parseData(grbl_data.block);

                if(grblReceiveCallback && !flush) {
                    flush = false;
                    grblReceiveCallback(grbl_data.block);
                }

                char_counter = 0;

            }

        } else if(char_counter < sizeof(grbl_data.block))
            grbl_data.block[char_counter++] = (char)c; // Upcase lowercase??

    }
}

void grblSendSerial (char *line)
{
    serialWriteLn(line);
}

bool grblParseState (char *data, grbl_t *grbl)
{
    bool changed = false;
    uint_fast8_t len = strlen(data);;

    if(len < sizeof(grbl->state_text) && strncmp(grbl->state_text, data, len)) {
        uint_fast8_t state = 0;
        while(state < NUMSTATES) {
            if((changed = !strcmp(data, grblState[state]))) {
                grbl->state = (grbl_state_t)state;
                grbl->state_color = grblStateColor[state];
                strcpy(grbl->state_text, data);
                break;
            }
            state++;
        }
    }

    return changed;
}
