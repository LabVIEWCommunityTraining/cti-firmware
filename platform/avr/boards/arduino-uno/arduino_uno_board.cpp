#include "cti/platform.h"
#include "cti_board.h"

#include <avr/io.h>

void CTI::Platform::BoardInit() {
}

void CTI::PlatformIO::InitStatusLED() {
    DDRB |= _BV(DDB5);
}

void CTI::PlatformIO::_statusLED(bool value) {
    if (value) {
        PORTB |= _BV(PORTB5);
    } else {
        PORTB &= static_cast<uint8_t>(~_BV(PORTB5));
    }
}
