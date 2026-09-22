#include "cti/platform.h"
#include "avr_serial.h"

#include <avr/io.h>

namespace {
struct PinRegisters {
    volatile uint8_t* input;
    volatile uint8_t* output;
    volatile uint8_t* direction;
    uint8_t mask;
};

bool registersForPin(CTI::ChanIndex pin, PinRegisters& registers) {
    if (pin >= 0 && pin <= 7) {
        registers = {&PIND, &PORTD, &DDRD, static_cast<uint8_t>(_BV(pin))};
        return true;
    }
    if (pin >= 8 && pin <= 13) {
        registers = {&PINB, &PORTB, &DDRB, static_cast<uint8_t>(_BV(pin - 8))};
        return true;
    }
    if (pin >= 14 && pin <= 19) {
        registers = {&PINC, &PORTC, &DDRC, static_cast<uint8_t>(_BV(pin - 14))};
        return true;
    }
    return false;
}
}

int CTI::PlatformIO::_getchar_timeout_us(uint32_t timeoutUs) {
    return avrSerialGetcharTimeout(timeoutUs);
}

CTI::LVBlock availableGPIOs = {22, {
    0x00, 0x00, 0x00, 0x12,
    2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19
}};

CTI::LVBlock* CTI::PlatformDigital::Available() {
    return &availableGPIOs;
}

void CTI::PlatformDigital::SetOutput(ChanIndex channel, bool value) {
    if (channel == 0 || channel == 1) {
        return;
    }
    PinRegisters registers;
    if (!registersForPin(channel, registers)) {
        return;
    }
    if (value) {
        *registers.output |= registers.mask;
    } else {
        *registers.output &= static_cast<uint8_t>(~registers.mask);
    }
}

void CTI::PlatformDigital::SetDirection(ChanIndex channel, bool output) {
    if (channel == 0 || channel == 1) {
        return;
    }
    PinRegisters registers;
    if (!registersForPin(channel, registers)) {
        return;
    }
    if (output) {
        *registers.direction |= registers.mask;
    } else {
        *registers.direction &= static_cast<uint8_t>(~registers.mask);
    }
}

void CTI::PlatformDigital::SetPull(ChanIndex channel, PullDirection direction) {
    if (channel == 0 || channel == 1) {
        return;
    }
    PinRegisters registers;
    if (!registersForPin(channel, registers)) {
        return;
    }
    if (direction == Up) {
        *registers.direction &= static_cast<uint8_t>(~registers.mask);
        *registers.output |= registers.mask;
    } else {
        *registers.output &= static_cast<uint8_t>(~registers.mask);
    }
}

void CTI::PlatformDigital::GetValue(ChanIndex channel, bool* value) {
    PinRegisters registers;
    *value = registersForPin(channel, registers) && ((*registers.input & registers.mask) != 0);
}

void CTI::PlatformDigital::GetDirection(ChanIndex channel, bool* output) {
    PinRegisters registers;
    *output = registersForPin(channel, registers) && ((*registers.direction & registers.mask) != 0);
}

void CTI::PlatformDigital::GetPull(ChanIndex channel, PullDirection* direction) {
    PinRegisters registers;
    if (!registersForPin(channel, registers)) {
        *direction = None;
        return;
    }
    bool input = (*registers.direction & registers.mask) == 0;
    *direction = input && ((*registers.output & registers.mask) != 0) ? Up : None;
}

CTI::LVBlock availableAnalogInputs = {16, {
    0x00, 0x00, 0x00, 0x06,
    0, 14,
    1, 15,
    2, 16,
    3, 17,
    4, 18,
    5, 19
}};

CTI::LVBlock* CTI::PlatformAnalog::Available() {
    return &availableAnalogInputs;
}

void CTI::PlatformAnalog::EnableInput(ChanIndex channel) {
    if (channel >= 0 && channel <= 5) {
        DIDR0 |= static_cast<uint8_t>(_BV(channel));
    }
}

void CTI::PlatformAnalog::GetInput(ChanIndex channel, uint16_t* value) {
    if (channel < 0 || channel > 5) {
        *value = 0;
        return;
    }

    ADMUX = _BV(REFS0) | static_cast<uint8_t>(channel);
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }
    *value = ADC;
}
