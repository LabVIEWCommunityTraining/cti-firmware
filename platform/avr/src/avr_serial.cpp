#include "avr_serial.h"
#include "circularbuffer.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include <math.h>
#include <stdlib.h>

//#include <stdint.h>

CircularBuffer _rx(true);
CircularBuffer _tx(true);

ISR(USART_RX_vect) {
    _rx.put(UDR0);
}

ISR(USART_UDRE_vect) {
    if (_tx.isEmpty()) {
        UCSR0B &= ~(1<<UDRIE0);
    } else {
        UDR0 = _tx.get();
    }
}

static int uart_putchar(char c, FILE *stream) {
    if (c == '\n')
        uart_putchar('\r', stream);
    
    while (_tx.isFull());
    
    _tx.put(c);
    
    UCSR0B |= (1<<UDRIE0);
    
    return 0;
}

static int uart_getchar(FILE *stream) {
    while (_rx.isEmpty());
    
    return _rx.get();
}

static FILE *_uartStream;

void initSerial(long baud) {
    _uartStream = fdevopen(uart_putchar, uart_getchar);
    
    uint16_t baud_setting = (F_CPU / 4 / baud - 1) / 2;
    UCSR0A = 1 << U2X0;

    // hardcoded exception for 57600 for compatibility with the bootloader
    // shipped with the Duemilanove and previous boards and the firmware
    // on the 8U2 on the Uno and Mega 2560. Also, The baud_setting cannot
    // be > 4095, so switch back to non-u2x mode if the baud rate is too
    // low.
    if (((F_CPU == 16000000UL) && (baud == 57600)) || (baud_setting >4095)) {
        UCSR0A = 0;
        baud_setting = (F_CPU / 8 / baud - 1) / 2;
    }

    // assign the baud_setting, a.k.a. ubrr (USART Baud Rate Register)
    UBRR0H = baud_setting >> 8;
    UBRR0L = baud_setting;
    
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);

    UCSR0B |= (1<<RXEN0);
    UCSR0B |= (1<<TXEN0);
    UCSR0B |= (1<<RXCIE0);
    UCSR0B &= ~(1<<UDRIE0);
}

void printDec(const float &val) {
    if (isnan(val)) {
        printf("NaN");
    } else {
        printf("%c%d.%03d", (val < 0 ? '-' : ' '), abs((int)val), abs((int)(val * 1000) % 1000));
    }
}

void printDec(const float *val) {
    if (isnan(*val)) {
        printf("NaN");
    } else {
        printf("%c%d.%03d", (val < 0 ? '-' : ' '), abs((int)*val), abs((int)(*val * 1000) % 1000));
    }
}

void printDec(volatile float *val) {
    if (isnan(*val)) {
        printf("NaN");
    } else {
        printf("%c%d.%03d", (val < 0 ? '-' : ' '), abs((int)*val), abs((int)(*val * 1000) % 1000));
    }
}