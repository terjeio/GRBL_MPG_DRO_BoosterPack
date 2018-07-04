//
// 4x4 keypad handler with partial 2-key rollover, autorepeat and I2C interface
//
// Target: MSP430G2553
//
// v1.1 / 2018-05-08 / Io Engineering / Terje
//

// TODO: change interrupt signal to simulated open collector by setting the pin as input when inactive

/*

Copyright (c) 2017-2018, Terje Io
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

#include <msp430.h> 

#include <stdint.h>
#include <stdbool.h>

#define I2CADDRESS 0x49

#define MPGKEYS // 5x5 keypad (for grbl MPG & DRO)

#define SCL BIT6    // P1.6 I2C
#define SDA BIT7    // P1.7 I2C

#ifdef MPGKEYS
#define BUTDRIVE (BIT0|BIT1|BIT2|BIT3|BIT4) // P1.0-4
#define BUTINPUT (BIT0|BIT1|BIT2|BIT3|BIT4)	// P2.0-4
#define	BUTINTR BIT5					    // P2.5
#else
#define BUTDRIVE (BIT0|BIT1|BIT2|BIT3)      // P1.0-3
#define BUTINPUT (BIT0|BIT1|BIT2|BIT3)      // P2.0-3
#define BUTINTR BIT4                        // P2.4
#endif

#define CNCMAPPING

typedef enum {
    Keytype_None,
    Keytype_AutoRepeat,
    Keytype_SingleEvent,
    Keytype_LongPress,
    Keytype_Persistent
} keytype_t;

typedef struct {
    char key;
    keytype_t type;
    uint16_t scanCode;
} key_t;

char keycode = '\0'; // Keycode to transmit
unsigned char i2cData[3];
unsigned volatile int i2cRXCount;
unsigned volatile char *pi2cData;
volatile bool keyclick = false;

#ifdef CNCMAPPING

#define ROWSHIFT 8

bool getpower = false;
unsigned int laserpower = 10;

#define NUMKEYS 30

const key_t keyMap[NUMKEYS] = {
    {'\r', Keytype_SingleEvent, 0x0101}, // MPG mode toggle
    {'C',  Keytype_SingleEvent, 0x0201}, // Flood coolant toogle
    {'R',  Keytype_Persistent,  0x0401}, // Right (X+)
    {'.',  Keytype_SingleEvent, 0x0801},
    {'.',  Keytype_SingleEvent, 0x1001},

    {'~',  Keytype_SingleEvent, 0x0102}, // Cycle Start
    {'S',  Keytype_SingleEvent, 0x0202}, // Spindle on/off toogle
    {'.',  Keytype_SingleEvent, 0x0402},
    {'U',  Keytype_Persistent,  0x0802}, // Up (Z+)
    {'.',  Keytype_SingleEvent, 0x1002},

    {'E',  Keytype_LongPress,   0x0104}, // Z lock
    {'m',  Keytype_SingleEvent, 0x0204}, // X factor
    {'.',  Keytype_SingleEvent, 0x0404},
    {'.',  Keytype_SingleEvent, 0x0804},
    {'L',  Keytype_Persistent,  0x1004}, // Left (X-)

    {'!',  Keytype_SingleEvent, 0x0108}, // Feed hold
    {'M',  Keytype_SingleEvent, 0x0208}, // Mist coolant toogle
    {'F',  Keytype_Persistent,  0x0408},
    {'.',  Keytype_SingleEvent, 0x0808},
    {'H',  Keytype_LongPress,   0x1008}, // Jog toggle / Home

    {'o',  Keytype_SingleEvent, 0x0110}, // Z factor
    {'A',  Keytype_LongPress,   0x0210}, // X lock
    {'.',  Keytype_SingleEvent, 0x0410},
    {'.',  Keytype_SingleEvent, 0x0810},
    {'D',  Keytype_Persistent,  0x1010}, // Down (Z-),
// two key rollover codes
    {'s',  Keytype_Persistent,  0x0C03}, // R + U
    {'r',  Keytype_Persistent,  0x1411}, // D + R
    {'q',  Keytype_Persistent,  0x1806}, // L + U
    {'t',  Keytype_Persistent,  0x1014}, // D + L
    {'.',  Keytype_SingleEvent, 0xC502}
};

#else

#define ROWSHIFT 4
#define NUMKEYS 20

const key_t keyMap[NUMKEYS] = {
    {0x0B, Keytype_AutoRepeat, 0x11},
    {'9',  Keytype_AutoRepeat, 0x21},
    {'8',  Keytype_AutoRepeat, 0x41},
    {'7',  Keytype_AutoRepeat, 0x81},

    {0x1B, Keytype_AutoRepeat, 0x12},
    {'6',  Keytype_AutoRepeat, 0x22},
    {'5',  Keytype_AutoRepeat, 0x42},
    {'4',  Keytype_AutoRepeat, 0x82},

    {0x18, Keytype_SingleEvent, 0x14},
    {'3',  Keytype_AutoRepeat,  0x24},
    {'2',  Keytype_AutoRepeat,  0x44},
    {'1',  Keytype_AutoRepeat,  0x84},

    {'\r', Keytype_AutoRepeat, 0x18},
    {'-',  Keytype_AutoRepeat, 0x28},
    {'+',  Keytype_AutoRepeat, 0x48},
    {'0',  Keytype_AutoRepeat, 0x88},

// two key rollover codes
    {'s',  Keytype_AutoRepeat,  0xC3},
    {'t',  Keytype_AutoRepeat,  0xC6},
    {'q',  Keytype_AutoRepeat,  0x63},
    {'r',  Keytype_AutoRepeat,  0x66}
};

#endif

// System Routines

const key_t *KeyScan (void);

void initI2C (void)
{
	P1SEL  |= SCL|SDA;				// Assign I2C pins to
	P1SEL2 |= SCL|SDA;				// USCI_B0 (P1)

	IE2 &= ~UCB0TXIE;  				// Disable TX interrupt
	UCB0CTL1 |= UCSWRST;			// Enable SW reset
	UCB0CTL0 = UCMODE_3|UCSYNC;		// I2C Slave, synchronous mode
	UCB0I2COA = I2CADDRESS;			// Own Address is 048h
	UCB0CTL1 &= ~UCSWRST;			// Clear SW reset, resume operation
	UCB0I2CIE |= UCSTPIE| UCSTTIE;	// Enable STT, STP and
	IE2 |= UCB0RXIE|UCB0TXIE;		// RX/TX interrupts
}

void sleep (uint16_t time)
{
    TA0CTL |= TACLR;    // Clear timer and
    TA0CCR0 = time;     // set sleep duration
    TA0CTL |= MC0;      // Start timer in up mode and
    LPM0;               // sleep until times out...
}

const key_t *Debounce (char key)
{
    const key_t *keypress;

    while((keypress = KeyScan()) && keypress->key != key) { // Scan keys
        sleep(5000);                                        // until result
        key = keypress->key;                                // settles
    }

    return keypress;
}

void main (void)
{
    volatile char lastkey;
	uint_fast16_t autorepeat;
	bool longPress = false;
	const key_t *keypress;

	DCOCTL = 0;
	WDTCTL = WDTPW+WDTHOLD;		        // Stop watchdog timer
	DCOCTL = CALDCO_16MHZ;              // Set DCO for 16MHz using
	BCSCTL1 = CALBC1_16MHZ;             // calibration registers
	BCSCTL2 = DIVS0|DIVS1;		        // SMCLK = MCLK / 8

    TA0CTL |= TACLR|TASSEL1|ID0|ID1;    // SMCLK/8, Clear TA
    TA0CCTL0 |= CCIE;                   // Enable CCR0 interrupt

    _EINT();					        // Enable interrupts

	sleep(50);

	initI2C();

	P2DIR |= BUTINTR;			        // Enable P2 button pressed out (keydown signal)
    P2OUT &= ~BUTINTR;                  // and set it low
	P2DIR &= ~BUTINPUT;				    // P2.x button inputs
	P2OUT &= ~BUTINPUT;					// Enable button input pull downs
	P2REN |= BUTINPUT;					// ...
	P2IES &= ~BUTINPUT;					// and set L-to-H interrupt

	P2IFG = 0;							// Clear any pending flags
	P2IE |= BUTINPUT;					// Enable interrupts

	P1DIR |= BUTDRIVE;					// Enable P1 outputs
	P1OUT |= BUTDRIVE;					// for keypad

	P3DIR = 0xFF;						// Set P3 pins to outputs
	P3OUT = 0xFF;						// and set high

	while(1) {

		keycode = '\0';
		keyclick = false;
	    P1OUT |= BUTDRIVE;                          // Drive all rows high
        P2OUT &= ~BUTINTR;                          // and clear button strobe
        P2IFG = 0;                                  // Clear any pending flags

        sleep(10000);                               // Wait a bit for things to settle

        if(!(P2IN & BUTINPUT)) {                    // No key pressed?
            P2IE |= BUTINPUT;                       // Yes, enable interrupts and
            LPM0; 								    // sleep until key pressed

            P2IE &= ~BUTINPUT;                      // Disable interrupts
        }

        sleep(10000);                               // Wait a bit for things to settle

		if(keyclick && (keypress = KeyScan())) {    // Key still pressed?
				            // Wait a bit (40 ms)
			keypress = Debounce(keypress->key);     // and debounce
		}

		if(keypress) {

            lastkey = '\0';
            autorepeat = 0;
            longPress = false;

			do {

                if(lastkey && keypress->key != lastkey) { // Key change?

                    keycode = '\0';
                    autorepeat = 0;
                    longPress = false;

                    if(P2OUT & BUTINTR) {   // for persistent key?
                        P2OUT &= ~BUTINTR;  // terminate key pressed signal
                        sleep(200);         // and sleep a little (0.8 mS)
                    }
                }

                switch(keypress->type) {

                    case Keytype_LongPress:
                        keycode = keypress->key;
                        longPress = true;
                        autorepeat++;
                        sleep(64000);
                        break;

                    case Keytype_AutoRepeat:
                        keycode = keypress->key;
                        P2OUT |= BUTINTR;
                        sleep(50); // 200 us
                        P2OUT &= ~BUTINTR;
                        autorepeat++;
                        sleep(autorepeat < 2 ? 64000 : 10000);
                        break;

                    case Keytype_Persistent:
                        keycode = keypress->key;
                        P2OUT |= BUTINTR;
                        break;

                    default:
                        if(lastkey != keypress->key) {
                            keycode = keypress->key;
                            P2OUT |= BUTINTR;
                            sleep(50); // 200 us
                            P2OUT &= ~BUTINTR;
                        }
                        break;
                }

                lastkey = keypress->key;

#ifdef CNCMAPPING
				switch(keycode) {

					case '-':
						if(laserpower > 0)
							laserpower--;
						break;

					case '+':
						if(laserpower < 100)
							laserpower++;
						break;
				}
#endif
				sleep(5000); // Wait for ~20 ms before transmitting again

			} while((keypress = Debounce(lastkey))); // Keep transmitting while button(s) held down

#ifdef CNCMAPPING
			if(lastkey == '-' || lastkey == '+') {
				keycode = 'P';
				P2OUT |= BUTINTR;
				sleep(50); // 200 us
			} else
#endif
			if(longPress) { // lowercaps key
                keycode = autorepeat < 5 ? lastkey | 0x20 : lastkey;
                P2OUT |= BUTINTR;
                sleep(50); // 200 us
			}
	        P2OUT &= ~BUTINTR;                          // Clear button strobe
		}
	}
}

const key_t *KeyScan (void)
{
#if ROWSHIFT == 8
	uint16_t scancode = 0;	                // Initialize scancode and
#else
    uint8_t scancode = 0;                   // Initialize scancode and
#endif
    uint16_t index = BIT0;                  // row mask

	if(!(P2IN & BUTINPUT))					// Keys still pressed?
		return 0;						    // no, exit
keycode = P2IN & BUTINPUT;
	while(index & BUTDRIVE) {				// Loop through all rows

		P1OUT &= ~BUTDRIVE;					// Stop driving rows

		P2DIR |= BUTINPUT;					// Temporarily set column pins to output
		P2OUT &= ~BUTINPUT;					// and switch low to bleed off charge
		P2DIR &= ~BUTINPUT;					// in order to avoid erroneous reads
		P2OUT &= ~BUTINPUT;					// Enable pull down resistors
		P2REN |= BUTINPUT;                  // on column inputs

		P1OUT |= index;						// Drive row

		__delay_cycles(16);

		if(P2IN & BUTINPUT) {				// If any key pressed:
			scancode |= index << ROWSHIFT;  // set bit for row scanned
			scancode |= (P2IN & BUTINPUT);	// set bit(s) for column(s)
		}

		index = index << 1;                 // Next row
	}

	P1OUT |= BUTDRIVE;						// Drive all rows high again

	index = NUMKEYS;

	if(scancode != 0)                      				        // If key(s) were pressed
		while(index && keyMap[--index].scanCode != scancode);	// then try to resolve to legal entry in lookup table

	return scancode != 0 && keyMap[index].scanCode == scancode ? &keyMap[index] : 0;
}

// P2.x Interrupt service routine
#pragma vector=PORT2_VECTOR
__interrupt void P2_ISR(void)
{
	P2IFG &= ~BUTINPUT;						// Clear button interrupt flag(s)

	if((P2IN & BUTINPUT) && !keyclick) {	// Button interrupt while sleeping?
		keyclick = true;					// Yep - set event semaphore and
		LPM0_EXIT;                          // exit LPM0 on return
	}
}

// CCR0 Interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void CCR0_ISR(void)
{
	TA0CTL &= ~(MC0|MC1|TAIFG);	// Stop debounce timer and clear
    TA0CCTL0 &= ~CCIFG;         // interrupt flags
	LPM0_EXIT;					// Exit LPM0 on return
}

#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR(void)
{

#ifdef CNCMAPPING
	if(IFG2 & UCB0TXIFG) {
		UCB0TXBUF = getpower ? laserpower : keycode;      // Transmit current keycode
//		getpower = true;
        keycode = '\0';
	}
#else
	if(IFG2 & UCB0TXIFG)
		UCB0TXBUF = keycode;     // Transmit current keycode
#endif

	if(IFG2 & UCB0RXIFG) {		 // Save any received bytes - not used in this version

		if(i2cRXCount < 3) {
			*pi2cData++ = UCB0RXBUF;
			i2cRXCount++;
		}

	}
}

#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR(void)
{
	uint8_t intstate = UCB0STAT;

	if(intstate & UCGC) {		// General call?
		UCB0STAT &= ~UCGC;		// Clear interrupt flag
		return;					// and return
	}

	if(intstate & UCSTTIFG) {
#ifdef CNCMAPPING
		getpower = false;
#endif
		i2cRXCount = 0;
		pi2cData = i2cData;
	}

#ifdef CNCMAPPING

    if((intstate & UCSTPIFG) && (i2cRXCount > 0)) {

        P3OUT = i2cData[0];

        i2cRXCount = 0;
    }

	if((intstate & UCSTPIFG) && (i2cRXCount > 1)) {

		switch(i2cData[0]) {

			case 2:	// Set power level
				laserpower = i2cData[1];
				break;

		}

		i2cRXCount = 0;
	}
#endif

	UCB0STAT &= ~intstate;		// Clear interrupt flags
}
