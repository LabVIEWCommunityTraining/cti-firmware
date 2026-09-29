#include "cti/platform/comms.h"

#include <avr/io.h>
#include <util/twi.h>

namespace {
uint8_t spiTransfer(uint8_t value) {
    SPDR = value;
    while (!(SPSR & _BV(SPIF))) {
    }
    return SPDR;
}

bool twiStart(uint8_t address) {
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    while (!(TWCR & _BV(TWINT))) {
    }
    uint8_t status = TW_STATUS;
    if (status != TW_START && status != TW_REP_START) {
        return false;
    }

    TWDR = address;
    TWCR = _BV(TWINT) | _BV(TWEN);
    while (!(TWCR & _BV(TWINT))) {
    }
    status = TW_STATUS;
    return status == TW_MT_SLA_ACK || status == TW_MR_SLA_ACK;
}

void twiStop() {
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
}
}

CTI::PlatformUART::PlatformUART() {
}

CTI::LVBlock availableUarts = {4, {0, 0, 0, 0}};

CTI::LVBlock* CTI::PlatformUART::Available() {
    return &availableUarts;
}

uint8_t CTI::PlatformUART::termChar(uint8_t) {
    return 0;
}

uint32_t CTI::PlatformUART::init(uint8_t, uint32_t, int8_t, int8_t, uint8_t) {
    return 0;
}

size_t CTI::PlatformUART::write(uint8_t, size_t, const uint8_t*) {
    return 0;
}

size_t CTI::PlatformUART::read(uint8_t, size_t, uint8_t*) {
    return 0;
}

CTI::PlatformI2C::PlatformI2C() {
}

CTI::LVBlock availableI2c = {15, {
    0x00, 0x00, 0x00, 0x01,
    0x00,
    0x00, 0x00, 0x00, 0x01, 19,
    0x00, 0x00, 0x00, 0x01, 18
}};

CTI::LVBlock* CTI::PlatformI2C::Available() {
    return &availableI2c;
}

uint32_t CTI::PlatformI2C::init(uint8_t bus, uint32_t baud, int8_t sclPin, int8_t sdaPin) {
    if (bus != 0 || sclPin != 19 || sdaPin != 18 || baud == 0) {
        return 0;
    }

    TWSR = 0;
    uint32_t divider = baud >= F_CPU / 16UL
        ? 0
        : ((F_CPU / baud) - 16UL) / 2UL;
    if (divider > 255) {
        divider = 255;
    }
    TWBR = static_cast<uint8_t>(divider);
    TWCR = _BV(TWEN);
    return F_CPU / (16UL + 2UL * TWBR);
}

size_t CTI::PlatformI2C::write(
    uint8_t bus, uint8_t address, size_t length, const uint8_t* data, bool noStop) {
    if (bus != 0 || address > 0x7f || !twiStart(static_cast<uint8_t>(address << 1))) {
        twiStop();
        return 0;
    }

    size_t written = 0;
    while (written < length) {
        TWDR = data[written];
        TWCR = _BV(TWINT) | _BV(TWEN);
        while (!(TWCR & _BV(TWINT))) {
        }
        if (TW_STATUS != TW_MT_DATA_ACK) {
            break;
        }
        ++written;
    }

    if (!noStop || written != length) {
        twiStop();
    }
    return written;
}

size_t CTI::PlatformI2C::read(
    uint8_t bus, uint8_t address, size_t length, uint8_t* buffer, bool noStop) {
    if (bus != 0 || address > 0x7f ||
        !twiStart(static_cast<uint8_t>((address << 1) | TW_READ))) {
        twiStop();
        return 0;
    }

    size_t received = 0;
    while (received < length) {
        bool acknowledge = received + 1 < length;
        TWCR = _BV(TWINT) | _BV(TWEN) | (acknowledge ? _BV(TWEA) : 0);
        while (!(TWCR & _BV(TWINT))) {
        }
        uint8_t expected = acknowledge ? TW_MR_DATA_ACK : TW_MR_DATA_NACK;
        if (TW_STATUS != expected) {
            break;
        }
        buffer[received++] = TWDR;
    }

    if (!noStop || received != length) {
        twiStop();
    }
    return received;
}

CTI::PlatformSPI::PlatformSPI() {
}

CTI::LVBlock availableSpi = {20, {
    0x00, 0x00, 0x00, 0x01,
    0x00,
    0x00, 0x00, 0x00, 0x01, 11,
    0x00, 0x00, 0x00, 0x01, 12,
    0x00, 0x00, 0x00, 0x01, 13
}};

CTI::LVBlock* CTI::PlatformSPI::Available() {
    return &availableSpi;
}

uint32_t CTI::PlatformSPI::init(
    uint8_t bus, uint32_t baud, uint8_t mode, uint8_t bits,
    int8_t mosiPin, int8_t misoPin, int8_t sckPin) {
    if (bus != 0 || baud == 0 || mode > 3 || bits != 8 ||
        mosiPin != 11 || misoPin != 12 || sckPin != 13) {
        return 0;
    }

    DDRB |= _BV(DDB2) | _BV(DDB3) | _BV(DDB5);
    DDRB &= static_cast<uint8_t>(~_BV(DDB4));
    PORTB |= _BV(PORTB2);

    const uint16_t dividers[] = {2, 4, 8, 16, 32, 64, 128};
    uint16_t divider = 128;
    for (uint8_t i = 0; i < sizeof(dividers) / sizeof(dividers[0]); ++i) {
        if (F_CPU / dividers[i] <= baud) {
            divider = dividers[i];
            break;
        }
    }

    uint8_t control = _BV(SPE) | _BV(MSTR);
    if (mode & 1) {
        control |= _BV(CPHA);
    }
    if (mode & 2) {
        control |= _BV(CPOL);
    }

    uint8_t status = 0;
    switch (divider) {
        case 2: status = _BV(SPI2X); break;
        case 4: break;
        case 8: control |= _BV(SPR0); status = _BV(SPI2X); break;
        case 16: control |= _BV(SPR0); break;
        case 32: control |= _BV(SPR1); status = _BV(SPI2X); break;
        case 64: control |= _BV(SPR1); break;
        default: control |= _BV(SPR1) | _BV(SPR0); break;
    }
    SPCR = control;
    SPSR = status;
    return F_CPU / divider;
}

size_t CTI::PlatformSPI::write(uint8_t bus, size_t length, const uint8_t* data) {
    if (bus != 0 || !(SPCR & _BV(SPE))) {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        spiTransfer(data[i]);
    }
    return length;
}

size_t CTI::PlatformSPI::read(
    uint8_t bus, size_t length, uint8_t* buffer, const uint8_t* data) {
    if (bus != 0 || !(SPCR & _BV(SPE))) {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        buffer[i] = spiTransfer(data ? data[i] : 0);
    }
    return length;
}
