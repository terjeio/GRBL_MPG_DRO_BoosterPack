/*
 * serial.c - HAL serial driver for Raspberry RP2040 ARM processor
 *
 * Part of MPG/DRO for grbl on a secondary processor
 *
 * v0.0.2 / 2022-01-28 / (c) Io Engineering / Terje
 */

/*

Copyright (c) 2021-2022, Terje Io
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

#include <string.h>

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#include "../src/interface.h"

#include "serial.h"
#include "driver.h"
#include "driver.h"

#define UART ((uart_hw_t *)UART_PORT)

#define BUFNEXT(ptr, buffer) ((ptr + 1) & (sizeof(buffer.data) - 1))
#define BUFCOUNT(head, tail, size) ((head >= tail) ? (head - tail) : (size - tail + head))
#define STREAM_BUFFER_SIZE 512

typedef struct {
    volatile uint_fast16_t head;
    volatile uint_fast16_t tail;
    bool overflow;
    char data[STREAM_BUFFER_SIZE];
} stream_buffer_t;

static uint16_t tx_fifo_size;
static stream_buffer_t txbuffer = {0};
static stream_buffer_t rxbuffer = {0};

static void uart_interrupt_handler (void);

void serial_init (void)
{
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    uart_init(UART_PORT, 2400);

    uart_set_hw_flow(UART_PORT, false, false);
    uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);
    uart_set_baudrate(UART_PORT, 115200);
    uart_set_fifo_enabled(UART_PORT, true);

    irq_set_exclusive_handler(UART_IRQ, uart_interrupt_handler);
    irq_set_enabled(UART_IRQ, true);
    
    hw_set_bits(&UART->imsc, UART_UARTIMSC_RXIM_BITS|UART_UARTIMSC_RTIM_BITS);             
}

static bool serialBlockingCallbackDummy (void)
{
    return true;
}

void setSerialBlockingCallback (bool (*fn)(void))
{
    interface.on_serial_block = fn == NULL ? serialBlockingCallbackDummy : fn;
}

//
// serialReceiveCallback - called before data is inserted into the buffer, return false to drop
//

void setSerialReceiveCallback (bool (*fn)(char))
{
 //   serialReceiveCallback = fn;
}

//
// serialGetC - returns -1 if no data available
//

int16_t serial_getC (void)
{
    int16_t data;
    uint_fast16_t bptr = rxbuffer.tail;

    if(bptr == rxbuffer.head)
        return -1; // no data available else EOF

    data = rxbuffer.data[bptr];                 // Get next character, increment tmp pointer
    rxbuffer.tail = BUFNEXT(bptr, rxbuffer);    // and update pointer

    return data;
}

inline static uint16_t serialRxCount (void)
{
    uint_fast16_t head = rxbuffer.head, tail = rxbuffer.tail;

    return BUFCOUNT(head, tail, RX_BUFFER_SIZE);
}

uint16_t static serialRxFree (void)
{
    return STREAM_BUFFER_SIZE - 1 - serialRxCount();
}

void serialRxFlush (void)
{
    rxbuffer.tail = rxbuffer.head;
    rxbuffer.overflow = false;
}

void serial_RxCancel (void)
{
    serialRxFlush();
    rxbuffer.data[rxbuffer.head] = ASCII_CAN;
    rxbuffer.head = BUFNEXT(rxbuffer.head, rxbuffer);
}

void serial_writeS (const char *data)
{
    char c, *ptr = (char *)data;

    while((c = *ptr++) != '\0')
        serial_putC(c);

}

void serial_writeLn (const char *data)
{
    serial_writeS(data);
    serial_writeS(ASCII_EOL);
}

void serial_write (const char *data, unsigned int length)
{
    char *ptr = (char *)data;

    while(length--)
        serial_putC(*ptr++);

}

#ifdef LINE_BUFFER_SIZE

char *serialReadLn (void) {

    static char cmdbuf[LINE_BUFFER_SIZE];

    int32_t c = 0;
    uint32_t count = 0;

    while(c != ASCII_CR) {
        if((c = serialGetC()) != -1) {

            if(c == CR)
                cmdbuf[count] = '\0';
            else if(c > 31 && count < sizeof(cmdbuf))
                cmdbuf[count++] = (char)c;
            else if(c == ASCII_EOF)
                c = ASCII_CR;
            else if(c == DEL && count > 0)
                count--;
        }
    }

    return count ? cmdbuf : 0;
}
#endif

bool serial_putC (const char c)
{
    uint_fast16_t next_head;

    if(!(UART->imsc & UART_UARTIMSC_TXIM_BITS)) {                   // If the transmit interrupt is deactivated
        if(!(UART->fr & UART_UARTFR_TXFF_BITS)) {                   // and if the TX FIFO is not full
            UART->dr = c;                                           // Write data in the TX FIFO
            return true;
        } else
            hw_set_bits(&UART->imsc, UART_UARTIMSC_TXIM_BITS);      // Enable transmit interrupt
    }

    // Write data in the Buffer is transmit interrupt activated or TX FIFO is                                                                
    next_head = BUFNEXT(txbuffer.head, txbuffer);  // Get and update head pointer

    while(txbuffer.tail == next_head) {                             // Buffer full, block until space is available...
   //     if(!hal.stream_blocking_callback())
            return false;
    }

    txbuffer.data[txbuffer.head] = c;                               // Add data to buffer
    txbuffer.head = next_head;                                      // and update head pointer

    return true;
}

uint16_t serialTxCount(void)
{
    uint_fast16_t head = txbuffer.head, tail = txbuffer.tail;

    return BUFCOUNT(head, tail, TX_BUFFER_SIZE) + ((UART->fr & UART_UARTFR_BUSY_BITS) ? 0 : 1);

}

static void uart_interrupt_handler (void)
{
    uint_fast16_t bptr;
    uint32_t data, ctrl = UART->mis;

    if(ctrl & (UART_UARTMIS_RXMIS_BITS | UART_UARTIMSC_RTIM_BITS)) {

        while (uart_is_readable(UART_PORT)) {

            bptr = BUFNEXT(rxbuffer.head, rxbuffer);  // Get next head pointer
            data = UART->dr & 0xFF;                             // and read input (use only 8 bits of data)

            if(bptr == rxbuffer.tail) {                         // If buffer full
                rxbuffer.overflow = true;                       // flag overflow
            } else {
#if MODBUS_ENABLE
                rxbuffer.data[rxbuffer.head] = (char)data;  // Add data to buffer
                rxbuffer.head = bptr;                       // and update pointer
#else
            //    iif(!hal.stream.enqueue_realtime_command((char)data)) {
                    rxbuffer.data[rxbuffer.head] = (char)data;  // Add data to buffer
                    rxbuffer.head = bptr;                       // and update pointer
             //   }
#endif
            }
        }
    }

    // Interrupt if the TX FIFO is lower or equal to the empty TX FIFO threshold
    if(ctrl & UART_UARTMIS_TXMIS_BITS)
    {
        bptr = txbuffer.tail;

        // As long as the TX FIFO is not full or the buffer is not empty
        while((!(UART->fr & UART_UARTFR_TXFF_BITS)) && (txbuffer.head != bptr)) {
            UART->dr = txbuffer.data[bptr];                         // Put character in TX FIFO
            bptr = BUFNEXT(bptr, txbuffer);        // and update tmp tail pointer
        }

        txbuffer.tail = bptr;                                       //  Update tail pointer

        if(txbuffer.tail == txbuffer.head)						    // Disable TX interrupt when the TX buffer is empty
            hw_clear_bits(&UART->imsc, UART_UARTIMSC_TXIM_BITS);
    }

}
