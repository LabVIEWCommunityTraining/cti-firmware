#include "cti/platform.h"

#include <avr/io.h>

namespace {
bool isPwmPin(CTI::ChanIndex pin) {
    return pin == 3 || pin == 5 || pin == 6 || pin == 9 || pin == 10 || pin == 11;
}

uint16_t timer1Divider() {
    switch (TCCR1B & 0x07) {
        case 1: return 1;
        case 2: return 8;
        case 3: return 64;
        case 4: return 256;
        case 5: return 1024;
        default: return 0;
    }
}

uint16_t timer2Divider() {
    switch (TCCR2B & 0x07) {
        case 1: return 1;
        case 2: return 8;
        case 3: return 32;
        case 4: return 64;
        case 5: return 128;
        case 6: return 256;
        case 7: return 1024;
        default: return 0;
    }
}

uint8_t timer1ClockBits(uint16_t divider) {
    switch (divider) {
        case 1: return 1;
        case 8: return 2;
        case 64: return 3;
        case 256: return 4;
        case 1024: return 5;
        default: return 0;
    }
}

uint8_t timer2ClockBits(uint16_t divider) {
    switch (divider) {
        case 1: return 1;
        case 8: return 2;
        case 32: return 3;
        case 64: return 4;
        case 128: return 5;
        case 256: return 6;
        case 1024: return 7;
        default: return 0;
    }
}
}

CTI::LVBlock availablePwms = {10, {
    0x00, 0x00, 0x00, 0x06, 3, 5, 6, 9, 10, 11
}};

CTI::LVBlock* CTI::PlatformPWM::Available() {
    return &availablePwms;
}

bool CTI::PlatformPWM::InitPWM(ChanIndex pin, bool phaseCorrect, bool enable) {
    if (!isPwmPin(pin)) {
        return false;
    }

    if (pin == 5 || pin == 6) {
        if (phaseCorrect) {
            return false;
        }
        if (pin == 5) {
            TCCR0A = (TCCR0A & static_cast<uint8_t>(~_BV(COM0B0))) | _BV(COM0B1);
            DDRD |= _BV(DDD5);
        } else {
            TCCR0A = (TCCR0A & static_cast<uint8_t>(~_BV(COM0A0))) | _BV(COM0A1);
            DDRD |= _BV(DDD6);
        }
    } else if (pin == 9 || pin == 10) {
        uint8_t outputs = TCCR1A & (_BV(COM1A1) | _BV(COM1B1));
        TCCR1A = outputs | _BV(WGM10);
        TCCR1B = (phaseCorrect ? 0 : _BV(WGM12)) | _BV(CS11) | _BV(CS10);
        if (pin == 9) {
            TCCR1A |= _BV(COM1A1);
            DDRB |= _BV(DDB1);
        } else {
            TCCR1A |= _BV(COM1B1);
            DDRB |= _BV(DDB2);
        }
    } else {
        uint8_t outputs = TCCR2A & (_BV(COM2A1) | _BV(COM2B1));
        TCCR2A = outputs | _BV(WGM20) | (phaseCorrect ? 0 : _BV(WGM21));
        TCCR2B = _BV(CS22);
        if (pin == 3) {
            TCCR2A |= _BV(COM2B1);
            DDRD |= _BV(DDD3);
        } else {
            TCCR2A |= _BV(COM2A1);
            DDRB |= _BV(DDB3);
        }
    }

    SetDuty(pin, 0);
    SetEnable(pin, enable);
    return true;
}

bool CTI::PlatformPWM::SetDuty(ChanIndex pin, float duty) {
    if (!isPwmPin(pin) || duty < 0.0f || duty > 1.0f) {
        return false;
    }

    uint16_t top = (pin == 9 || pin == 10) && (TCCR1B & _BV(WGM13)) ? ICR1 : 255;
    uint16_t level = static_cast<uint16_t>(duty * top + 0.5f);
    switch (pin) {
        case 3: OCR2B = static_cast<uint8_t>(level); break;
        case 5: OCR0B = static_cast<uint8_t>(level); break;
        case 6: OCR0A = static_cast<uint8_t>(level); break;
        case 9: OCR1A = level; break;
        case 10: OCR1B = level; break;
        case 11: OCR2A = static_cast<uint8_t>(level); break;
        default: return false;
    }
    return true;
}

