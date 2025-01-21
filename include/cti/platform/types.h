#ifndef platform_types_h_
#define platform_types_h_

#include <stdint.h>

namespace CTI {

    typedef int8_t GpioNum;
    typedef int8_t AdcNum;
    typedef int8_t UartNum;
    typedef int8_t I2cNum;
    typedef int8_t SpiNum;

    struct LVBlock {
        int32_t len;
        uint8_t buffer[];
    };

}

#endif // platform_types_h_