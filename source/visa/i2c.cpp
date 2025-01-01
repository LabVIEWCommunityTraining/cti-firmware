#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

using namespace SCPI;

const uint8_t i2c_buf_len = 100;
uint8_t i2c_buf[i2c_buf_len + 1]; // extra byte to accomodate null termination

CommandResult i2c_init(ScpiParser* scpi) {
    uint8_t bus = scpi->nodeNum(1);
    if (bus < 0) {
        return CommandResult::MissingParam;
    }

    uint32_t baud;
    uint8_t txPin;
    uint8_t rxPin;

    if (scpi->parseInt(baud) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(txPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(rxPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    baud = gPlatform.I2C.init(bus, baud, txPin, rxPin);

    return CommandResult::Success;
}

CommandResult i2c_write(ScpiParser* scpi) {
    uint8_t bus = scpi->nodeNum(1);
    if (bus < 0) {
        return CommandResult::MissingParam;
    }

    int len = 0;
    char* buf;
    int32_t addr;
    bool nostop = false;

    if (scpi->parseInt(addr) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseBlock(&buf, &len) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    scpi->parseBool(nostop); //optional nostop parameter to keep control of bus between transactions

    if (len > 0) {
        gPlatform.I2C.write(bus, addr, len, (uint8_t*) buf, nostop);
        //gPlatform.IO.Print(len, buf);
        //gPlatform.IO.Print('\n');

        return CommandResult::Success;
    }

    return CommandResult::SyntaxError;
}

QueryResult i2c_read(ScpiParser* scpi) {
    uint8_t uart = scpi->nodeNum(1);
    if (uart < 0) {
        return QueryResult::Error;
    }

    uint8_t len = 0;
    uint8_t addr;
    bool nostop = false;

    if (scpi->parseInt(addr) != ParseResult::Success) {
        return QueryResult::Error;
    }

    if (scpi->parseInt(len) != ParseResult::Success || len > i2c_buf_len) {
        return QueryResult::Error;
    }

    scpi->parseBool(nostop); // optional flag to not release bus after transaction

    len = gPlatform.I2C.read(uart, addr, len, i2c_buf, nostop);

    for (uint8_t i = 0; i < len; ++i) {
        gPlatform.IO.Print(i2c_buf[i]);
    }
    gPlatform.IO.Print('\n');

    return QueryResult::Success;
}

void initI2CCommands(Visa* visa) {
    visa->addCommand("COMM:I2C#:INIT", i2c_init, nullptr);
    visa->addCommand("COMM:I2C#:DATA", i2c_write, i2c_read);
}

} // namespace VISA
} // namespace CTI