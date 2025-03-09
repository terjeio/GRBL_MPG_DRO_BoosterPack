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
| JTAGfunc430.c                                                              |
|                                                                            |
| JTAG Control Sequences for Erasing / Programming / Fuse Burning            |
|----------------------------------------------------------------------------|
| Project:              JTAG Functions                                       |
| Developed using:      IAR Embedded Workbench 6.20                          |
|             and:      Code Composer Eessentials 6.0                        |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 04/02 FRGR        Initial version.                                     |
| 1.1 04/02 ALB2        Formatting changes, added comments.                  |
| 1.2 08/02 ALB2        Initial code release with Lit# SLAA149.              |
| 1.3 09/05 JDI         'ResetTAP': added SetTDI for fuse check              |
|                       search for F2xxx to find respective modifications in |
|                       'SetPC', 'HaltCPU', 'VerifyPSA', 'EraseFLASH'        |
|                       'WriteFLASH'                                         |
|           SUN1        Software delays redesigned to use TimerA harware;    |
|                       see MsDelay() routine.                               |
| 1.4 01/06 STO         Added entry sequence for SpyBiWire devices           |
|                       Minor cosmetic changes                               |
| 1.5 03/06 STO         BlowFuse() make correct fuse check after blowing.    |
| 1.6 07/06 STO         Loop in WriteFLASH() changed.                        |
| 1.7 04/06 WLUT        'VerifyPSA', 'ReadMemQuick' changed to TCLK high.    |
|                       JTAG4 sequence changed according to figure 10 in     |
|                       Lit# SLAA149.                                        |
|                       WriteFLASHallSections changed due to spec of srec_cat|
|                       Renamed 'ExecutePUC' to 'ExecutePOR'.                |
| 1.8 01/08 WLUT        Added usDelay(5) in ResetTAP() to ensure at least    |
|                       5us low phase of TMS during JTAG fuse check.         |
| 1.9 08/08 WLUT        Added StartJtag() and StopJtag() for a clean init    |
|                       sequence.                                            |
| 2.0 07/09 FB          Added support for Spy-Bi-Wire and new replecator     |
| 2.1 06/12 RL/GC (Elprotronic) Updated commentaries/Added Fuse Blow via SBW |
| 2.2 03/13 RL/MD       Added unlock function for Info A                     |
| 2.3 07/13 RL          Fixed unlock function for Info A                     |
| 2.4 02/14 RL          Fixed flow in SetPC()                                |
| 2.5 04/02 RL          Updated SBW entry sequence to support i2040          |
|----------------------------------------------------------------------------|
| Designed 2002 by Texas Instruments Germany                                 |
\*==========================================================================*/
//! \file JTAGfunc430.c
//! \brief JTAG Control Sequences for Erasing / Programming / Fuse Burning
/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "JTAGfunc430.h"
#include "LowLevelFuncRP2040.h"
#include "Devices430.h"

/****************************************************************************/
/* Low level routines for accessing the target device via JTAG:             */
/****************************************************************************/

//----------------------------------------------------------------------------
//! \brief Function for shifting a given 16-bit word into the JTAG data
//! register through TDI.
//! \param[in] word data (16-bit data, MSB first)
//! \return word (value is shifted out via TDO simultaneously)
word DR_Shift16(word data)
{
#ifdef SPYBIWIRE_MODE
  
    // JTAG FSM state = Run-Test/Idle
    if (TCLK_saved)
    {
        TMSH_TDIH();
    }
    else
    {
        TMSH_TDIL();
    }
    // JTAG FSM state = Select DR-Scan
    TMSL_TDIH();
    // JTAG FSM state = Capture-DR
    TMSL_TDIH();
#else
    // JTAG FSM state = Run-Test/Idle
    SetTMS();
    ClrTCK();
    SetTCK();

    // JTAG FSM state = Select DR-Scan
    ClrTMS();
    ClrTCK();
    SetTCK();
    // JTAG FSM state = Capture-DR
    ClrTCK();
    SetTCK();
#endif
    // JTAG FSM state = Shift-DR, Shift in TDI (16-bit)
    return(Shift(F_WORD, data));
    // JTAG FSM state = Run-Test/Idle

}

//----------------------------------------------------------------------------
//! \brief Function for shifting a new instruction into the JTAG instruction
//! register through TDI (MSB first, but with interchanged MSB - LSB, to
//! simply use the same shifting function, Shift(), as used in DR_Shift16).
//! \param[in] byte Instruction (8bit JTAG instruction, MSB first)
//! \return word TDOword (value shifted out from TDO = JTAG ID)
word IR_Shift(byte instruction)
{
#ifdef SPYBIWIRE_MODE
    // JTAG FSM state = Run-Test/Idle
    if (TCLK_saved)
    {
        TMSH_TDIH();
    }
    else
    {
        TMSH_TDIL();
    }
    // JTAG FSM state = Select DR-Scan
    TMSH_TDIH();

    // JTAG FSM state = Select IR-Scan
    TMSL_TDIH();
    // JTAG FSM state = Capture-IR
    TMSL_TDIH();
#else
    // JTAG FSM state = Run-Test/Idle
    SetTMS();
    ClrTCK();
    SetTCK();
    // JTAG FSM state = Select DR-Scan
    ClrTCK();
    SetTCK();

    // JTAG FSM state = Select IR-Scan
    ClrTMS();
    ClrTCK();
    SetTCK();
    // JTAG FSM state = Capture-IR
    ClrTCK();
    SetTCK();
#endif
    // JTAG FSM state = Shift-IR, Shift in TDI (8-bit)
    return(Shift(F_BYTE, instruction));
    // JTAG FSM state = Run-Test/Idle
}

