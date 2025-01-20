#ifndef platform_comms_h_
#define platform_comms_h_

#include <stdint.h>
#include <stdio.h>

namespace CTI {

class PlatformUART {
public:
    PlatformUART();

    uint32_t init(uint8_t uart, uint32_t baud, int8_t txPin, int8_t rxPin, uint8_t lineTerm = 0);
    size_t write(uint8_t uart, size_t len, const uint8_t* data);
    size_t read(uint8_t uart, size_t len, uint8_t* buf);

    uint8_t termChar(uint8_t uart);
};

class PlatformI2C {
public:
    PlatformI2C();

    uint32_t init(uint8_t bus, uint32_t baud, int8_t txPin, int8_t rxPin);
    size_t write(uint8_t bus, uint8_t addr, size_t len, const uint8_t* data, bool nostop = false);
    size_t read(uint8_t bus, uint8_t addr, size_t len, uint8_t* buf, bool nostop = false);
};

class PlatformSPI {
public:
    PlatformSPI();

    uint32_t init(uint8_t bus, uint32_t baud, uint8_t spiMode, uint8_t bits, int8_t mosiPin, int8_t misoPin, int8_t sckPin);
    size_t write(uint8_t bus, size_t len, const uint8_t* data);
    size_t read(uint8_t bus, size_t len, uint8_t* buf, const uint8_t* data = 0);
};

} // namespace CTI

#endif //platform_comms_h_