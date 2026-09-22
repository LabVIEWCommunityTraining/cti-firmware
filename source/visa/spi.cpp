#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

using namespace SCPI;

#ifndef CTI_IO_BUFFER_LENGTH
#define CTI_IO_BUFFER_LENGTH 255
#endif

const uint8_t spi_buf_len = CTI_IO_BUFFER_LENGTH;
uint8_t spi_buf[spi_buf_len + 1]; // extra byte to accomodate null termination

CommandResult spi_init(ScpiParser* scpi) {
    ChanIndex bus = scpi->nodeNum(1);
    if (bus < 0) {
        errSuffixOutOfRange(scpi);
        return CommandResult::Error;
    }

    uint32_t baud;
    uint8_t bits;
    uint8_t misoPin;
    uint8_t mosiPin;
    uint8_t sckPin;
    uint8_t spiMode;

    if (scpi->parseInt(baud) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(spiMode) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(bits) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(mosiPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(misoPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(sckPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    baud = gPlatform.SPI.init(bus, baud, spiMode, bits, mosiPin, misoPin, sckPin);
    if (baud == 0) {
        errParamOutOfRange(scpi);
        return CommandResult::Error;
    }

    return CommandResult::Success;
}

CommandResult spi_write(ScpiParser* scpi) {
    ChanIndex bus = scpi->nodeNum(1);
    if (bus < 0) {
        errSuffixOutOfRange(scpi);
        return CommandResult::Error;
    }

    int len = 0;
    char* buf;

    if (scpi->parseBlock(&buf, &len) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (len > 0) {
        if (gPlatform.SPI.write(bus, len, (uint8_t*)buf) != static_cast<size_t>(len)) {
            errCommand(scpi);
            return CommandResult::Error;
        }
        //gPlatform.IO.Print(len, buf);
        //gPlatform.IO.Print('\n');

        return CommandResult::Success;
    }

    errParamOutOfRange(scpi);
    return CommandResult::Error;
}

QueryResult spi_available(ScpiParser* scpi) {
    LVBlock* spis = gPlatform.SPI.Available();

    PrintBlock(spis->len, spis->buffer);
    gPlatform.IO.Print('\n');

    return QueryResult::Success;
}

QueryResult spi_read(ScpiParser* scpi) {
    ChanIndex bus = scpi->nodeNum(1);
    if (bus < 0) {
        errSuffixOutOfRange(scpi);
        return QueryResult::Error;
    }

    uint8_t len = 0;
    char* data = 0;
    int dataLen = 0;

    if (scpi->parseInt(len) != ParseResult::Success) {
        return QueryResult::MissingParam;
    }  

    scpi->parseBlock(&data, &dataLen);

    if (dataLen > 0 && dataLen != len) {
        errParamOutOfRange(scpi);
        return QueryResult::Error;
    }

    uint8_t requested = len;
    len = gPlatform.SPI.read(bus, len, spi_buf, (uint8_t*)data);
    if (len != requested) {
        errCommand(scpi);
        return QueryResult::Error;
    }

    PrintBlock(len, spi_buf);
    gPlatform.IO.Print('\n');

    return QueryResult::Success;
}

void initSPICommands(Visa* visa) {

    visa->addCommand("COMM:SPI:AVAILable", nullptr, spi_available);

    /**
     * COMM:SPI#:INIT <BAUD>,<MODE>,<BITS>,<MOSI>,<MISO>,<SCK>,<CS>
     */
    visa->addCommand("COMM:SPI#:INIT", spi_init, nullptr);

    /**
     * COMM:SPI#:DATA <DATA>
     * COMM:SPI#:DATA? <LEN>,[DATA]
     */
    visa->addCommand("COMM:SPI#:DATA", spi_write, spi_read);
}

} // namespace VISA
} // namespace CTI