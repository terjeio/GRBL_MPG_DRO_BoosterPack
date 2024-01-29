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
| LowLevelFunc430.c                                                          |
|                                                                            |
| Low Level Functions regarding user's hardware                              |
|----------------------------------------------------------------------------|
| Project:              MSP430 Replicator                                    |
| Developed using:      IAR Embedded Workbench 6.20                          |
|             and:      Code Composer Studio 6.0                             |
|----------------------------------------------------------------------------|
| Version history:                                                           |
| 1.0 04/02 FRGR        Initial version.                                     |
| 1.1 04/02 FRGR        Included SPI mode to speed up shifting function by 2.|
| 1.2 06/02 ALB2        Formatting changes, added comments.                  |
| 1.3 08/02 ALB2        Initial code release with Lit# SLAA149.              |
| 1.4 09/05 SUN1        Software delays redesigned to use TimerA harware;    |
|                       see MsDelay() routine. Added TA setup                |
| 1.5 12/05 STO         Adapted for 2xx devices with SpyBiWire using 4JTAG   |
| 1.6 08/08 WLUT        Cleaned up InitTarget() for JTAG init sequence.      |
| 1.7 05/09 GC (Elprotronic)  Added support for the new hardware - REP430F   |
| 1.8 07/09 FB          Added support for Spy-Bi-Wire and function           |
|                        configure_IO_SBW( void )                            |
| 1.9 05/12 RL          Updated commentaries                                 |
|----------------------------------------------------------------------------|
| Designed 2002 by Texas Instruments Germany                                 |
\*==========================================================================*/
//! \file LowLevelFunc430.c
//! \brief Low Level Functions regarding user's Hardware
/****************************************************************************/
/* INCLUDES                                                                 */
/****************************************************************************/

#include "LowLevelFuncRP2040.h"
#include "JTAGfunc430.h"

#include "hardware/gpio.h"

/****************************************************************************/
/* GLOBAL VARIABLES                                                         */
/****************************************************************************/

//! \brief Holds the value of TDO-bit
uint32_t tdo_bit;
//! \brief Holds the last value of TCLK before entering a JTAG sequence
byte TCLK_saved = 1;

/****************************************************************************/
/* FUNCTIONS                                                                */
/****************************************************************************/