//----------------------------------------------------------------------------
//! \brief Reset target JTAG interface and perform fuse-HW check.
void ResetTAP(void)
{
    word i;
    
#ifdef SPYBIWIRE_MODE
    // Reset JTAG FSM
    for (i = 6; i > 0; i--)
    {
        TMSH_TDIH();
    }
    // JTAG FSM is in Test-Logic-Reset now
    TMSL_TDIH();                 
    
    // JTAG FSM is in Run-Test/IDLE

    // Perform fuse check
    TMSH_TDIH();
    TMSL_TDIH();
    TMSH_TDIH();
    TMSL_TDIH();
    TMSH_TDIH();
    // In every TDI slot a TCK for the JTAG machine is generated.
    // Thus we need to get TAP in Run/Test Idle state back again.
    TMSH_TDIH();
    TMSL_TDIH();                // now in Run/Test Idle
#else
    // process TDI first to settle fuse current
    SetTDI();
    SetTMS();
    SetTCK();

    // Reset JTAG FSM
    for (i = 6; i > 0; i--)
    {
        ClrTCK();
        SetTCK();
    }
    // JTAG FSM is now in Test-Logic-Reset
    ClrTCK();
    ClrTMS();
    SetTCK();
    SetTMS();
    // JTAG FSM is now in Run-Test/IDLE

    // Perform fuse check
    ClrTMS();
    usDelay(5); // at least 5us low required
    SetTMS();
    ClrTMS();
    usDelay(5); // at least 5us low required
    SetTMS();
#endif
}

//----------------------------------------------------------------------------
//! \brief Function to execute a Power-On Reset (POR) using JTAG CNTRL SIG 
//! register
//! \return word (STATUS_OK if target is in Full-Emulation-State afterwards,
//! STATUS_ERROR otherwise)
word ExecutePOR(void)
{
    word JtagVersion;

    // Perform Reset
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2C01);                 // Apply Reset
    DR_Shift16(0x2401);                 // Remove Reset
    ClrTCLK();
    SetTCLK();
    ClrTCLK();
    SetTCLK();
    ClrTCLK();
    JtagVersion = IR_Shift(IR_ADDR_CAPTURE); // read JTAG ID, checked at function end
    SetTCLK();

    WriteMem(F_WORD, 0x0120, 0x5A80);   // Disable Watchdog on target device

    if (JtagVersion != JTAG_ID)
    {
        return(STATUS_ERROR);
    }
    return(STATUS_OK);
}

//----------------------------------------------------------------------------
//! \brief Function to set target CPU JTAG FSM into the instruction fetch state
//! \return word (STATUS_OK if instr. fetch was set, STATUS_ERROR otherwise)
word SetInstrFetch(void)
{
    word i;

    IR_Shift(IR_CNTRL_SIG_CAPTURE);

    // Wait until CPU is in instr. fetch state, timeout after limited attempts
    for (i = 50; i > 0; i--)
    {
        if (DR_Shift16(0x0000) & 0x0080)
        {
            return(STATUS_OK);
        }
        ClrTCLK();
        SetTCLK();
    }
    return(STATUS_ERROR);
}

//----------------------------------------------------------------------------
//! \brief Load a given address into the target CPU's program counter (PC).
//! \param[in] word Addr (destination address)
void SetPC(word Addr)
{
    SetInstrFetch();              // Set CPU into instruction fetch mode, TCLK=1

    // Load PC with address
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x3401);           // CPU has control of RW & BYTE.
    IR_Shift(IR_DATA_16BIT);
    DR_Shift16(0x4030);           // "mov #addr,PC" instruction
    ClrTCLK();
    SetTCLK();                    // F2xxx
    DR_Shift16(Addr);             // "mov #addr,PC" instruction
    ClrTCLK();
    SetTCLK();
    IR_Shift(IR_ADDR_CAPTURE);
    ClrTCLK();                    // Now the PC should be on Addr
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2401);           // JTAG has control of RW & BYTE.
}

//----------------------------------------------------------------------------
//! \brief Function to set the CPU into a controlled stop state
void HaltCPU(void)
{
    SetInstrFetch();              // Set CPU into instruction fetch mode

    IR_Shift(IR_DATA_16BIT);
    DR_Shift16(0x3FFF);           // Send JMP $ instruction
    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2409);           // Set JTAG_HALT bit
    SetTCLK();
}

