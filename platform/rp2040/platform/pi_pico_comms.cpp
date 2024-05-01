#include "cti/platform/comms.h"

#include <hardware/uart.h>
#include <hardware/gpio.h>

namespace CTI {

const uint8_t uartCount = 2;

uart_inst_t* uarts[uartCount] = {
    uart0,
    uart1
};

uint8_t uart_term[uartCount] = {
    0,
    0
};

PlatformUART::PlatformUART() {
}

uint8_t PlatformUART::termChar(uint8_t uart) {
    return uart_term[uart];
}

uint32_t PlatformUART::init(uint8_t uart, uint32_t baud, int8_t txPin, int8_t rxPin, uint8_t lineTerm) {
    baud = uart_init(uarts[uart], baud);

    uart_term[uart] = lineTerm;

    if (txPin >= 0) {
        gpio_set_function(txPin, gpio_function::GPIO_FUNC_UART);
    }

    if (rxPin >= 0) {
        gpio_set_function(rxPin, gpio_function::GPIO_FUNC_UART);
    }

    return baud;
}

size_t PlatformUART::write(uint8_t uart, size_t len, const uint8_t* data) {
    uart_write_blocking(uarts[uart], data, len);

    return len;
}

size_t PlatformUART::read(uint8_t uart, size_t len, uint8_t* buf) {
    if (uart_term[uart] != 0) {
        uint8_t byte;
        for (size_t i = 0; i < len; ++i) {
            uart_read_blocking(uarts[uart], &byte, 1);
            buf[i] = byte;
            if (byte == uart_term[uart]) {
                return i + 1;
            }
        }

        return len;
    } else {
        uart_read_blocking(uarts[uart], buf, len);

        return len;
    }
}

} // namespace CTI