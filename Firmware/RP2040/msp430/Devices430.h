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
| Devices430.h                                                               |
|                                                                            |
| Device Function Prototypes and Definitions for FLASH programming.          |
|                                                                            |
|----------------------------------------------------------------------------|
| Project:              MSP430 Replicator                                    |
| Developed using:      IAR Embedded Workbench 6.20                          |
|             and:      Code Composer Studio 6.0                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 02/06 LUT         Initial version.                                     |
| 1.1 04/07 WLUT        Enhanced with adress ranges for all actual silica    |
| 1.2 06/12 RL          Updated commentaries                                 |
|----------------------------------------------------------------------------|
| Designed 2006 by Texas Instruments Germany                                 |
\*==========================================================================*/
//! \file Devices430.h
//! \brief Device Function Prototypes and Definitions for FLASH programming.
/****************************************************************************/
/* Defines                                                                  */
/****************************************************************************/

#ifndef _DEVICES_H_
#define _DEVICES_H_

#ifndef __BYTEWORD__
#define __BYTEWORD__
typedef unsigned int  word;
typedef unsigned char byte;
#endif

#include <stdbool.h>

//typedef unsigned char  bool;
#define TRUE          1
#define FALSE         0

// Constants for flash erasing modes
//! \brief Constant for flash erase: main & info of ALL      mem arrays
#define ERASE_GLOB                 0xA50E
//! \brief Constant for flash erase: main        of ALL      mem arrays
#define ERASE_ALLMAIN              0xA50C
//! \brief Constant for flash erase: main & info of SELECTED mem arrays
#define ERASE_MASS                 0xA506
//! \brief Constant for flash erase: main        of SELECTED mem arrays
#define ERASE_MAIN                 0xA504
//! \brief Constant for flash erase: SELECTED segment
#define ERASE_SGMT                 0xA502

/****************************************************************************/
/* Function prototypes                                                      */
/****************************************************************************/

void SetDevice(word wDeviceId);
bool DeviceHas_TestPin(void);
bool DeviceHas_CpuX(void);
bool DeviceHas_DataQuick(void);
bool DeviceHas_FastFlash(void);
bool DeviceHas_EnhVerify(void);
bool DeviceHas_JTAG(void);
bool DeviceHas_SpyBiWire(void);
word Device_RamStart(void);
word Device_RamEnd(void);
word Device_MainStart(void);

#endif