//----------------------------------------------------------------------------
//! \brief Function to release the target CPU from the controlled stop state
void ReleaseCPU(void)
{
    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2401);           // Clear the HALT_JTAG bit
    IR_Shift(IR_ADDR_CAPTURE);
    SetTCLK();
}
#ifdef SPYBIWIRE_MODE
//----------------------------------------------------------------------------
//! \brief This function compares the computed PSA (Pseudo Signature Analysis)
//! value to the PSA value shifted out from the target device.
//! It is used for very fast data block write or erasure verification.
//! \param[in] unsigned long StartAddr (Start address of data block to be checked)
//! \param[in] unsigned long Length (Number of words within data block)
//! \param[in] word *DataArray (Pointer to array with the data, 0 for Erase Check)
//! \return word (STATUS_OK if comparison was successful, STATUS_ERROR otherwise)
static word VerifyPSA(word StartAddr, word Length, word *DataArray)
{
    word TDOword, i;
    word POLY = 0x0805;           // Polynom value for PSA calculation
    word PSA_CRC = StartAddr-2;   // Start value for PSA calculation

    ExecutePOR();

    if(DeviceHas_EnhVerify())
    {
        SetPC(StartAddr-4);
        HaltCPU();
        ClrTCLK();
        IR_Shift(IR_DATA_16BIT);
        DR_Shift16(StartAddr-2);
    }
    else
    {
        SetPC(StartAddr-2);
        SetTCLK();
        ClrTCLK();
    }
    IR_Shift(IR_DATA_PSA);

    for (i = 0; i < Length; i++)
    {
        // Calculate the PSA (Pseudo Signature Analysis) value
        if ((PSA_CRC & 0x8000) == 0x8000)
        {
            PSA_CRC ^= POLY;
            PSA_CRC <<= 1;
            PSA_CRC |= 0x0001;
        }
        else
        {
            PSA_CRC <<= 1;
        }
        // if pointer is 0 then use erase check mask, otherwise data
        if (DataArray == 0)
        {
            PSA_CRC ^= 0xFFFF;
        }
        else
        {
            PSA_CRC ^= DataArray[i];
        }

        // Clock through the PSA
        SetTCLK();

        TMSH_TDIH();
        TMSL_TDIH();
        TMSL_TDIH();
        TMSH_TDIH();
        TMSH_TDIH();
        TMSL_TDIH();
        
        ClrTCLK();
    }
    IR_Shift(IR_SHIFT_OUT_PSA);
    TDOword = DR_Shift16(0x0000);   // Read out the PSA value
    SetTCLK();

    if(DeviceHas_EnhVerify())
    {
       ReleaseCPU();
    }

    ExecutePOR();

    return((TDOword == PSA_CRC) ? STATUS_OK : STATUS_ERROR);
}
#else

//! \brief This function compares the computed PSA (Pseudo Signature Analysis)
//! value to the PSA value shifted out from the target device.
//! It is used for very fast data block write or erasure verification.
//! \param[in] unsigned long StartAddr (Start address of data block to be checked)
//! \param[in] unsigned long Length (Number of words within data block)
//! \param[in] word *DataArray (Pointer to array with the data, 0 for Erase Check)
//! \return word (STATUS_OK if comparison was successful, STATUS_ERROR otherwise)
word VerifyPSA(word StartAddr, word Length, word *DataArray)
{
    word TDOword, i;
    word POLY = 0x0805;           // Polynom value for PSA calculation
    word PSA_CRC = StartAddr-2;   // Start value for PSA calculation

    ExecutePOR();

    if(DeviceHas_EnhVerify())
    {
        SetPC(StartAddr-4);
        HaltCPU();
        ClrTCLK();
        IR_Shift(IR_DATA_16BIT);
        DR_Shift16(StartAddr-2);
    }
    else
    {
        SetPC(StartAddr-2);
        SetTCLK();
        ClrTCLK();
    }
    IR_Shift(IR_DATA_PSA);
    for (i = 0; i < Length; i++)
    {
        // Calculate the PSA (Pseudo Signature Analysis) value
        if ((PSA_CRC & 0x8000) == 0x8000)
        {
            PSA_CRC ^= POLY;
            PSA_CRC <<= 1;
            PSA_CRC |= 0x0001;
        }
        else
        {
            PSA_CRC <<= 1;
        }
        // if pointer is 0 then use erase check mask, otherwise data
        &DataArray[0] == 0 ? (PSA_CRC ^= 0xFFFF) : (PSA_CRC ^= DataArray[i]);

        // Clock through the PSA
        SetTCLK();
//        ClrTCLK();           // set here -> Fixes problem with F123 PSA in RAM

        ClrTCK();

        SetTMS();
        SetTCK();            // Select DR scan
        ClrTCK();
        ClrTMS();

        SetTCK();            // Capture DR
        ClrTCK();

        SetTCK();            // Shift DR
        ClrTCK();

        SetTMS();
        SetTCK();            // Exit DR
        ClrTCK();
        SetTCK();
        ClrTMS();
        ClrTCK();
        SetTCK();
        
        ClrTCLK();           // set here -> future purpose
    }
    IR_Shift(IR_SHIFT_OUT_PSA);
    TDOword = DR_Shift16(0x0000);     // Read out the PSA value
    SetTCLK();

    if(DeviceHas_EnhVerify())
    {
        ReleaseCPU();
    }
    ExecutePOR();
    return((TDOword == PSA_CRC) ? STATUS_OK : STATUS_ERROR);    
}
#endif

