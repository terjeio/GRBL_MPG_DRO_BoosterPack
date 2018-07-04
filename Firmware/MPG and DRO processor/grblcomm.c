/*
 * grblcomm.c - collects and dispatches lines of data from grbls serial output stream
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

#include <string.h>

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

static char line[256];
static bool flush = false;
static volatile bool await = false;
static void (*grblReceiveCallback)(char *string) = 0;
static void (*grblTransmitCallback)(bool ok, char *string) = 0;

void setGrblReceiveCallback (void (*fn)(char *line))
{
    grblReceiveCallback = fn;
    grblTransmitCallback = 0;
}

static void processReply (char *line)
{
    grblTransmitCallback(strcmp(line, "ok") == 0, line);
}

void setGrblTransmitCallback (void (*fn)(bool ok, char *line))
{
    grblTransmitCallback = fn;
    grblReceiveCallback = fn ? processReply : 0;
}

void grblSerialFlush (void)
{
    flush = true;
}

void grblPollSerial (void) {

    static int_fast16_t c;
    static uint_fast16_t char_counter = 0;

    if((c = serialGetC()) != SERIAL_NO_DATA) {

        if(((c == '\n') || (c == '\r'))) { // End of line reached

            if(char_counter > 0) {

                line[line[0] == '<' || line[0] =='[' ? char_counter - 1 : char_counter] = '\0'; // strip last character from long messages

                if(grblReceiveCallback && !flush) {
                    flush = false;
                    grblReceiveCallback(line);
                }

                char_counter = 0;

            }

        } else if(char_counter < sizeof(line))
            line[char_counter++] = (char)c; // Upcase lowercase??

    }
}

void grblSendSerial (char *line)
{
    serialWriteLn(line);
}

bool grblParseState (char *state, grbl_t *grbl)
{
    bool changed;
    uint_fast8_t len= strlen(state);;

    if((changed = len < sizeof(grbl->state_text) && strncmp(grbl->state_text, state, len))) {
        uint_fast8_t i = 0;
        while(i < NUMSTATES) {
            if(!strcmp(state, grblState[i])) {
                grbl->state = (grbl_state_t)i;
                break;
            }
            i++;
        }
        strcpy(grbl->state_text, state);
    }

    return changed;
}
