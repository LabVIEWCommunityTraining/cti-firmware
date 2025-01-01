#include "cti/platform/comms.h"

#include "cti/platform.h"

#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <hardware/spi.h>

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

const uint8_t i2cCount = 2;

i2c_inst_t* i2cs[i2cCount] = {
    i2c0,
    i2c1
};

const uint8_t spiCount = 2;

spi_inst_t* spis[spiCount] = {
    spi0,
    spi1
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

PlatformI2C::PlatformI2C() {
}

uint32_t PlatformI2C::init(uint8_t bus, uint32_t baud, int8_t txPin, int8_t rxPin) {
    i2c_inst_t* i2c = i2cs[bus];

    uint32_t act_baud = i2c_init(i2c, baud);

    if (txPin >= 0) {
        gpio_set_function(txPin, gpio_function::GPIO_FUNC_I2C);
    }

    if (rxPin >= 0) {
        gpio_set_function(rxPin, gpio_function::GPIO_FUNC_I2C);
    }

    return act_baud;
}

size_t PlatformI2C::write(uint8_t bus, uint8_t addr, size_t len, const uint8_t* data, bool nostop) {
    i2c_write_blocking(i2cs[bus], addr, data, len, nostop);

    return len;
}

size_t PlatformI2C::read(uint8_t bus, uint8_t addr, size_t len, uint8_t* buf, bool nostop) {
    i2c_read_blocking(i2cs[bus], addr, buf, len, nostop);

    return len;
}

PlatformSPI::PlatformSPI() {

}

uint32_t PlatformSPI::init(uint8_t bus, uint32_t baud, uint8_t spiMode, uint8_t bits, int8_t mosiPin, int8_t misoPin, int8_t sckPin) {
    spi_inst_t* spi = spis[bus];
    uint32_t act_baud = spi_init(spi, baud);
    //gPlatform.IO.Printf("B: %d (%d) ", baud, act_baud);

    spi_cpol_t cpol = (spi_cpol_t)((spiMode & 0x2) >> 1);
    spi_cpha_t cpha = (spi_cpha_t)(spiMode & 0x1);

    spi_set_format(spi, bits, cpol, cpha, SPI_MSB_FIRST);

    if (mosiPin >= 0) {
        gpio_set_function(mosiPin, gpio_function::GPIO_FUNC_SPI);
    }

    if (misoPin >= 0) {
        gpio_set_function(misoPin, gpio_function::GPIO_FUNC_SPI);
    }

    if (sckPin >= 0) {
        gpio_set_function(sckPin, gpio_function::GPIO_FUNC_SPI);
    }

    return act_baud;
}

size_t PlatformSPI::write(uint8_t bus, size_t len, const uint8_t* data) {
    spi_write_blocking(spis[bus], data, len);

    return len;
}

size_t PlatformSPI::read(uint8_t bus, size_t len, uint8_t* buf, const uint8_t* data) {
    if (data != 0) {
        spi_write_read_blocking(spis[bus], data, buf, len);
    } else {
        spi_read_blocking(spis[bus], 0, buf, len);
    }

    return len;
}

} // namespace CTI