/****************************************************************************/
/* High level routines for accessing the target device via JTAG:            */
/*                                                                          */
/* From the following, the user is relieved from coding anything.           */
/* To provide better understanding and clearness, some functionality is     */
/* coded generously. (Code and speed optimization enhancements may          */
/* be desired)                                                              */
/****************************************************************************/
static void CheckJtagFuse_SBW(void)
{
    TMSL_TDIH();    // now in Run/Test Idle
      
    // Fuse check
    TMSH_TDIH();
    TMSL_TDIH();
    TMSH_TDIH();
    TMSL_TDIH();
    TMSH_TDIH();
    // In every TDI slot a TCK for the JTAG machine is generated.
    // Thus we need to get TAP in Run/Test Idle state back again.
    TMSH_TDIH();
    TMSL_TDIH();
}

static void CheckJtagFuse_JTAG(void)
{
    // perform a JTAG fuse check
    SetTMS();_NOP();_NOP();_NOP();
    ClrTMS();_NOP();_NOP();_NOP();
    usDelay(15);
    SetTMS();_NOP();_NOP();_NOP();
    ClrTMS();_NOP();_NOP();_NOP();
    usDelay(15);
    SetTMS();_NOP();_NOP();_NOP();
}

//! \brief Function to start the SBW communication - RST line high - device starts
//! code execution 
static void EntrySequences_RstHigh_SBW()
{
    ClrSBWTCK();    //1
    MsDelay(4); // reset TEST logic

    SetSBWTDIO();    //2

    SetSBWTCK();    //3
    MsDelay(20); // activate TEST logic

    // phase 1
    SetSBWTDIO();    //4
    usDelay(60);

    // phase 2 -> TEST pin to 0, no change on RST pin
    // for Spy-Bi-Wire
    _DINT();
    //(*_Jtag.Out) &= ~_Jtag.TST; //5
    ClrSBWTCK();  

    // phase 3
    usDelay(1);
    // phase 4 -> TEST pin to 1, no change on RST pin
    // for Spy-Bi-Wire
    //(*_Jtag.Out) |= _Jtag.TST;  //7    
    SetSBWTCK();  
    _EINT();
    //_EINT_FET();
    usDelay(60);

    // phase 5
    MsDelay(5);
}

//! \brief Function to start the JTAG communication - RST line high - device starts
//! code execution   
static void EntrySequences_RstHigh_JTAG()
{
    ClrTST();    //1
    MsDelay(4); // reset TEST logic

    SetRST();    //2

    SetTST();    //3
    MsDelay(20); // activate TEST logic

    // phase 1
    ClrRST();    //4
    usDelay(60);

    // phase 2 -> TEST pin to 0, no change on RST pin
    // for 4-wire JTAG clear Test pin
    ClrTST();  //5

    // phase 3
    usDelay(1);

    // phase 4 -> TEST pin to 1, no change on RST pin
    // for 4-wire JTAG
    SetTST();//7
    usDelay(60);

    // phase 5
    SetRST();
    MsDelay(5);
}

//----------------------------------------------------------------------------
//! \brief Function to start the JTAG communication
static word StartJtag(void)
{
    // drive JTAG/TEST signals
    DrvSignals();
    MsDelay(10);             // delay 10ms
    
    if(INTERFACE == SPYBIWIRE_IF)
    {
        EntrySequences_RstHigh_SBW();
    }
    else if(INTERFACE == SPYBIWIREJTAG_IF)
    {
        EntrySequences_RstHigh_JTAG();
    }
    else // JTAG_IF
    {
        SetRST();
        SetTST();
    }
    
    ResetTAP();  // reset TAP state machine -> Run-Test/Idle
    
    if(INTERFACE == SPYBIWIRE_IF)
    {
        CheckJtagFuse_SBW();
    }
    else
    {     
        CheckJtagFuse_JTAG();
    }    
    return IR_Shift(IR_BYPASS);
}

//----------------------------------------------------------------------------
//! \brief Function to stop the JTAG communication
static void StopJtag (void)
{
    // release JTAG/TEST signals
    {
      RlsSignals();
      MsDelay(10);             // delay 10ms
    }
}