#ifdef SPYBIWIRE_MODE
// Combinations of sbw-cycles (TMS, TDI, TDO)
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS low, TDI low, no TDO read
void TMSL_TDIL(void)
{
    TMSL  TDIL  TDOsbw
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS high, TDI low, no TDO read
void TMSH_TDIL(void)
{
    TMSH  TDIL  TDOsbw
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS low, TDI high, no TDO read
void TMSL_TDIH(void)
{
    TMSL  TDIH  TDOsbw
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS high, TDI high, no TDO read
void TMSH_TDIH(void)
{
    TMSH  TDIH  TDOsbw
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS low, TDI high, TDO read
void TMSL_TDIH_TDOrd(void)
{
    TMSL  TDIH  TDO_RD
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS low, TDI low, TDO read
void TMSL_TDIL_TDOrd(void)
{
    TMSL  TDIL  TDO_RD
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS high, TDI high, TDO read
void TMSH_TDIH_TDOrd(void)
{
    TMSH  TDIH  TDO_RD
}
//----------------------------------------------------------------------------
//! \brief Combination of SBW macros: TMS high, TDI low, TDO read
void TMSH_TDIL_TDOrd(void)
{
    TMSH  TDIL  TDO_RD
}

//----------------------------------------------------------------------------
//! \brief Clear TCLK in Spy-Bi-Wire mode
//! \details enters with TCLK_saved and exits with TCLK = 0
void ClrTCLK_sbw(void)
{
    if (TCLK_saved)
    {
        TMSLDH
    }
    else
    {
        TMSL
    }

    gpio_clr_mask(1 << SBWDATO);

    TDIL TDOsbw    //ExitTCLK
    TCLK_saved = 0;
}

//----------------------------------------------------------------------------
//! \brief Set TCLK in Spy-Bi-Wire mode
//! \details enters with TCLK_saved and exits with TCLK = 1
void SetTCLK_sbw(void)
{
    if (TCLK_saved)
    {
        TMSLDH
    }
    else
    {
        TMSL
    }

   gpio_set_mask(1 << SBWDATO);

   TDIH TDOsbw    //ExitTCLK
   TCLK_saved = 1;
}

//----------------------------------------------------------------------------
//! \brief Provide JTAG fuse blow instruction
void IR_Ex_Blow_SBW_Shift(void)
{
    word MSB;
    word Data, i;
   
    Data = IR_EX_BLOW;
   
    // JTAG FSM state = Run-Test/Idle
    if (TCLK_saved)
    {
        TMSH_TDIH();
    }
    else
    {
        TMSH_TDIL();
    }
    TMSH_TDIH();    // JTAG FSM state = Select DR-Scan
    TMSL_TDIH();    // JTAG FSM state = Select IR-Scan
    TMSL_TDIH();    // JTAG FSM state = Capture-IR

    MSB = 0x80;
    for (i = 8; i > 1; i--)
    {
      ((Data & MSB) == 0) ? TMSL_TDIL_TDOrd() : TMSL_TDIH_TDOrd();
      Data <<= 1;
    }
    // last bit requires TMS=1; TDO one bit before TDI    
    ((Data & MSB) == 0) ? TMSH_TDIL_TDOrd() : TMSH_TDIH_TDOrd();    
    // SBWTDIO must be low on exit!
    TMSH_TDIL();
    TMSL_TDIL();
    // instruction shift done!    
    
    TMSL    // go to TDI-slot via TMS-slot
            // now in TDI-slot ready to blow the fuse
}

//----------------------------------------------------------------------------
//! \brief Shift a value into TDI (MSB first) and simultaneously shift out a 
//! value from TDO (MSB first).
//! \param[in] Format (number of bits shifted, 8 (F_BYTE), 16 (F_WORD), 
//! 20 (F_ADDR) or 32 (F_LONG))
//! \param[in] Data (data to be shifted into TDI)
//! \return unsigned long (scanned TDO value)
word Shift(word Format, word Data)
{
   word TDOword = 0x0000;
   word MSB = 0x0000;
   word i;

   (Format == F_WORD) ? (MSB = 0x8000) : (MSB = 0x80);
   for (i = Format; i > 0; i--)
   {
        if (i == 1)                     // last bit requires TMS=1; TDO one bit before TDI
        {
          ((Data & MSB) == 0) ? TMSH_TDIL_TDOrd() : TMSH_TDIH_TDOrd();
        }
        else
        {
          ((Data & MSB) == 0) ? TMSL_TDIL_TDOrd() : TMSL_TDIH_TDOrd();
        }
        Data <<= 1;
        if (tdo_bit & (1 << SBWDATI))
            TDOword++;
        if (i > 1)
            TDOword <<= 1;               // TDO could be any port pin
   }
   TMSH_TDIH();                         // update IR
   if (TCLK_saved)
   {
        TMSL_TDIH();
   }
   else
   {
        TMSL_TDIL();
   }
   return(TDOword);
}
#else
//----------------------------------------------------------------------------
//! \brief Shift a value into TDI (MSB first) and simultaneously shift out a 
//! value from TDO (MSB first).
//! \param[in] Format (number of bits shifted, 8 (F_BYTE), 16 (F_WORD), 
//! 20 (F_ADDR) or 32 (F_LONG))
//! \param[in] Data (data to be shifted into TDI)
//! \return unsigned long (scanned TDO value)
word Shift(word Format, word Data)
{
    word tclk = StoreTCLK();        // Store TCLK state;
    word TDOword = 0x0000;          // Initialize shifted-in word
    word MSB = 0x0000;

    // Shift via port pins, no coding necessary
    volatile word i;
   (Format == F_WORD) ? (MSB = 0x8000) : (MSB = 0x80);
   for (i = Format; i > 0; i--)
   {
       ((Data & MSB) == 0) ? ClrTDI() : SetTDI();
        Data <<= 1;
        if (i == 1)                 // Last bit requires TMS=1
        {
            SetTMS();
        }
        ClrTCK();
        SetTCK();
        TDOword <<= 1;              // TDO could be any port pin
        if (ScanTDO() != 0)
        {          
            TDOword++;
        }
    }
    // common exit
    RestoreTCLK(tclk);                  // restore TCLK state
    
    // JTAG FSM = Exit-DR
    ClrTCK();
    SetTCK();
    // JTAG FSM = Update-DR
    ClrTMS();
    ClrTCK();
    SetTCK();
    // JTAG FSM = Run-Test/Idle
    return(TDOword);
}
#endif

//----------------------------------------------------------------------------
//! \brief Initialization of the Controller Board
void InitController(void)
{
/*
    TRSLDIR = 0;
    // set port to output from MSPF5437 to I/O translators           
    TRSL_CDIR = TEST_DIR + RST_DIR + TCK_DIR + TMS_DIR + TDOI_DIR + TDI_DIR;
    // set all directions from I/O translators to MSP430F5437
    // (all I/O JTAG lines to input)
    TRSLDIR = TEST_DIR + RST_DIR + TCK_DIR + TMS_DIR + TDOI_DIR + TDI_DIR;  

    // set LED ports direction
    LED_DIR |= LED_YELLOW+LED_GREEN+LED_RED;
    // TURN-ON all LEDs at the startup
    LED_OUT |= LED_YELLOW+LED_GREEN+LED_RED;

    // set SW ports pull-ups
    SW_PULLUP |= SW_MODE0+SW_MODE1+SW_1;            // set pull-up/pull-down
    SW_OUT |= SW_MODE0+SW_MODE1+SW_1;               // select pull-up
*/
    gpio_init(TDI);
    gpio_init(TCK);

    SetTargetVcc (0);
    SetVpp( 0 );
}

//----------------------------------------------------------------------------
//! \brief Function to set a specific voltage level via the PMM
//! \param[in] level
void SetVCoreUp (word level)
{
    /*
    // Open PMM registers for write access
    PMMCTL0_H = 0xA5;
    // Set SVS/SVM high side new level
    SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;
    // Set SVM low side to new level
    SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;
    // Wait till SVM is settled
    while ((PMMIFG & SVSMLDLYIFG) == 0);
    // Clear already set flags
    PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);
    // Set VCore to new level
    PMMCTL0_L = PMMCOREV0 * level;
    // Wait till new level reached
    if ((PMMIFG & SVMLIFG))
    {
        while ((PMMIFG & SVMLVLRIFG) == 0);
    }
    // Set SVS/SVM low side to new level
    SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;
    // Lock PMM registers for write access
    PMMCTL0_H = 0x00;
    */
}

//----------------------------------------------------------------------------
//! \brief Set target Vcc (supplied from REP430F)
//! \details input - 10*Vcc[V]-> range 2.1V to 3.6 V (data 21 to 36)
//! or data = 0 -> Vcc OFF  
//! \param[in] level (requested VCC in V * 10)
void SetTargetVcc (word level)  // level - requested Vcc * 10 
{
/*
    if( level == 0 )
        TVCC_EN_OUT |= TVCC_DIS_BIT; 
    else
        TVCC_EN_OUT &= ~TVCC_DIS_BIT; 
    TVCC_EN_DIR |= TVCC_DIS_BIT; 

    if( level < 21 ) level = 21;
    if( level > 36 ) level = 36;
    level = TVCC_MASK & ((level - 21)<<TVCC_SHIFT);
    TVCC_DIR |= TVCC_MASK;  
    TVCC_OUT |= TVCC_MASK;      // set min.Vcc ( 0xF0 )
    TVCC_OUT &= ~level;         // set desired Vcc - clear desired bits
    MsDelay( 50 );
*/
}

//----------------------------------------------------------------------------
//! \brief Determine target VCC
//! \return word (target VCC in mV)
word Get_target_Vcc(void)
{
    return( Get_Vx( ADC12INCH_14 ));
}

//----------------------------------------------------------------------------
//! \brief Determine external VCC
//! \return word (external VCC in mV)
word Get_Ext_Vcc(void)
{
    return( Get_Vx( ADC12INCH_15 ));
}

//----------------------------------------------------------------------------
//! \brief Measure different voltages via ADC12
//! \return word (voltage in mV)
word Get_Vx(word index)
{
    word y = 0,x; 
/*
    // ADC12  initialization
    UCSCTL8 |= MODOSCREQEN;     // Enable osc for ADC12
                                // (in the Unifield Clock System)
    ADC12CTL0 |= ADC12SHT0_8 + ADC12REFON + ADC12ON; // Internal reference =1.5V
    ADC12CTL1 = ADC12SHP;
    ADC12MCTL0 = ADC12SREF_1 + index;   // Input A14 or A15
    
    // Delay for needed ref start-up.
    MsDelay( 5 );

    ADC12CTL0 |= ADC12ENC;  // Enable conversions
    ADC12CTL0 |= ADC12SC;   // Start conversion - sw trigger
    ADC12IFG &= ~BIT0;
    do{ 
    }while( (ADC12IFG & BIT0) == 0 );
    ADC12CTL0 &= ~ADC12ENC; // Disable ADC12

    // Vcc hardware divider - Vcc/ADCin = 3/1;
    // Ref Vcc = 1.5
    // x = x * 3 * 1.5 = x * 4.5 = x * 9 /2

    x = (ADC12MEM0 * 9)>>1;

    // result Vcc =  x * 1000/4096   in mV
    // y = x * 1000/(4000+96) ~= x * 0.25 * 4000/(4000+96) 
    //   = x * 0.25 * 1/(1+96/4000) != x * 0.25 * (1 - 96/4000)
    // y = x * 0.25 * (1 - 96/4000) ~= x/4 - x * 24/4000
    //   = x/4 - x/167 ~= x/4 - 3*x/512 = x/4 - x/256 - x/512
    
    y = x>>2;           // y = x/4      
    x = x>>8;           // x = x/256
    y -= x + (x>>1);    // y = x/4 - x/256 - x/512;
    */
    return(y);
}
//----------------------------------------------------------------------------
//! \brief Set the direction for the TDI pin
//! \param[in] dir (0 = IN - from target to REP430F, !0 = OUT)
void TDI_dir(word dir)
{
    gpio_set_dir(TDI, dir != 0); // Always set to output
 //   gpio_set_dir(TDI_DIR, dir != 0);
/*
    JTAGDIR |= TDI;     // Always set to output in the F5437 
    if( dir == 0 )      // Direction IN - from target to REP430F
        TRSLDIR |= TDI_DIR;  
    else
        TRSLDIR &= ~TDI_DIR;  */
}
//----------------------------------------------------------------------------
//! \brief Set the direction for the TDO pin
//! \param dir (0 = IN - from target to REP430F, !0 = OUT)
void TDOI_dir(word dir)
{
#if TDO != -1
    gpio_set_dir(TDO, dir); // Always set to output
//    gpio_set_dir(TDOI_DIR, dir != 0);
/*
    JTAGDIR &= ~TDO;    // Always set to input in the F5437  
    if( dir == 0 )      // Direction IN - from target to REP430
        TRSLDIR |= TDOI_DIR;  
    else
        TRSLDIR &= ~TDOI_DIR;  */
#endif
}
//----------------------------------------------------------------------------
//! \brief Set the direction for the TEST pin
//! \param dir (0 = IN - from target to REP430F, !0 = OUT)
void TEST_dir(word dir)
{
#if TEST != -1
    if( dir == 0 )             // Direction IN - from target to REP430
    {
        gpio_set_dir(TEST, 0); // Switch MSP port to input first - to avoid two outputs on the line
//      gpio_set_dir(TEST_DIR, 1); 
    }
    else
    {
//        gpio_set_dir(TEST_DIR, 0);   // Switch translator to output first - to avoid two outputs on the line
        gpio_set_dir(TEST, 1);       // Switch MSP port to output
    }
#endif
}
//----------------------------------------------------------------------------
//! \brief Set the direction for the TMS pin
//! \param dir (0 = IN - from target to REP430F, !0 = OUT)
void TMS_dir(word dir)
{
#if TMS != 1
    /*
    if( dir == 0 )             // Direction IN - from target to REP430
    {
        JTAGDIR &= ~TMS;       // Switch MSP port to input first - to avoid two outputs on the line
        TRSLDIR |= TMS_DIR;  
    }
    else
    {
        TRSLDIR &= ~TMS_DIR;  // Switch translator to output first - to avoid two outputs on the line
        JTAGDIR |= TMS;       // Switch MSP port to output
    }*/
#endif
}
//----------------------------------------------------------------------------
//! \brief Set the direction for the RST pin
//! \param dir (0 = IN - from target to REP430F, !0 = OUT)
void RST_dir(word dir)
{
#if RST != -1
    if( dir == 0 )            // Direction IN - from target to REP430
    {
        gpio_set_dir(RST, 0);      // Switch MSP port to input first - to avoid two outputs on the line
//        gpio_set_dir(RST_DIR, 1);  
    }
    else
    {
//        gpio_set_dir(RST_DIR, 0);  // Switch translator to output first - to avoid two outputs on the line
        gpio_set_dir(RST, 1);      // Switch MSP port to output
    }
#endif
}
//----------------------------------------------------------------------------
//! \brief Set the direction for the TCK pin
//! \param dir (0 = IN - from target to REP430F, !0 = OUT)
void TCK_dir(word dir)
{
    gpio_set_dir(TCK, dir); // Always set to output
    /*
    JTAGDIR |= TCK;     // Always set to output in the F5437 
    if( dir == 0 )      // Direction IN - from target to REP430
        TRSLDIR |= TCK_DIR;  
    else
        TRSLDIR &= ~TCK_DIR;
        */
}
//----------------------------------------------------------------------------
//! \brief function to set the fuse blow voltage Vpp
//! \param source (Select the pin to which Vpp is applied according to chosen interface)
void SetVpp(word source)
{
    /*
    if( source & (VPPONTEST | VPPONTDI ))           Enable_Vpp();
    if(( source & (VPPONTEST | VPPONTDI )) == 0 )   Disable_Vpp();

    if( source & VPPONTEST )    TEST_dir( 0 );
    if( source & VPPONTDI ) TDI_dir( 0 );

    VPPOUT &= ~( VPPONTDI | VPPONTEST );
    source &= VPPONTDI | VPPONTEST;
    VPPOUT |= source; 
    VPPDIR |= VPPONTDI | VPPONTEST;
    MsDelay( 2 );
    if(( source & VPPONTEST ) == 0 ) TEST_dir( 1 );
    if(( source & VPPONTDI ) == 0 )  TDI_dir( 1 );
    */
}

//----------------------------------------------------------------------------
//! \brief Enable fuse blow voltage Vpp
void Enable_Vpp(void)
{
    /*
    SW_DIR |= SW_VPPEN;
    SW_OUT &= ~SW_VPPEN; 
    MsDelay( 20 );
    */
}

//----------------------------------------------------------------------------
//! \brief Disable fuse blow voltage Vpp
void Disable_Vpp(void)
{
/*
    SW_OUT |= SW_VPPEN; 
    SW_DIR &= ~SW_VPPEN;
*/
}
//----------------------------------------------------------------------------
//! \brief Set JTAG pins to output direction - from REP430F to target
void configure_IO_JTAG(void)
{
    TDI_dir( 1 );
    TDOI_dir( 0 );
    TEST_dir( 1 );
    TMS_dir( 1 );
    RST_dir( 1 );
    TCK_dir( 1 );
}

//----------------------------------------------------------------------------
//! \brief Set SBW pins to output direction - from REP430F to target
void configure_IO_SBW(void)
{
    TDOI_dir( 1 );
    TCK_dir( 1 );
}

//----------------------------------------------------------------------------
//! \brief Set all JTAG pins to input direction - from target to REP430F
void IO_3state(void)
{
    TDI_dir( 0 );
    TDOI_dir( 0 );
    TEST_dir( 0 );
    TMS_dir( 0 );
    RST_dir( 0 );
    TCK_dir( 0 );
}

//----------------------------------------------------------------------------
//! \brief This function switches TDO to Input (used for fuse blowing)
void TDOisInput(void)
{
    TDI_dir( 0 );       // Release TDI pin on target
    TDOI_dir( 1 );      // Switch TDI --> TDO
}

//----------------------------------------------------------------------------
//! \brief Set up I/O pins for JTAG communication
void DrvSignals(void)
{
    SetVpp( 0 );  
    IO_3state();

#if ( INTERFACE == SPYBIWIRE_IF )
    gpio_set_dir(TDI, 1);
    gpio_set_dir(TCK, 0);
/*    JTAGOUT  |= TDI;
    JTAGOUT  &= ~TCK; */
    configure_IO_SBW();
#else
    JTAGOUT  |=  TDI | TMS | TCK | TCLK | RST;
    JTAGOUT  &= ~ TEST;    
    configure_IO_JTAG();
#endif
}

//----------------------------------------------------------------------------
//! \brief Release I/O pins
void RlsSignals(void)
{
    SetVpp( 0 );
    Disable_Vpp();
    IO_3state();
}

//----------------------------------------------------------------------------
//! \brief Initialization of the Target Board (switch voltages on, preset JTAG 
//! pins)
//! \details For devices with normal 4wires JTAG  (JTAG4SBW=0)\n
//! For devices with Spy-Bi-Wire to work in 4wires JTAG (JTAG4SBW=1)
void InitTarget(void)
{
    DrvSignals();
    SetTargetVcc( VCC_LEVEL );  //level - requested Vcc * 10 
}

//----------------------------------------------------------------------------
//! \brief Release Target Board (switch voltages off, JTAG pins are HI-Z)
void ReleaseTarget(void)
{
    RlsSignals();
    SetTargetVcc( 0 );
}

#include "hardware/timer.h"

//----------------------------------------------------------------------------
//! \brief Delay function (resolution is 1 ms)
//! \param[in] milliseconds (number of ms, max number is 0xFFFF)
void MsDelay(word milliseconds)
{
    uint32_t delay = milliseconds * 1000, start = timer_hw->timerawl;
    while (timer_hw->timerawl - start < delay);
}

//----------------------------------------------------------------------------
//! \brief Delay function (resolution is ~1 us)
//! \param[in] microseconds (number of ms, max number is 0xFFFF)
void usDelay(word microseconds)
{
    uint32_t start = timer_hw->timerawl;
    while (timer_hw->timerawl - start < microseconds);
}

#ifdef SPYBIWIRE_MODE
/*----------------------------------------------------------------------------
   This function controls the status LEDs depending on the status
   argument. It stops program in error case.
   Arguments: word status (4 stati - can be extended to 8 - possible for 3 LEDs - Yellow,Green,Red)
              word index (additional number for detailed diagnostics or
                          watch variable during debugging phase)
*/
void TCLKstrobes(word Amount) // enters with TCLK_saved and exits with TCLK = 1
{
   word i;

   if (TCLK_saved)
   {
        TMSLDH
   }                         // TDI = 1 with rising sbwclk
   else
   {
        TMSL
   }

    // This implementation has 30 body cycles! -> 400kHz
    // DO NOT MODIFY IT !

    static volatile uint32_t dly;

   for (i = Amount; i > 0; i--)
   {
        gpio_clr_mask(1 << SBWDATO);
        dly = 16;
        while(--dly)
            __ASM volatile ("nop");
 
        gpio_set_mask(1 << SBWDATO);
        dly = 7;
        while(--dly)
            __ASM volatile ("nop");
 
   }
   TDIH TDOsbw    //ExitTCLK
   TCLK_saved = 1;
}
#else

//----------------------------------------------------------------------------
//! \brief This function generates Amount strobes with the Flash Timing
//! Generator
//! \details Frequency fFTG = 257..476kHz (t = 3.9..2.1us).
//! Used freq. in procedure - 400 kHz\n
//! User knows target frequency, instruction cycles, C implementation.\n
//! No. of MCKL cycles - 18MHz/400 kHz = 45 or 12MHz/400 kHz = 30
//! \param[in] Amount (number of strobes to be generated)
void TCLKstrobes(word Amount)
{
    volatile word i;

    // This implementation has 45 (MCLK=18MHz)
    // or 30 (MCLK 12MHz) body cycles! -> 400kHz
    // DO NOT MODIFY IT !

    for (i = Amount; i > 0; i--)
    {
        JTAGOUT |=  TCLK;       // Set TCLK
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
#ifdef  MCLK_18MHZ
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
#endif
        JTAGOUT &= ~TCLK;       // Reset TCLK
        _NOP();
#ifdef  MCLK_18MHZ
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
        _NOP();
#endif
    }
}
#endif

//----------------------------------------------------------------------------
//! \brief This function controls the status LEDs depending on the status
//! argument. It stops program in error case.
//! \param[in] status (4 stati - can be extended to 8 - possible for 3 LEDs - 
//! Yellow,Green,Red)
//! \param[in] index (additional number for detailed diagnostics or watch 
//! variable during debugging phase)
void ShowStatus(word status, word index)
{
    All_LEDs_off();                     
    switch (status)
    {
        case STATUS_ERROR:
            LED_red_on();               // Switch red LED on
            ReleaseTarget();            // Voltages off, JTAG HI-Z
            while(index);               // Stop program, index must be > 0
        case STATUS_ACTIVE:;            // Switch yellow LEDs on
            LED_yellow_on();
             break;
        case STATUS_OK:                 // Switch green LED on
            LED_green_on();
             break;
        case STATUS_IDLE:;              // Keep LEDs switched off
    }
}                                       // return if active, idle, ok

//----------------------------------------------------------------------------
//! \brief This function performs a Trigger Pulse for test/development
//! \param[in] mode
#ifdef DEBUG
void TriggerPulse(word mode)
{
    switch (mode)
    {
        case 1: LEDOUT  |=  TRIGGER;    // mode = 1: set trigger
                break;
        case 2: LEDOUT  |=  TRIGGER;    // mode = 2: set/reset trigger
        case 0: LEDOUT  &= ~TRIGGER;    // mode = 0: reset trigger
    }
}
#endif

/****************************************************************************/
/*                         END OF SOURCE FILE                               */
/****************************************************************************/
