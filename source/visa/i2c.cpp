#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

using namespace SCPI;

const uint8_t i2c_buf_len = 255;
uint8_t i2c_buf[i2c_buf_len + 1]; // extra byte to accomodate null termination

CommandResult i2c_init(ScpiParser* scpi) {
    uint8_t bus = scpi->nodeNum(1);
    if (bus < 0) {
        errSuffixOutOfRange(scpi);
        return CommandResult::Error;
    }

    uint32_t baud;
    uint8_t sclPin;
    uint8_t sdaPin;

    if (scpi->parseInt(baud) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(sclPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(sdaPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    baud = gPlatform.I2C.init(bus, baud, sclPin, sdaPin);

    return CommandResult::Success;
}

CommandResult i2c_write(ScpiParser* scpi) {
    uint8_t bus = scpi->nodeNum(1);
    if (bus < 0) {
        errSuffixOutOfRange(scpi);
        return CommandResult::Error;
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

QueryResult i2c_available(ScpiParser* scpi) {
    LVBlock* i2cs = gPlatform.I2C.Available();

    PrintBlock(i2cs->len, i2cs->buffer);
    gPlatform.IO.Print('\n');
    
    return QueryResult::Success;
}

QueryResult i2c_read(ScpiParser* scpi) {
    uint8_t bus = scpi->nodeNum(1);
    if (bus < 0) {
        errSuffixOutOfRange(scpi);
        return QueryResult::Error;
    }

    uint8_t len = 0;
    uint8_t addr;
    bool nostop = false;

    if (scpi->parseInt(addr) != ParseResult::Success) {
        return QueryResult::MissingParam;
    }

    if (scpi->parseInt(len) != ParseResult::Success) {
        return QueryResult::MissingParam;
    }

    if (len > i2c_buf_len) {
        errParamOutOfRange(scpi);
        return QueryResult::Error;
    }

    scpi->parseBool(nostop); // optional flag to not release bus after transaction

    len = gPlatform.I2C.read(bus, addr, len, i2c_buf, nostop);

    PrintBlock(len, i2c_buf);
    gPlatform.IO.Print('\n');

    return QueryResult::Success;
}

void initI2CCommands(Visa* visa) {
    visa->addCommand("COMM:I2C:AVAILable", nullptr, i2c_available);
    visa->addCommand("COMM:I2C#:INIT", i2c_init, nullptr);
    visa->addCommand("COMM:I2C#:DATA", i2c_write, i2c_read);
}

} // namespace VISA
} // namespace CTI