//----------------------------------------------------------------------------
//! \brief Function to take target device under JTAG control. Disables the 
//! target watchdog. Sets the global DEVICE variable as read from the target 
//! device.
//! \return word (STATUS_ERROR if fuse is blown, incorrect JTAG ID or
//! synchronizing time-out; STATUS_OK otherwise)
word GetDevice(void)
{
    word JtagId = 0;            // initialize JtagId with an invalid value
    word i;
    for (i = 0; i < MAX_ENTRY_TRY; i++)
    {
      
        StopJtag();               // release JTAG/TEST signals to savely reset the test logic
        JtagId = StartJtag();     // establish the physical connection to the JTAG interface
        if(JtagId == JTAG_ID)     // break if a valid JTAG ID is being returned
        {
            break;
        }
    }
    if(i >= MAX_ENTRY_TRY)
    {
        return(STATUS_ERROR);
    }
                      
    if (IsFuseBlown())                   // Stop here if fuse is already blown
    {
        return(STATUS_FUSEBLOWN);
    }
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2401);                  // Set device into JTAG mode + read
    if (IR_Shift(IR_CNTRL_SIG_CAPTURE) != JTAG_ID)
    {
        return(STATUS_ERROR);
    }

    // Wait until CPU is synchronized, timeout after a limited # of attempts
    for (i = 50; i > 0; i--)
    {
        if (DR_Shift16(0x0000) & 0x0200)
        {
            word DeviceId;
            DeviceId = ReadMem(F_WORD, 0x0FF0);// Get target device type
                                               //(bytes are interchanged)
            DeviceId = (DeviceId << 8) + (DeviceId >> 8); // swop bytes
            //Set Device index, which is used by functions in Device.c
            SetDevice(DeviceId);
            break;
        }
        else
        {
            if (i == 1)
            {
                return(STATUS_ERROR);      // Timeout reached, return false
            }
        }
    }
    if (!ExecutePOR())                     // Perform PUC, Includes
    {
        return(STATUS_ERROR);              // target Watchdog disable.
    }
    return(STATUS_OK);
}

//----------------------------------------------------------------------------
//! \brief Function to release the target device from JTAG control
//! \param[in] word Addr (0xFFFE: Perform Reset, means Load Reset Vector into 
//! PC, otherwise: Load Addr into PC)
void ReleaseDevice(word Addr)
{
    if (Addr == V_RESET)
    {
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2C01);         // Perform a reset
        DR_Shift16(0x2401);
    }
    else
    {
        SetPC(Addr);                // Set target CPU's PC
    }
    IR_Shift(IR_CNTRL_SIG_RELEASE);
}

//----------------------------------------------------------------------------
//! \brief This function writes one byte/word at a given address ( <0xA00)
//! \param[in] word Format (F_BYTE or F_WORD)
//! \param[in] word Addr (Address of data to be written)
//! \param[in] word Data (shifted data)
void WriteMem(word Format, word Addr, word Data)
{
    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    if  (Format == F_WORD)
    {
        DR_Shift16(0x2408);     // Set word write
    }
    else
    {
        DR_Shift16(0x2418);     // Set byte write
    }
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(Addr);           // Set addr
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(Data);           // Shift in 16 bits
    SetTCLK();

    ReleaseCPU();
}

//----------------------------------------------------------------------------
//! \brief This function writes an array of words into the target memory.
//! \param[in] word StartAddr (Start address of target memory)
//! \param[in] word Length (Number of words to be programmed)
//! \param[in] word *DataArray (Pointer to array with the data)
void WriteMemQuick(word StartAddr, word Length, word *DataArray)
{
    word i;

    // Initialize writing:
    SetPC((word)(StartAddr-4));
    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2408);             // Set RW to write
    IR_Shift(IR_DATA_QUICK);
    for (i = 0; i < Length; i++)
    {
        DR_Shift16(DataArray[i]);   // Shift in the write data
        SetTCLK();
        ClrTCLK();                  // Increment PC by 2
    }
    ReleaseCPU();
}

#ifdef SPYBIWIRE_MODE
//----------------------------------------------------------------------------
//! \brief This function programs/verifies an array of words into the FLASH
//! memory by using the FLASH controller.
//! \param[in] word StartAddr (Start address of FLASH memory)
//! \param[in] word Length (Number of words to be programmed)
//! \param[in] word *DataArray (Pointer to array with the data)
void WriteFLASH(word StartAddr, word Length, word *DataArray)
{
    word i;                     // Loop counter
    word addr = StartAddr;      // Address counter
    word FCTL3_val = SegmentInfoAKey;   // SegmentInfoAKey holds Lock-Key for Info
                                        // Seg. A 

    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2408);         // Set RW to write
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x0128);         // FCTL1 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(0xA540);         // Enable FLASH write
    SetTCLK();

    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012A);         // FCTL2 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(0xA540);         // Select MCLK as source, DIV=1
    SetTCLK();

    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012C);         // FCTL3 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(FCTL3_val);      // Clear FCTL3; F2xxx: Unlock Info-Seg.
                                // A by toggling LOCKA-Bit if required,
    SetTCLK();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);

    for (i = 0; i < Length; i++, addr += 2)
    {
        DR_Shift16(0x2408);             // Set RW to write
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(addr);               // Set address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(DataArray[i]);       // Set data
        SetTCLK();
        ClrTCLK();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2409);             // Set RW to read

        TCLKstrobes(35);        // Provide TCLKs, min. 33 for F149 and F449
                                // F2xxx: 29 are ok
    }

    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2408);         // Set RW to write
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x0128);         // FCTL1 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(0xA500);         // Disable FLASH write
    SetTCLK();

    // set LOCK-Bits again
    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012C);         // FCTL3 address
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(FCTL3_val | 0x0010);      // Lock Inf-Seg. A by toggling LOCKA and set LOCK again
    SetTCLK();

    ReleaseCPU();
}
#else

