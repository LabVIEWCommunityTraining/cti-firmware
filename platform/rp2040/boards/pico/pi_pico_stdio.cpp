#include "cti/platform.h"

#include <pico/stdlib.h>

#include <string.h>

namespace PiPico {
    int32_t stdio_getchar(uint32_t timeout_us) {
        CTI_DEBUG(" stdio");
        int c;
        c = getchar_timeout_us(timeout_us);
        if (c == PICO_ERROR_TIMEOUT) {
            CTI_DEBUG(" t");
            return -1;
        }

        return c;
    }

    int32_t stdio_vprintf(const char* fmt, va_list args) {
        return vprintf(fmt, args);
    }

    void stdio_printchar(char c) {
        fputc(c, stdout);
    }

    void stdio_printstr(const char* str) {
        fwrite(str, 1, strlen(str), stdout);
    }

    void stdio_printnstr(int32_t len, const char* str) {
        fwrite(str, 1, len, stdout);
    }

    void stdio_flush() {
        fflush(stdout);
    }

    CTI::StreamIOImpl stdioStream = {
        stdio_vprintf,
        stdio_printchar,
        stdio_printstr,
        stdio_printnstr,
        stdio_getchar,
        stdio_flush
    };
} // namespace PiPico