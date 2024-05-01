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

};

class PlatformSPI {

};

} // namespace CTI

#endif //platform_comms_h_