//----------------------------------------------------------------------------
//! \brief This function programs/verifies an array of words into the FLASH
//! memory by using the FLASH controller.
//! \param[in] word StartAddr (Start address of FLASH memory)
//! \param[in] word Length (Number of words to be programmed)
//! \param[in] word *DataArray (Pointer to array with the data)
void WriteFLASH(word StartAddr, word Length, word *DataArray)
{
    word i;                     // Loop counter
    word addr = StartAddr;      // Address counter
    word FCTL3_val = SegmentInfoAKey;   // SegmentInfoAKey holds Lock-Key for Info
                                        // Seg. A 

    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2408);         // Set RW to write
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x0128);         // FCTL1 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(0xA540);         // Enable FLASH write
    SetTCLK();

    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012A);         // FCTL2 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(0xA540);         // Select MCLK as source, DIV=1
    SetTCLK();

    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012C);         // FCTL3 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(FCTL3_val);      // Clear FCTL3; F2xxx: Unlock Info-Seg.
                                // A by toggling LOCKA-Bit if required,
    SetTCLK();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);

    for (i = 0; i < Length; i++, addr += 2)
    {
        DR_Shift16(0x2408);             // Set RW to write
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(addr);               // Set address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(DataArray[i]);       // Set data
        SetTCLK();
        ClrTCLK();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2409);             // Set RW to read

        TCLKstrobes(35);        // Provide TCLKs, min. 33 for F149 and F449
                                // F2xxx: 29 are ok
    }

    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2408);         // Set RW to write
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x0128);         // FCTL1 register
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(0xA500);         // Disable FLASH write
    SetTCLK();

    // set LOCK-Bits again
    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012C);         // FCTL3 address
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(FCTL3_val | 0x0010);      // Lock Inf-Seg. A by toggling LOCKA and set LOCK again
    SetTCLK();

    ReleaseCPU();
}
#endif

//----------------------------------------------------------------------------
//! \brief This function programs/verifies a set of data arrays of words into a FLASH
//! memory by using the "WriteFLASH()" function. It conforms with the
//! "CodeArray" structure convention of file "Target_Code_(IDE).s43" or "Target_Code.h".
//! \param[in] const unsigned int  *DataArray (Pointer to array with the data)
//! \param[in] const unsigned long *address (Pointer to array with the startaddresses)
//! \param[in] const unsigned long *length_of_sections (Pointer to array with the number of words counting from startaddress)
//! \param[in] const unsigned long sections (Number of sections in code file)
//! \return word (STATUS_OK if verification was successful,
//! STATUS_ERROR otherwise)
word WriteFLASHallSections(const unsigned short *data, const unsigned long *address, const unsigned long *length_of_sections, const unsigned long sections)
{
    unsigned int i;
    word *p = (word *)data;

    for(i = 0; i < sections; i++)
        p += length_of_sections[i];      

    i = sections;
    do { // Write/Verify(PSA) one FLASH section
        p -= length_of_sections[--i];      
        WriteFLASH(address[i], length_of_sections[i], p);
        if(!VerifyMem(address[i], length_of_sections[i], p))
           return STATUS_ERROR;
    } while(i);
    
    return STATUS_OK;
}

//----------------------------------------------------------------------------
//! \brief This function reads one byte/word from a given address in memory
//! \param[in] word Format (F_BYTE or F_WORD)
//! \param[in] word Addr (address of memory)
//! \return word (content of the addressed memory location)
word ReadMem(word Format, word Addr)
{
    word TDOword;

    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    if  (Format == F_WORD)
    {
        DR_Shift16(0x2409);         // Set word read
    }
    else
    {
        DR_Shift16(0x2419);         // Set byte read
    }
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(Addr);               // Set address
    IR_Shift(IR_DATA_TO_ADDR);
    SetTCLK();

    ClrTCLK();
    TDOword = DR_Shift16(0x0000);   // Shift out 16 bits

    ReleaseCPU();
    return(Format == F_WORD ? TDOword : TDOword & 0x00FF);
}

