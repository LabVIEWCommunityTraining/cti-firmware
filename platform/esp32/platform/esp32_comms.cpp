#include "cti/platform/comms.h"

#include <driver/i2c.h>
#include <driver/spi_master.h>
#include <driver/uart.h>
#include <esp_err.h>

namespace CTI {

static uart_port_t uarts[] = { UART_NUM_0, UART_NUM_1, UART_NUM_2 };
static uint8_t uart_terms[] = { 0, 0, 0 };
static i2c_port_t i2cs[] = { I2C_NUM_0, I2C_NUM_1 };
static spi_device_handle_t spi_devices[2] = {};
static constexpr spi_host_device_t spi_hosts[] = { SPI2_HOST, SPI3_HOST };

static LVBlock availableUARTs = { 15, {
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x01
}};

PlatformUART::PlatformUART() {}
LVBlock* PlatformUART::Available() { return &availableUARTs; }
uint8_t PlatformUART::termChar(uint8_t uart) { return uart_terms[uart]; }

uint32_t PlatformUART::init(uint8_t uart, uint32_t baud, int8_t txPin, int8_t rxPin, uint8_t lineTerm) {
    uart_config_t config = {};
    config.baud_rate = static_cast<int>(baud);
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(uarts[uart], &config);
    uart_set_pin(uarts[uart], txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uarts[uart], 2048, 0, 0, nullptr, 0);
    uart_terms[uart] = lineTerm;
    return baud;
}

size_t PlatformUART::write(uint8_t uart, size_t len, const uint8_t* data) {
    return uart_write_bytes(uarts[uart], reinterpret_cast<const char*>(data), len);
}

size_t PlatformUART::read(uint8_t uart, size_t len, uint8_t* buf) {
    size_t count = 0;
    while (count < len) {
        int got = uart_read_bytes(uarts[uart], &buf[count], 1, portMAX_DELAY);
        if (got <= 0 || (uart_terms[uart] && buf[count] == uart_terms[uart])) break;
        count += static_cast<size_t>(got);
    }
    return count;
}

static LVBlock availableI2Cs = { 24, {
    0x00, 0x00, 0x00, 0x02,
    0x00,
    0x00, 0x00, 0x00, 0x01, 0x0F,
    0x00, 0x00, 0x00, 0x01, 0x04,
    0x01,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
}};
PlatformI2C::PlatformI2C() {}
LVBlock* PlatformI2C::Available() { return &availableI2Cs; }

uint32_t PlatformI2C::init(uint8_t bus, uint32_t baud, int8_t sclPin, int8_t sdaPin) {
    i2c_config_t config = {};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = static_cast<gpio_num_t>(sdaPin);
    config.scl_io_num = static_cast<gpio_num_t>(sclPin);
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = baud;
    i2c_param_config(i2cs[bus], &config);
    i2c_driver_install(i2cs[bus], config.mode, 0, 0, 0);
    return baud;
}

size_t PlatformI2C::write(uint8_t bus, uint8_t addr, size_t len, const uint8_t* data, bool) {
    return i2c_master_write_to_device(i2cs[bus], addr, data, len, pdMS_TO_TICKS(1000)) == ESP_OK ? len : 0;
}

size_t PlatformI2C::read(uint8_t bus, uint8_t addr, size_t len, uint8_t* buf, bool) {
    return i2c_master_read_from_device(i2cs[bus], addr, buf, len, pdMS_TO_TICKS(1000)) == ESP_OK ? len : 0;
}

static LVBlock availableSPIs = { 20, {
    0x00, 0x00, 0x00, 0x01,
    0x00,
    0x00, 0x00, 0x00, 0x01, 0x1B,
    0x00, 0x00, 0x00, 0x01, 0x13,
    0x00, 0x00, 0x00, 0x01, 0x05
}};
PlatformSPI::PlatformSPI() {}
LVBlock* PlatformSPI::Available() { return &availableSPIs; }

uint32_t PlatformSPI::init(uint8_t bus, uint32_t baud, uint8_t mode, uint8_t bits, int8_t mosiPin, int8_t misoPin, int8_t sckPin) {
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = mosiPin;
    bus_config.miso_io_num = misoPin;
    bus_config.sclk_io_num = sckPin;
    if (bus >= sizeof(spi_hosts) / sizeof(spi_hosts[0])) return 0;
    const spi_host_device_t host = spi_hosts[bus];
    spi_bus_initialize(host, &bus_config, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t device_config = {};
    device_config.clock_speed_hz = baud;
    device_config.mode = mode;
    device_config.spics_io_num = -1;
    device_config.queue_size = 1;
    spi_bus_add_device(host, &device_config, &spi_devices[bus]);
    return baud;
}

size_t PlatformSPI::write(uint8_t bus, size_t len, const uint8_t* data) {
    spi_transaction_t transaction = {};
    transaction.length = len * 8;
    transaction.tx_buffer = data;
    return spi_device_transmit(spi_devices[bus], &transaction) == ESP_OK ? len : 0;
}

size_t PlatformSPI::read(uint8_t bus, size_t len, uint8_t* buf, const uint8_t* data) {
    spi_transaction_t transaction = {};
    transaction.length = len * 8;
    transaction.tx_buffer = data;
    transaction.rx_buffer = buf;
    return spi_device_transmit(spi_devices[bus], &transaction) == ESP_OK ? len : 0;
}

} // namespace CTI
