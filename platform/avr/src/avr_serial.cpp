#include "avr_serial.h"
#include "avr_timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>

namespace {
constexpr uint8_t RxBufferSize = 32;
volatile uint8_t rxBuffer[RxBufferSize];
volatile uint8_t rxHead;
volatile uint8_t rxTail;

int uartPutchar(char value, FILE*) {
    while (!(UCSR0A & _BV(UDRE0))) {
    }
    UDR0 = static_cast<uint8_t>(value);
    return 0;
}

int uartGetchar(FILE*) {
    while (rxHead == rxTail) {
    }

    uint8_t value = rxBuffer[rxTail];
    rxTail = static_cast<uint8_t>((rxTail + 1) % RxBufferSize);
    return value;
}

FILE uartStream;
}

ISR(USART_RX_vect) {
    uint8_t next = static_cast<uint8_t>((rxHead + 1) % RxBufferSize);
    uint8_t value = UDR0;
    if (next != rxTail) {
        rxBuffer[rxHead] = value;
        rxHead = next;
    }
}

void initSerial(uint32_t baud) {
    uint16_t divisor = static_cast<uint16_t>((F_CPU + (baud * 4UL)) / (baud * 8UL) - 1UL);

    UCSR0A = _BV(U2X0);
    UBRR0H = static_cast<uint8_t>(divisor >> 8);
    UBRR0L = static_cast<uint8_t>(divisor);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);

    fdev_setup_stream(&uartStream, uartPutchar, uartGetchar, _FDEV_SETUP_RW);
    stdin = &uartStream;
    stdout = &uartStream;
    stderr = &uartStream;
}

int avrSerialGetcharTimeout(uint32_t timeoutUs) {
    uint32_t start = micros();
    while (rxHead == rxTail) {
        if (static_cast<uint32_t>(micros() - start) >= timeoutUs) {
            return -1;
        }
    }
    return uartGetchar(nullptr);
}