//----------------------------------------------------------------------------
//! \brief This function reads an array of words from the memory.
//! \param[in] word StartAddr (Start address of memory to be read)
//! \param[in] word Length (Number of words to be read)
//! \param[out] word *DataArray (Pointer to array for the data)
void ReadMemQuick(word StartAddr, word Length, word *DataArray)
{
    word i;

    // Initialize reading:
    SetPC(StartAddr-4);
    HaltCPU();

    ClrTCLK();
    IR_Shift(IR_CNTRL_SIG_16BIT);
    DR_Shift16(0x2409);                    // Set RW to read
    IR_Shift(IR_DATA_QUICK);

    for (i = 0; i < Length; i++)
    {
        SetTCLK();
        DataArray[i] = DR_Shift16(0x0000); // Shift out the data
                                           // from the target.
        ClrTCLK();
    }
    ReleaseCPU();
}

#ifdef SPYBIWIRE_MODE
//----------------------------------------------------------------------------
//! \brief This function performs a mass erase (with and w/o info memory) or a
//! segment erase of a FLASH module specified by the given mode and address.
//! Large memory devices get additional mass erase operations to meet the spec.
//! (Could be extended with erase check via PSA)
//! \param[in] word Mode (could be ERASE_MASS or ERASE_MAIN or ERASE_SGMT)
//! \param[in] word Addr (any address within the selected segment)
void EraseFLASH(word EraseMode, word EraseAddr)
{
#ifdef ixx_family
    word StrobeAmount = 9628;       // ixx family requires additional TCLKs due
                                    // due to larger segment size
#else
    word StrobeAmount = 4820;       // default for Segment Erase
#endif
    word i, loopcount = 1;          // erase cycle repeating for Mass Erase
    word FCTL3_val = SegmentInfoAKey;    // SegmentInfoAKey holds Lock-Key for Info
                                         // Seg. A

    if ((EraseMode == ERASE_MASS) || (EraseMode == ERASE_MAIN))
    {
        if(DeviceHas_FastFlash())
        {
            StrobeAmount = 10600;        // Larger Flash memories require
        }
        else
        {
            StrobeAmount = 5300;        // Larger Flash memories require
            loopcount = 19;             // additional cycles for erase.
        }
    }
    HaltCPU();

    for (i = loopcount; i > 0; i--)
    {
        ClrTCLK();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2408);         // set RW to write
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x0128);         // FCTL1 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(EraseMode);      // Enable erase mode
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x012A);         // FCTL2 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(0xA540);         // MCLK is source, DIV=1
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x012C);         // FCTL3 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(FCTL3_val);      // Clear FCTL3; F2xxx: Unlock Info-Seg. A by toggling LOCKA-Bit if required,
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(EraseAddr);      // Set erase address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(0x55AA);         // Dummy write to start erase
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2409);         // Set RW to read
        TCLKstrobes(StrobeAmount);  // Provide TCLKs
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2408);         // Set RW to write
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x0128);         // FCTL1 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(0xA500);         // Disable erase
        SetTCLK();
    }
    // set LOCK-Bits again
    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012C);         // FCTL3 address
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(FCTL3_val | 0x0010);      // Lock Inf-Seg. A by toggling LOCKA (F2xxx) and set LOCK again
    SetTCLK();

    ReleaseCPU();
}
#else
//----------------------------------------------------------------------------
//! \brief This function performs a mass erase (with and w/o info memory) or a
//! segment erase of a FLASH module specified by the given mode and address.
//! Large memory devices get additional mass erase operations to meet the spec.
//! (Could be extended with erase check via PSA)
//! \param[in] word Mode (could be ERASE_MASS or ERASE_MAIN or ERASE_SGMT)
//! \param[in] word Addr (any address within the selected segment)
void EraseFLASH(word EraseMode, word EraseAddr)
{
#ifdef ixx_family
    word StrobeAmount = 9628;       // ixx family requires additional TCLKs due
                                    // due to larger segment size
#else
    word StrobeAmount = 4820;       // default for Segment Erase
#endif
    word i, loopcount = 1;          // erase cycle repeating for Mass Erase
    word FCTL3_val = SegmentInfoAKey;   // SegmentInfoAKey holds Lock-Key for Info
                                        // Seg. A     

    if ((EraseMode == ERASE_MASS) || (EraseMode == ERASE_MAIN))
    {
        if(DeviceHas_FastFlash())
        {
            StrobeAmount = 10600;        // Larger Flash memories require
        }
        else
        {
            StrobeAmount = 5300;        // Larger Flash memories require
            loopcount = 19;             // additional cycles for erase.
        }
    }
    HaltCPU();

    for (i = loopcount; i > 0; i--)
    {
        ClrTCLK();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2408);         // set RW to write
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x0128);         // FCTL1 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(EraseMode);      // Enable erase mode
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x012A);         // FCTL2 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(0xA540);         // MCLK is source, DIV=1
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x012C);         // FCTL3 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(FCTL3_val);      // Clear FCTL3; F2xxx: Unlock Info-Seg. A by toggling LOCKA-Bit if required,
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(EraseAddr);      // Set erase address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(0x55AA);         // Dummy write to start erase
        SetTCLK();

        ClrTCLK();
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2409);         // Set RW to read
        TCLKstrobes(StrobeAmount);  // Provide TCLKs
        IR_Shift(IR_CNTRL_SIG_16BIT);
        DR_Shift16(0x2408);         // Set RW to write
        IR_Shift(IR_ADDR_16BIT);
        DR_Shift16(0x0128);         // FCTL1 address
        IR_Shift(IR_DATA_TO_ADDR);
        DR_Shift16(0xA500);         // Disable erase
        SetTCLK();
    }
    // set LOCK-Bits again
    ClrTCLK();
    IR_Shift(IR_ADDR_16BIT);
    DR_Shift16(0x012C);         // FCTL3 address
    IR_Shift(IR_DATA_TO_ADDR);
    DR_Shift16(FCTL3_val | 0x0010);      // Lock Inf-Seg. A by toggling LOCKA (F2xxx) and set LOCK again
    SetTCLK();

    ReleaseCPU();    
}
#endif

