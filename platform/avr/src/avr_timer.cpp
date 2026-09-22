#include "avr_timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

namespace {
volatile uint32_t timer0OverflowCount;
volatile uint32_t timer0Millis;
volatile uint8_t timer0Fraction;
}

ISR(TIMER0_OVF_vect) {
    uint32_t millisValue = timer0Millis + 1;
    uint8_t fraction = static_cast<uint8_t>(timer0Fraction + 3);

    if (fraction >= 125) {
        fraction = static_cast<uint8_t>(fraction - 125);
        ++millisValue;
    }

    timer0Fraction = fraction;
    timer0Millis = millisValue;
    ++timer0OverflowCount;
}

uint32_t millis() {
    uint32_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        value = timer0Millis;
    }
    return value;
}

uint32_t micros() {
    uint32_t overflows;
    uint8_t counter;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        overflows = timer0OverflowCount;
        counter = TCNT0;
        if ((TIFR0 & _BV(TOV0)) && counter < 255) {
            ++overflows;
        }
    }

    return ((overflows << 8) + counter) * 4UL;
}

void initTimer() {
    TCCR0A = _BV(WGM01) | _BV(WGM00);
    TCCR0B = _BV(CS01) | _BV(CS00);
    TIMSK0 = _BV(TOIE0);
}