bool CTI::PlatformPWM::SetFreq(ChanIndex pin, float frequency) {
    if (!isPwmPin(pin) || frequency <= 0.0f || pin == 5 || pin == 6) {
        return false;
    }

    if (pin == 9 || pin == 10) {
        float dutyA = GetDuty(9);
        float dutyB = GetDuty(10);
        const uint16_t dividers[] = {1, 8, 64, 256, 1024};
        uint16_t divider = 0;
        uint32_t top = 0;
        for (uint8_t i = 0; i < sizeof(dividers) / sizeof(dividers[0]); ++i) {
            uint32_t candidate = static_cast<uint32_t>(F_CPU / (frequency * dividers[i]) - 1);
            if (candidate >= 2 && candidate <= 65535) {
                divider = dividers[i];
                top = candidate;
                break;
            }
        }
        if (!divider) {
            return false;
        }
        TCCR1A = (TCCR1A & (_BV(COM1A1) | _BV(COM1B1))) | _BV(WGM11);
        TCCR1B = _BV(WGM13) | _BV(WGM12) | timer1ClockBits(divider);
        ICR1 = static_cast<uint16_t>(top);
        SetDuty(9, dutyA);
        SetDuty(10, dutyB);
        return true;
    }

    const uint16_t dividers[] = {1, 8, 32, 64, 128, 256, 1024};
    uint16_t bestDivider = 1;
    float periodCounts = (TCCR2A & _BV(WGM21)) ? 256.0f : 510.0f;
    float bestError = 3.4e38f;
    for (uint8_t i = 0; i < sizeof(dividers) / sizeof(dividers[0]); ++i) {
        float actual = static_cast<float>(F_CPU) / (dividers[i] * periodCounts);
        float error = actual > frequency ? actual - frequency : frequency - actual;
        if (error < bestError) {
            bestError = error;
            bestDivider = dividers[i];
        }
    }
    TCCR2B = timer2ClockBits(bestDivider);
    return true;
}

void CTI::PlatformPWM::SetEnable(ChanIndex pin, bool enable) {
    uint8_t mask = 0;
    volatile uint8_t* control = nullptr;
    switch (pin) {
        case 3: control = &TCCR2A; mask = _BV(COM2B1); break;
        case 5: control = &TCCR0A; mask = _BV(COM0B1); break;
        case 6: control = &TCCR0A; mask = _BV(COM0A1); break;
        case 9: control = &TCCR1A; mask = _BV(COM1A1); break;
        case 10: control = &TCCR1A; mask = _BV(COM1B1); break;
        case 11: control = &TCCR2A; mask = _BV(COM2A1); break;
        default: return;
    }
    if (enable) {
        *control |= mask;
    } else {
        *control &= static_cast<uint8_t>(~mask);
    }
}

bool CTI::PlatformPWM::SetTop(ChanIndex pin, uint16_t top) {
    if ((pin != 9 && pin != 10) || top < 2) {
        return false;
    }
    float dutyA = GetDuty(9);
    float dutyB = GetDuty(10);
    uint8_t clockBits = TCCR1B & 0x07;
    TCCR1A = (TCCR1A & (_BV(COM1A1) | _BV(COM1B1))) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | clockBits;
    ICR1 = top;
    SetDuty(9, dutyA);
    SetDuty(10, dutyB);
    return true;
}

bool CTI::PlatformPWM::SetDivider(ChanIndex pin, float divider) {
    uint16_t integerDivider = static_cast<uint16_t>(divider);
    if (divider != integerDivider) {
        return false;
    }
    if (pin == 9 || pin == 10) {
        uint8_t bits = timer1ClockBits(integerDivider);
        if (!bits) {
            return false;
        }
        TCCR1B = (TCCR1B & static_cast<uint8_t>(~0x07)) | bits;
        return true;
    }
    if (pin == 3 || pin == 11) {
        uint8_t bits = timer2ClockBits(integerDivider);
        if (!bits) {
            return false;
        }
        TCCR2B = (TCCR2B & static_cast<uint8_t>(~0x07)) | bits;
        return true;
    }
    return false;
}

float CTI::PlatformPWM::GetDuty(ChanIndex pin) {
    uint16_t top = (pin == 9 || pin == 10) && (TCCR1B & _BV(WGM13)) ? ICR1 : 255;
    uint16_t level;
    switch (pin) {
        case 3: level = OCR2B; break;
        case 5: level = OCR0B; break;
        case 6: level = OCR0A; break;
        case 9: level = OCR1A; break;
        case 10: level = OCR1B; break;
        case 11: level = OCR2A; break;
        default: return 0.0f;
    }
    return top ? static_cast<float>(level) / top : 0.0f;
}

float CTI::PlatformPWM::GetFreq(ChanIndex pin) {
    if (pin == 5 || pin == 6) {
        return static_cast<float>(F_CPU) / (64.0f * 256.0f);
    }
    if (pin == 9 || pin == 10) {
        uint16_t divider = timer1Divider();
        if (!divider) {
            return 0.0f;
        }
        if (TCCR1B & _BV(WGM13)) {
            return static_cast<float>(F_CPU) / (divider * (ICR1 + 1.0f));
        }
        float counts = (TCCR1B & _BV(WGM12)) ? 256.0f : 510.0f;
        return static_cast<float>(F_CPU) / (divider * counts);
    }
    if (pin == 3 || pin == 11) {
        uint16_t divider = timer2Divider();
        float counts = (TCCR2A & _BV(WGM21)) ? 256.0f : 510.0f;
        return divider ? static_cast<float>(F_CPU) / (divider * counts) : 0.0f;
    }
    return 0.0f;
}
