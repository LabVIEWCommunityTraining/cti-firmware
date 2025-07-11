#ifndef platform_types_h_
#define platform_types_h_

#include <stdint.h>

namespace CTI {

    typedef int8_t GpioNum;
    typedef int8_t AdcNum;
    typedef int8_t UartNum;
    typedef int8_t I2cNum;
    typedef int8_t SpiNum;
    
    typedef int8_t CharSourceID;

    struct LVBlock {
        int32_t len;
        uint8_t buffer[];
    };

    typedef int (*getchar_timeout_us)(uint32_t timeout_us);
    typedef const char* (*getchar_source_id_string)(CharSourceID source_id);

    struct CharSource {
        getchar_timeout_us get_char;
        getchar_source_id_string source_string;
    };

    struct CharSources {
        uint8_t count;
        const CharSource* sources;
    };
}

#endif // platform_types_h_