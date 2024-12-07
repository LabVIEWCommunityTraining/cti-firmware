#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

using namespace SCPI;

CommandResult uart_init(ScpiParser* scpi) {
    uint8_t uart = scpi->nodeNum(1);
    if (uart < 0) {
        return CommandResult::MissingParam;
    }

    uint32_t baud;
    uint8_t txPin;
    uint8_t rxPin;
    uint8_t lineTerm = 0;

    if (scpi->parseInt(baud) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(txPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (scpi->parseInt(rxPin) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    scpi->parseInt(lineTerm); // optional, doesn't matter if not specified

    baud = gPlatform.UART.init(uart, baud, txPin, rxPin, lineTerm);

    return CommandResult::Success;
}

CommandResult uart_write(ScpiParser* scpi) {
    uint8_t uart = scpi->nodeNum(1);
    if (uart < 0) {
        return CommandResult::MissingParam;
    }

    int len = 0;
    char* buf;
    if (scpi->parseBlock(&buf, &len) != ParseResult::Success) {
        return CommandResult::MissingParam;
    }

    if (len > 0) {
        gPlatform.UART.write(uart, len, (uint8_t*) buf);
        gPlatform.IO.Print(len, buf);
        gPlatform.IO.Print('\n');

        return CommandResult::Success;
    }

    return CommandResult::SyntaxError;
}

QueryResult uart_read(ScpiParser* scpi) {
    uint8_t uart = scpi->nodeNum(1);
    if (uart < 0) {
        return QueryResult::Error;
    }

    uint8_t len = 0;
    uint8_t term = gPlatform.UART.termChar(uart);
    uint8_t byte = 0;

    if (term == 0) {
        if (scpi->parseInt(len) != ParseResult::Success) {
            return QueryResult::Error;
        }
        while (len > 0) {
            gPlatform.UART.read(uart, 1, &byte);
            gPlatform.IO.Print((char)byte);
            len--;
        }
    } else {
        while (byte != term) {
            gPlatform.UART.read(uart, 1, &byte);
            gPlatform.IO.Print((char)byte);
        }
    }

    return QueryResult::Success;
}

void initUartCommands(Visa* visa) {
    visa->addCommand("COMM:UART#:INIT", uart_init, nullptr);
    visa->addCommand("COMM:UART#:DATA", uart_write, uart_read);
}

} // namespace VISA
} // namespace CTI