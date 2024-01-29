/*
 * msp430.c - HAL driver for Raspberry RP2040 ARM processor
 *
 * Part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.1 / 2024-01-29 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2024, Terje Io
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

#include "../msp430/JTAGfunc430.h"           // JTAG functions
#include "../msp430/LowLevelFuncRP2040.h"    // low level functions
#include "../msp430/Devices430.h"            // holds Device specific information
#include "msp430_firmware.h"              // holds Keypad Controller firmware

bool flashKeypadController (void)
{
    bool ok;

    InitController();                           // Initialize the RP2040 host for SBW
    InitTarget();                               // Initialize target board

    if(GetDevice() != STATUS_OK)                // Set DeviceId
        return false;                           // stop here if invalid JTAG ID or time-out.

    if(!(ok = ReadMem(F_WORD, 0x1080) == 2)) {  // Check firmware version in infoB
        EraseFLASH(ERASE_SGMT, 0x1080);         // Mass-Erase InfoB
        EraseFLASH(ERASE_MAIN, 0xC000);         // Mass-Erase Flash
        if(!EraseCheck(0xC000, 0x2000)) {
            ReleaseDevice(V_RESET);
            return false;
        }
    }

    if(!ok)
        ok = WriteFLASHallSections(eprom, eprom_address, eprom_length_of_sections, eprom_sections);

    ReleaseDevice(V_RESET);

    return ok;
}
