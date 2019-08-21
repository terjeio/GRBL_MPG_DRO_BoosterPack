/*
 * boot.c - boot screen
 *
 * part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2019-08-20 / ©Io Engineering / Terje
 */

/*

Copyright (c) 2015-2019, Terje Io
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
#include <stdbool.h>

#include "boot.h"
#include "fonts.h"
#include "signals.h"
#include "resources/elogo.h"
#include "uilib/uilib.h"

char const ioEngineering[] = "©2018-2019 Io Engineering", version[] = "v0.01 - 2019-08-20";

static Canvas *canvasBoot = NULL;

/*
 * Event handlers
 *
 */

/*
 *  end event handlers
 *
 */

/*
 * Public functions
 *
 */

void BOOTShowCanvas (lcd_display_t *screen)
{
    uint32_t x = 15, timeout = 0;

    if(!canvasBoot) {
		canvasBoot = UILibCanvasCreate(0, 0, screen->Width, screen->Height, NULL);
		canvasBoot->widget.bgColor = Black;

		Image *logo = UILibImageCreate((Widget *)canvasBoot, 35, 24, 248, 56, (void *)engineering, NULL);
		logo->widget.fgColor = White;
        logo->widget.bgColor = SeaGreen;

        signalsInit();
    }

	UILibCanvasDisplay(canvasBoot);

	drawStringAligned(font_freepixel_9x17, 0, 190, ioEngineering, Align_Center, screen->Width, false);
	drawStringAligned(font_freepixel_9x17, 0, 205, version, Align_Center, screen->Width, false);

    delay(300); // Wait a bit for grbl card to power up

	// Wait for grbl to signal ready status
	// (MPG mode pin pulled high)
    while(signalIsMPGMode()) {

        delay(5);

        if(!(timeout % 50)) {
            drawString(font_23x16, x, 220,  ".", false);
            x += 8;
            if(x > 310) {
                setColor(canvasBoot->widget.bgColor);
                fillRect(5, 210, 315, 220);
                setColor(canvasBoot->widget.fgColor);
                x = 15;
            }
        }
        timeout++;
    }

	if(timeout < 500)
	    delay(500 - timeout);
}
