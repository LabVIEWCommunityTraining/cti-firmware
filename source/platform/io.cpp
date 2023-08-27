#include "cti/platform.h"

#include <string.h>
#include <stdarg.h>

namespace CTI {

    PlatformIO::PlatformIO() {

    }

    void PlatformIO::Print(int32_t len, const char* str) {
        fwrite(str, 1, len, stdout);
    }

    void PlatformIO::Print(const char* str) {
        fwrite(str, 1, strlen(str), stdout);
    }

    void PlatformIO::Print(char c) {
        fwrite(&c, 1, 1, stdout);
    }

    void PlatformIO::Printf(const char* format, ...) const {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }

    void PlatformIO::Flush() {
        fflush(stdout);
    }

    void PlatformIO::ReadStdin(char** buffer, size_t maxLen) {

    }

} //namespace CTI