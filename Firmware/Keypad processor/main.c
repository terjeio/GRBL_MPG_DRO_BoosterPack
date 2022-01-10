//
// Keypad handler with partial 2-key rollover, autorepeat and I2C interface
//
// Target: MSP430G2553
//
// v2.0 / 2022-01-09 / Io Engineering / Terje
//

/*

Copyright (c) 2017-2022, Terje Io
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
#include <string.h>

#define I2CADDRESS 0x49

#define MAP_4X4_EMPTY   0
#define MAP_5X5_EMPTY   1
#define MAP_4X4_DEFAULT 2
#define MAP_5X5_DEFAULT 3
#define MAP_4X4_CNC     4
#define MAP_5X5_CNC     5
#define MAP_5X5_LEGACY  6

#define N_KEYS_MAX 75
#define DEFAULT_MAP MAP_5X5_CNC
#define MPGKEYS // 5x5 keypad (for grbl MPG & DRO)

#define SCL BIT6    // P1.6 I2C
#define SDA BIT7    // P1.7 I2C

#ifdef MPGKEYS
#define BUTINTR BIT5                        // P2.5
#else
#define BUTINTR BIT4                        // P2.4
#endif

#define BUTDRIVE4 (BIT0|BIT1|BIT2|BIT3)      // P1.0-3
#define BUTINPUT4 (BIT0|BIT1|BIT2|BIT3)      // P2.0-3
#define BUTDRIVE5 (BIT0|BIT1|BIT2|BIT3|BIT4) // P1.0-4
#define BUTINPUT5 (BIT0|BIT1|BIT2|BIT3|BIT4) // P2.0-4

typedef uint8_t keytype_t;

enum keytype_t {
    Keytype_None = 0,
    Keytype_AutoRepeat,
    Keytype_SingleEvent,
    Keytype_LongPress,
    Keytype_Persistent
};

#pragma pack(push, 1)

typedef struct {
    char key;
    keytype_t type;
    uint16_t scanCode;
} key_t;

typedef struct {
    uint8_t n_keys;
    uint8_t rowshift;
    uint8_t drive_mask;
    uint8_t input_mask;
    bool powerkeys;
    uint16_t shift_key;
    key_t key[N_KEYS_MAX];
} keymap_t;

#pragma pack(pop)

char keycode = '\0'; // Keycode to transmit
volatile bool keyclick = false;
bool getpower = false;
unsigned int laserpower = 10;
keymap_t keymap;
struct {
    volatile uint16_t count;
    volatile uint8_t *ptr;
    uint8_t data[4];
} i2c_rx = {0};

static void gpio_init (keymap_t *map, uint8_t input_mask, uint8_t drive_mask)
{
    map->input_mask = input_mask;
    map->drive_mask = drive_mask;

    P2DIR &= ~input_mask;               // P2.x button inputs
    P2OUT &= ~input_mask;               // Enable button input pull downs
    P2REN |= input_mask;                // ...
    P2IES &= ~input_mask;               // and set L-to-H interrupt

    P2IFG = 0;                          // Clear any pending flags
    P2IE |= input_mask;                 // Enable interrupts

    P1DIR |= drive_mask;                // Enable P1 outputs
    P1OUT |= drive_mask;                // for keypad
}

// Keytype_LongPress generates the lower case version of the character on a short keypress, uppercase on a long keypress.
// Keytype_Persistent keeps the strobe pin low as long as the key is pressed.

void map_4x4_empty (void)
{
    memset(&keymap, 0, sizeof(keymap_t));
    keymap.rowshift = 4;

    gpio_init(&keymap, BUTINPUT4, BUTDRIVE4);
}

void map_5x5_empty (void)
{
    memset(&keymap, 0, sizeof(keymap_t));
    keymap.rowshift = 5;

    gpio_init(&keymap, BUTINPUT5, BUTDRIVE5);
}

void map_4x4_default (void)
{
    static const key_t keyMap[] = {
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

    map_4x4_empty();

    keymap.n_keys = (sizeof(keyMap) / sizeof(key_t));
    memcpy(&keymap.key, keyMap, sizeof(keyMap));
}

void map_5x5_default (void)
{
    static const key_t keyMap[] = {
        {'7',  Keytype_LongPress,   0b0000100001}, //
        {'8',  Keytype_Persistent,  0b0000100010}, //
        {'9',  Keytype_LongPress,   0b0000100100}, //
        {'-',  Keytype_Persistent,  0b0000101000}, //
        {0x7F, Keytype_SingleEvent, 0b0000110000}, // DEL

        {'4',  Keytype_Persistent,  0b0001000001}, //
        {'5',  Keytype_LongPress,   0b0001000010}, //
        {'6',  Keytype_Persistent,  0b0001000100}, //
        {'+',  Keytype_LongPress,   0b0001001000}, //
        {0x08, Keytype_SingleEvent, 0b0001010000}, // BS

        {'1',  Keytype_SingleEvent, 0b0010000001}, //
        {'2',  Keytype_Persistent,  0b0010000010}, //
        {'3',  Keytype_SingleEvent, 0b0010000100}, //
        {'\r', Keytype_Persistent,  0b0010001000}, //
        {0x0B, Keytype_SingleEvent, 0b0010010000}, // VT

        {'0',  Keytype_SingleEvent, 0b0100000001}, //
        {0,    Keytype_SingleEvent, 0b0100000010}, //
        {0,    Keytype_SingleEvent, 0b0100000100}, //
        {0x09, Keytype_SingleEvent, 0b0100001000}, // HT
        {0x0A, Keytype_SingleEvent, 0b0100010000}, // LF

        {128,  Keytype_LongPress,   0b1000000001}, //
        {129,  Keytype_SingleEvent, 0b1000000010}, //
        {0,    Keytype_LongPress,   0b1000000100}, //
        {0,    Keytype_SingleEvent, 0b1000001000}, //
        {0xFF, Keytype_SingleEvent, 0b1000010000}, // Shift
    };

    map_5x5_empty();

    keymap.n_keys = (sizeof(keyMap) / sizeof(key_t));
    keymap.shift_key = 0b1000010000;
    memcpy(&keymap.key, keyMap, sizeof(keyMap));
}

void cnc_map_4x4_default (void)
{
    static const key_t keyMap[] = {
        {0x0B, Keytype_SingleEvent, 0x11},
        {'C',  Keytype_SingleEvent, 0x21},
        {'U',  Keytype_Persistent, 0x41},
        {'M',  Keytype_SingleEvent, 0x81},

        {0x1B, Keytype_SingleEvent, 0x12},
        {'R',  Keytype_Persistent, 0x22},
        {'H',  Keytype_LongPress, 0x42},
        {'L',  Keytype_Persistent, 0x82},

        {'~',  Keytype_SingleEvent, 0x14},
        {'3',  Keytype_SingleEvent,  0x24},
        {'D',  Keytype_Persistent,  0x44},
        {'S',  Keytype_SingleEvent,  0x84},

        {'!',  Keytype_SingleEvent, 0x18},
        {'-',  Keytype_SingleEvent, 0x28},
        {'+',  Keytype_SingleEvent, 0x48},
        {'0',  Keytype_SingleEvent, 0x88},

    // two key rollover codes
        {'s',  Keytype_Persistent,  0xC3},
        {'t',  Keytype_Persistent,  0xC6},
        {'q',  Keytype_Persistent,  0x63},
        {'r',  Keytype_Persistent,  0x66}
    };

    map_4x4_empty();

    keymap.n_keys = (sizeof(keyMap) / sizeof(key_t));
    keymap.powerkeys = true;
    memcpy(&keymap.key, keyMap, sizeof(keyMap));
}

void cnc_map_default (void)
{
    static const key_t keyMap[] = {
        {'A',  Keytype_LongPress,   0b0000100001}, // X lock toggle, X<0>
        {'F',  Keytype_Persistent,  0b0000100010}, // Jog Y+
        {'G',  Keytype_LongPress,   0b0000100100}, // Y lock toggle, Y<0>
        {'U',  Keytype_Persistent,  0b0000101000}, // Jog Z+
        {'\r', Keytype_SingleEvent, 0b0000110000}, // MPG mode toggle

        {'L',  Keytype_Persistent,  0b0001000001}, // Jog X-
        {'H',  Keytype_LongPress,   0b0001000010}, // Jog mode toggle (step, slow, fast), Home
        {'R',  Keytype_Persistent,  0b0001000100}, // Jog X+
        {'E',  Keytype_LongPress,   0b0001001000}, // Z lock toggle, Z<0>
        {'~',  Keytype_SingleEvent, 0b0001010000}, // Cycle Start

        {'M',  Keytype_SingleEvent, 0b0010000001}, // Mist coolant toogle
        {'B',  Keytype_Persistent,  0b0010000010}, // Jog Y-
        {'C',  Keytype_SingleEvent, 0b0010000100}, // Flood coolant toogle
        {'D',  Keytype_Persistent,  0b0010001000}, // Jog Z-
        {'!',  Keytype_SingleEvent, 0b0010010000}, // Feed hold

        {'m',  Keytype_SingleEvent, 0b0100000001}, // X MPG scale factor toggle
        {'n',  Keytype_SingleEvent, 0b0100000010}, // Y MPG scale factor toggle
        {'o',  Keytype_SingleEvent, 0b0100000100}, // Z MPG scale factor toggle
        {'T',  Keytype_SingleEvent, 0b0100001000}, // Toggle MPG axis (X, Y, Z)
        {'s',  Keytype_SingleEvent, 0b0100010000}, // Spindle on/off toogle

        {'I',  Keytype_LongPress,   0b1000000001}, // Feed rate override +10%, feed rate override reset
        {'j',  Keytype_SingleEvent, 0b1000000010}, // Feed rate override -10%
        {'K',  Keytype_LongPress,   0b1000000100}, // Spindle RPM override +10%, spindle RPM override reset
        {'z',  Keytype_SingleEvent, 0b1000001000}, // Spindle RPM override -10%
        {0xFF, Keytype_SingleEvent, 0b1000010000}, // Shift
    // two key rollover codes
        {'r',  Keytype_Persistent,  0b0001000100|0b0000100010}, // R + F: Jog X+Y+
        {'q',  Keytype_Persistent,  0b0001000100|0b0010000010}, // R + B: Jog X+Y-
        {'s',  Keytype_Persistent,  0b0001000001|0b0000100010}, // L + F: Jog X-Y+
        {'t',  Keytype_Persistent,  0b0001000001|0b0010000010}, // L + B: Jog X-Y-
        {'w',  Keytype_Persistent,  0b0001000100|0b0000101000}, // R + U: Jog X+Z+
        {'v',  Keytype_Persistent,  0b0001000100|0b0010001000}, // R + D: Jog X+Z-
        {'u',  Keytype_Persistent,  0b0001000001|0b0000101000}, // L + U: Jog X-Z+
        {'x',  Keytype_Persistent,  0b0001000001|0b0010001000}, // L + D: Jog X-Z-
        {0x93, Keytype_SingleEvent, 0b1000010000|0b1000000001}, // Shift + I: Feed rate override +1%
        {0x94, Keytype_SingleEvent, 0b1000010000|0b1000000010}, // Shift + j: Feed rate override -1%
        {0x9C, Keytype_SingleEvent, 0b1000010000|0b1000000100}, // Shift + K: Spindle RPM override +1%
        {0x9D, Keytype_SingleEvent, 0b1000010000|0b1000001000}, // Shift + z: Spindle RPM override -1%
    };

    map_5x5_empty();

    keymap.n_keys = (sizeof(keyMap) / sizeof(key_t));
    keymap.shift_key = 0b1000010000;
    memcpy(&keymap.key, keyMap, sizeof(keyMap));
}

void cnc_map_legacy (void)
{
    static const key_t keyMap[] = {
        {'\r', Keytype_SingleEvent, 0x0101}, // MPG mode toggle
        {'C',  Keytype_SingleEvent, 0x0201}, // Flood coolant toogle
        {'R',  Keytype_Persistent,  0x0401}, // Right (X+)
        {'.',  Keytype_SingleEvent, 0x0801},
        {'.',  Keytype_SingleEvent, 0x1001}, // Shift

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
        {'.',  Keytype_SingleEvent, 0xC502},
        {'.',  Keytype_SingleEvent, 0b0000011111}
    };

    map_5x5_empty();

    keymap.n_keys = (sizeof(keyMap) / sizeof(key_t));
    keymap.rowshift = 8;
    memcpy(&keymap.key, keyMap, sizeof(keyMap));
}

void select_map (uint8_t map)
{
    switch(map) {

        case MAP_4X4_EMPTY:
            map_4x4_empty();
            break;

        case MAP_5X5_EMPTY:
            map_5x5_empty();
            break;

        case MAP_4X4_DEFAULT:
            map_4x4_default();
            break;

        case MAP_5X5_DEFAULT:
            map_5x5_default();
            break;

        case MAP_4X4_CNC:
            cnc_map_4x4_default();
            break;

        case MAP_5X5_CNC:
            cnc_map_default();
            break;

        case MAP_5X5_LEGACY:
            cnc_map_legacy();
            break;
    }
}

// System Routines

const key_t *keyscan (void);

void initI2C (void)
{
    P1SEL  |= SCL|SDA;              // Assign I2C pins to
    P1SEL2 |= SCL|SDA;              // USCI_B0 (P1)

    IE2 &= ~UCB0TXIE;               // Disable TX interrupt
    UCB0CTL1 |= UCSWRST;            // Enable SW reset
    UCB0CTL0 = UCMODE_3|UCSYNC;     // I2C Slave, synchronous mode
    UCB0I2COA = I2CADDRESS;         // Own Address is 048h
    UCB0CTL1 &= ~UCSWRST;           // Clear SW reset, resume operation
    UCB0I2CIE |= UCSTPIE| UCSTTIE;  // Enable STT, STP and
    IE2 |= UCB0RXIE|UCB0TXIE;       // RX/TX interrupts
}

void sleep (uint16_t time)
{
    TA0CTL |= TACLR;    // Clear timer and
    TA0CCR0 = time;     // set sleep duration
    TA0CTL |= MC0;      // Start timer in up mode and
    LPM0;               // sleep until times out...
}

const key_t *debounce (char key)
{
    const key_t *keypress;

    while((keypress = keyscan()) && keypress->key != key) { // Scan keys
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
    WDTCTL = WDTPW+WDTHOLD;             // Stop watchdog timer
    DCOCTL = CALDCO_16MHZ;              // Set DCO for 16MHz using
    BCSCTL1 = CALBC1_16MHZ;             // calibration registers
    BCSCTL2 = DIVS0|DIVS1;              // SMCLK = MCLK / 8

    TA0CTL |= TACLR|TASSEL1|ID0|ID1;    // SMCLK/8, Clear TA
    TA0CCTL0 |= CCIE;                   // Enable CCR0 interrupt

    _EINT();                            // Enable interrupts

    sleep(50);

    initI2C();

    P3DIR = 0xFF;                       // Set P3 pins to outputs
    P3OUT = 0xFF;                       // and set high

    select_map(DEFAULT_MAP);

    while(1) {

        keycode = '\0';
        keyclick = false;
        P1OUT |= keymap.drive_mask;                 // Drive all rows high
        P2DIR &= ~BUTINTR;                          // and clear button strobe (keydown, set to high-Z)
        P2IFG = 0;                                  // Clear any pending flags

        sleep(10000);                               // Wait a bit for things to settle

        if(!(P2IN & keymap.input_mask) || keypress->scanCode == keymap.shift_key) {                    // No key pressed?
            keypress = 0;
            P2IE |= keymap.input_mask;              // Yes, enable interrupts and
            LPM0;                                   // sleep until key pressed

            P2IE &= ~keymap.input_mask;             // Disable interrupts
        }

        sleep(10000);                               // Wait a bit for things to settle

        if(keyclick && (keypress = keyscan()))      // Key still pressed?
            keypress = debounce(keypress->key);     // Yep, debounce to ensure stable result

        if(keypress) {

            lastkey = '\0';
            autorepeat = 0;
            longPress = false;

            do {

                if(lastkey && keypress->key != lastkey) { // Key change?

                    keycode = '\0';
                    autorepeat = 0;
                    longPress = false;

                    if(P2DIR & BUTINTR) {   // for persistent key?
                        P2DIR &= ~BUTINTR;  // terminate key pressed signal
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
                        P2DIR |= BUTINTR;
                        P2OUT &= ~BUTINTR;
                        sleep(50); // 200 us
                        P2DIR &= ~BUTINTR;
                        autorepeat++;
                        sleep(autorepeat < 2 ? 64000 : 10000);
                        break;

                    case Keytype_Persistent:
                        keycode = keypress->key;
                        P2DIR |= BUTINTR;
                        P2OUT &= ~BUTINTR;
                        break;

                    default:
                        if(lastkey != keypress->key) {
                            keycode = keypress->key;
                            P2DIR |= BUTINTR;
                            P2OUT &= ~BUTINTR;
                            sleep(50); // 200 us
                            P2DIR &= ~BUTINTR;
                        }
                        break;
                }

                lastkey = keypress->key;

                if(keymap.powerkeys) switch(keycode) {

                    case '-':
                        if(laserpower > 0)
                            laserpower--;
                        break;

                    case '+':
                        if(laserpower < 100)
                            laserpower++;
                        break;
                }

                sleep(5000); // Wait for ~20 ms before transmitting again

                if(keypress->scanCode == keymap.shift_key)
                    break;

            } while((keypress = debounce(lastkey))); // Keep transmitting while button(s) held down

            if(keymap.powerkeys && (lastkey == '-' || lastkey == '+')) {
                keycode = 'P';
                P2DIR |= BUTINTR;
                P2OUT &= ~BUTINTR;
                sleep(50); // 200 us
            } else if(longPress) { // lowercaps key
                keycode = autorepeat < 5 ? lastkey | 0x20 : lastkey;
                P2DIR |= BUTINTR;
                P2OUT &= ~BUTINTR;
                sleep(50); // 200 us
            }
            P2DIR &= ~BUTINTR;                      // Clear button strobe
        }
    }
}

const key_t *keyscan (void)
{
    uint16_t scancode = 0;                          // Initialize scancode and
    uint16_t index = BIT0;                          // row mask

    if(!(P2IN & keymap.input_mask))                 // Keys still pressed?
        return 0;                                   // no, exit

    while(index & keymap.drive_mask) {              // Loop through all rows

        P1OUT &= ~keymap.drive_mask;                // Stop driving rows

        P2DIR |= keymap.input_mask;                 // Temporarily set column pins to output
        P2OUT &= ~keymap.input_mask;                // and switch low to bleed off charge
        P2DIR &= ~keymap.input_mask;                // in order to avoid erroneous reads
        P2OUT &= ~keymap.input_mask;                // Enable pull down resistors
        P2REN |= keymap.input_mask;                 // on column inputs

        P1OUT |= index;                             // Drive row

        __delay_cycles(16);

        if(P2IN & keymap.input_mask) {              // If any key pressed:
            scancode |= (index << keymap.rowshift); // set bit for row scanned
            scancode |= (P2IN & keymap.input_mask); // set bit(s) for column(s)
        }

        index = index << 1;                         // Next row
    }

    P1OUT |= keymap.drive_mask;                     // Drive all rows high again

    index = keymap.n_keys;

    if(scancode != 0)                                               // If key(s) were pressed
        while(index && keymap.key[--index].scanCode != scancode);   // then try to resolve to legal entry in lookup table

    return scancode != 0 && keymap.key[index].scanCode == scancode ? &keymap.key[index] : 0;
}

// P2.x Interrupt service routine
#pragma vector=PORT2_VECTOR
__interrupt void P2_ISR(void)
{
    P2IFG &= ~keymap.input_mask;                    // Clear button interrupt flag(s)

    if((P2IN & keymap.input_mask) && !keyclick) {   // Button interrupt while sleeping?
        keyclick = true;                            // Yep - set event semaphore and
        LPM0_EXIT;                                  // exit LPM0 on return
    }
}

// CCR0 Interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void CCR0_ISR(void)
{
    TA0CTL &= ~(MC0|MC1|TAIFG); // Stop debounce timer and clear
    TA0CCTL0 &= ~CCIFG;         // interrupt flags
    LPM0_EXIT;                  // Exit LPM0 on return
}

#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR(void)
{
    if(IFG2 & UCB0TXIFG) {
        UCB0TXBUF = getpower ? laserpower : keycode;      // Transmit current keycode
//      getpower = keymap.powerkeys;
        keycode = '\0';
    }

    if(IFG2 & UCB0RXIFG) {       // Save any received bytes
        if(i2c_rx.count < sizeof(i2c_rx.data)) {
            *i2c_rx.ptr++ = UCB0RXBUF;
            i2c_rx.count++;
        }
    }
}

#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR(void)
{
    uint8_t intstate = UCB0STAT;

    if(intstate & UCGC) {       // General call?
        UCB0STAT &= ~UCGC;      // Clear interrupt flag
        return;                 // and return
    }

    if(intstate & UCSTTIFG) {
        getpower = false;
        i2c_rx.count = 0;
        i2c_rx.ptr = i2c_rx.data;
    }

    if((intstate & UCSTPIFG)) switch(i2c_rx.count) {

        case 1: // Set LEDs
            P3OUT = i2c_rx.data[0];
            break;

        case 2:
            switch(i2c_rx.data[0]) {

                case 0:  // Select key map
                    select_map(i2c_rx.data[1]);
                    break;

                case 2: // Set power level
                    laserpower = i2c_rx.data[1];
                    break;
            }
            break;

        case 4: // add keymapping
            if(keymap.n_keys < N_KEYS_MAX) {
                keymap.key[keymap.n_keys].type = (keytype_t)i2c_rx.data[1];
                keymap.key[keymap.n_keys].scanCode = (i2c_rx.data[3] << 8) | i2c_rx.data[2];
                if((keymap.key[keymap.n_keys].key = i2c_rx.data[0]) == 0xFF)
                    keymap.shift_key = keymap.key[keymap.n_keys].scanCode;
                keymap.n_keys++;
            }
            break;
    }

    UCB0STAT &= ~intstate;      // Clear interrupt flags
}