//----------------------------------------------------------------------------
//! \brief This function performs an Erase Check over the given memory range
//! \param[in] word StartAddr (Start address of memory to be checked)
//! \param[in] word Length (Number of words to be checked)
//! \return word (STATUS_OK if erase check was successful, STATUS_ERROR 
//! otherwise)
word EraseCheck(word StartAddr, word Length)
{
    Length >>= 1;
    for (uint p = 0; p < Length; p++)
    {
        if(ReadMem(F_WORD, StartAddr + p * 2) != 0xFFFF) {
            Length = p;
                ReleaseCPU();   
            return STATUS_ERROR;
        }
    }
    return STATUS_OK; 
//    return (VerifyPSA(StartAddr, Length, 0));
}

//----------------------------------------------------------------------------
//! \brief This function performs a Verification over the given memory range
//! \param[in] word StartAddr (Start address of memory to be verified)
//! \param[in] word Length (Number of words to be verified)
//! \param[in] word *DataArray (Pointer to array with the data)
//! \return word (STATUS_OK if verification was successful, STATUS_ERROR
//! otherwise)
word VerifyMem(word StartAddr, word Length, word *DataArray)
{
    for (uint p = 0; p < Length; p++)
    {
        if(ReadMem(F_WORD, StartAddr + p * 2) != DataArray[p])
            return STATUS_ERROR;
    }
    return STATUS_OK;
//    return (VerifyPSA(StartAddr, Length, DataArray));
}

//------------------------------------------------------------------------
//! \brief This function blows the security fuse.
//! \return word (TRUE if burn was successful, FALSE otherwise)
word BlowFuse(void)
{
#ifdef SPYBIWIRE_MODE 

    IR_Shift(IR_PREPARE_BLOW);      // Initialize fuse blowing
    MsDelay(1);
    
    IR_Ex_Blow_SBW_Shift();
    MsDelay(1);
    VPPon( VPP_ON_TEST );           // Switch VPP onto selected pin
    MsDelay(3);
    gpio_set_mask(1 << SBWDATO);             // Execute fuse blowing  
    MsDelay(1);
  
#else

    word mode = VPP_ON_TEST;        // Devices with TEST pin: VPP to TEST

    if(!DeviceHas_TestPin())
    {
        // Devices without TEST pin
        IR_Shift(IR_CNTRL_SIG_16BIT);// TDO becomes TDI functionality
        DR_Shift16(0x7201);
        TDOisInput();
        mode = VPP_ON_TDI;          // Enable VPP on TDI
    }

    IR_Shift(IR_PREPARE_BLOW);      // Initialize fuse blowing
    MsDelay(1);
    VPPon(mode);                    // Switch VPP onto selected pin
    MsDelay(5);
    IR_Shift(IR_EX_BLOW);           // Execute fuse blowing
    MsDelay(1);

#endif
    
    // Switch off power to target and wait
    ReleaseTarget();                // switch VPP and VCC target off
    MsDelay(200);

    // Check fuse: switch power on, simulate an initial JTAG entry
    InitTarget();                   // Supply and preset Target Board

    // Return result of "is fuse blown?"
    return(GetDevice() == STATUS_FUSEBLOWN);
}

//------------------------------------------------------------------------
//! \brief This function checks if the JTAG access security fuse is blown.
//! \return word (STATUS_OK if fuse is blown, STATUS_ERROR otherwise)
word IsFuseBlown(void)
{
    word i;

    for (i = 3; i > 0; i--)     //  First trial could be wrong
    {
        IR_Shift(IR_CNTRL_SIG_CAPTURE);
        if (DR_Shift16(0xAAAA) == 0x5555)
        {
            return(STATUS_OK);  // Fuse is blown
        }
    }
    return(STATUS_ERROR);       // fuse is not blown
}

//------------------------------------------------------------------------
//! \brief This Function unlocks segment A of the InfoMemory (Flash)
void UnlockInfoA(void)
{
    SegmentInfoAKey = 0xA540;
}

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
