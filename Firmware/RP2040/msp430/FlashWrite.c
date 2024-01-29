/*
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
/*==========================================================================*\
|                                                                            |
| FlashWrite.c                                                               |
|                                                                            |
| Funclet for Flash write operations. This binary code is placed in the      |
| target device's RAM to speed up flash write operations.                    |
| The actual source code has to be compiled separately and converted using   |
| SRecord. The procedure is described in the slau320.pdf documentation.      |
|----------------------------------------------------------------------------|
| Project:              MSP430 Replicator                                    |
| Developed using:      IAR Embedded Workbench 6.20                          |
|             and:      Code Composer Studio 6.0                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 --/-- FB         Initial version.                                      |
|----------------------------------------------------------------------------|
| Designed by Texas Instruments Germany                                      |
\*==========================================================================*/
//! \file FlashWrite.c
//! \brief Funclet for Flash write operations

//! \brief Holds the target code for an flash write operation
//! \details This code is modified by the flash write function depending on it's parameters.
unsigned short FlashWrite_o[] =
{
0x001C, 0x00EE, 0xBEEF, 0xDEAD, 0xBEEF, 0xDEAD, 0xA508, 0xA508, 0xA500,
0xA500, 0xDEAD, 0x000B, 0xDEAD, 0x000B, 0x40B2, 0x5A80, 0x015C, 0x40B2,
0xABAD, 0x018E, 0x40B2, 0xBABE, 0x018C, 0x4290, 0x0140, 0xFFDE, 0x4290,
0x0144, 0xFFDA, 0x180F, 0x4AC0, 0xFFD6, 0x180F, 0x4BC0, 0xFFD4, 0xB392,
0x0144, 0x23FD, 0x4092, 0xFFBE, 0x0144, 0x4290, 0x0144, 0xFFB8, 0x90D0,
0xFFB2, 0xFFB2, 0x2406, 0x401A, 0xFFAA, 0xD03A, 0x0040, 0x4A82, 0x0144,
0x1F80, 0x405A, 0xFF94, 0x1F80, 0x405B, 0xFF92, 0x40B2, 0xA540, 0x0140,
0x40B2, 0x0050, 0x0186, 0xB392, 0x0186, 0x27FD, 0x429A, 0x0188, 0x0000,
0xC392, 0x0186, 0xB392, 0x0144, 0x23FD, 0x1800, 0x536A, 0x1800, 0x835B,
0x23F0, 0x1F80, 0x405A, 0xFF6C, 0x1F80, 0x405B, 0xFF6A, 0xE0B0, 0x3300,
0xFF5C, 0xE0B0, 0x3300, 0xFF58, 0x4092, 0xFF52, 0x0140, 0x4092, 0xFF4E,
0x0144, 0x4290, 0x0144, 0xFF42, 0x90D0, 0xFF42, 0xFF3C, 0x2406, 0xD0B0,
0x0040, 0xFF38, 0x4092, 0xFF34, 0x0144, 0x40B2, 0xCAFE, 0x018E, 0x40B2,
0xBABE, 0x018C, 0x3FFF,
};
unsigned long FlashWrite_o_termination = 0x00000000;
unsigned long FlashWrite_o_start       = 0x00005C00;
unsigned long FlashWrite_o_finish      = 0x00005CF0;
unsigned long FlashWrite_o_length      = 0x000000F0;

#define FLASHWRITE_O_TERMINATION 0x00000000
#define FLASHWRITE_O_START       0x00005C00
#define FLASHWRITE_O_FINISH      0x00005CF0
#define FLASHWRITE_O_LENGTH      0x000000F0
