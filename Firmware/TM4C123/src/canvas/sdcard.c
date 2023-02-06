/*
 * canvas/sd_card.c - SD card file selection menu
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2022-01-29 / (c)Io Engineering / Terje
 */

/*

Copyright (c) 2022, Terje Io
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

#include <stdio.h>
#include <string.h>

#include "../fonts.h"
#include "../grblcomm.h"
#include "../interface.h"
#include "../UILib/uilib.h"

#define LISTELEMENTS 8

static bool exit = false;
static uint_fast16_t list_index = 0;
static Canvas *canvasSDCard = 0, *canvasPrevious;
static List *listPrograms;
static sd_files_t *sd_files = NULL;
static sd_file_t *sd_file = NULL;

static inline int32_t max (int32_t a, int32_t b)
{
	return a > b ? a : b;
}

static sd_file_t *get_file (uint_fast16_t idx)
{
    if(sd_files == NULL || sd_files->num_files == 0)
        return NULL;

    if(idx == 0)
        return sd_files->files;

    sd_file_t *file = sd_files->files;

    if(idx < sd_files->num_files) {
        if(file) do {
            file = file->next;
        } while(--idx && file);
    }

    return file;
}

static bool listRefresh (int index)
{
	Widget *element = listPrograms->widget.firstChild;

	if(sd_files == NULL || (index < 0 || index > max(0, sd_files->num_files - LISTELEMENTS)))
		return false;

	index = max(0, index); //??
    list_index = index;

	while(element) {
		sd_file                    = get_file(index);
		element->privateData       = sd_file;
//		element->flags.selected    = program == (UImode == UIMode_DAB ? DABLastPlayed : presetLastPlayed);
		element->flags.highlighted = false;
		UILibButtonSetLabel((Button *)element, sd_file ? sd_file->name + 1 : "");
		element = element->nextSibling;
		index++;
	}

	return true;
}

/*
 * Event handlers
 *
 */

static void on_sd_files_received (sd_files_t *files)
{
    sd_files = files;

    listRefresh(0);
}

static void handlerSelectList (Widget *self, Event *event)
{
    sd_file_t *sd_file = (sd_file_t *)self->privateData;

	if(sd_file && event->reason == EventPointerUp) {
        char run[80] = "$F=";
        strcat(run, sd_file->name);
        grblSendSerial(run);
        UILibCanvasDisplay(canvasPrevious);
	}
}

static void handlerProgramList (Widget *self, Event *event)
{
	switch(event->reason) {

		case EventListOffStart:
            listRefresh(list_index - 1);
			event->claimed = true;
			break;

		case EventListOffEnd:
			listRefresh(list_index + 1);
			event->claimed = true;
			break;
	}
}

static void handlerCanvas (Widget *self, Event *event)
{
    switch(event->reason) {

        case EventNullEvent:
            if(exit)
                UILibCanvasDisplay(canvasPrevious);
            break;
    }
}

static void keyEvent (bool keyDown, char key)
{
    exit = !keyDown;
}

/*
 *  end event handlers
 *
 */

/*
 * Public functions
 *
 */

void SDCardShowCanvas (Canvas *previous)
{
    if(!canvasSDCard) {

        canvasSDCard = UILibCanvasCreate(0, 0, 320, 240, handlerCanvas);
		listPrograms = UILibListCreate((Widget *)canvasSDCard, 10, 40, 300, LISTELEMENTS, handlerProgramList);
		uint_fast8_t i;
		for(i = 0; i < LISTELEMENTS; i++)
			UILibListCreateElement(listPrograms, i, "", handlerSelectList);
    }

    exit = false;
    canvasPrevious = previous; //UILibCanvasGetCurrent();

    UILibCanvasDisplay(canvasSDCard);

    setColor(White);
    drawStringAligned(font_23x16, 0, 22, "SD Card files", Align_Center, 320, false);
    drawStringAligned(font_23x16, 10, 239, "Press any key to exit.", Align_Left, 320, false);

    grblGetSDFiles(on_sd_files_received);

    setKeyclickCallback2(keyEvent, false);
}
