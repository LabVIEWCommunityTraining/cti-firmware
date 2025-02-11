#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

using namespace SCPI;

const uint8_t uart_buf_len = 255;
uint8_t uart_buf[uart_buf_len]; // extra byte to accomodate null termination

QueryResult uart_avail(ScpiParser* scpi) {
    LVBlock* uarts = gPlatform.UART.Available();

    PrintBlock(uarts->len, uarts->buffer);

    return QueryResult::Success;
}

CommandResult uart_init(ScpiParser* scpi) {
    uint8_t uart = scpi->nodeNum(1);
    if (uart < 0) {
        errSuffixOutOfRange(scpi);
        return CommandResult::Error;
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
        errSuffixOutOfRange(scpi);
        return CommandResult::Error;
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
        errSuffixOutOfRange(scpi);
        return QueryResult::Error;
    }

    uint8_t len = 0;
    uint8_t term = gPlatform.UART.termChar(uart);
    uint8_t byte = 0;

    if (term == 0) {
        //send back false for line mode response
        gPlatform.IO.Print('0');
        gPlatform.IO.Print(',');

        //binary mode, read into buffer and print binary block
        if (scpi->parseInt(len) != ParseResult::Success) {
            return QueryResult::MissingParam;
        }
        
        if (len > uart_buf_len) {
            errParamOutOfRange(scpi);
            return QueryResult::Error;
        }

        len = gPlatform.UART.read(uart, len, uart_buf);
        PrintBlock(len, uart_buf);
        gPlatform.IO.Print('\n');
    } else {
        //ascii line mode, echo bytes until term char is seen
        //send back true for line mode response
        gPlatform.IO.Print('1');
        gPlatform.IO.Print(',');

        while (byte != term) {
            gPlatform.UART.read(uart, 1, &byte);
            gPlatform.IO.Print((char)byte);
        }
    }

    return QueryResult::Success;
}

void initUartCommands(Visa* visa) {
    visa->addCommand("COMM:UART:AVAILable",  nullptr, uart_avail);
    visa->addCommand("COMM:UART#:INIT", uart_init, nullptr);
    visa->addCommand("COMM:UART#:DATA", uart_write, uart_read);
}

} // namespace VISA
} // namespace CTI