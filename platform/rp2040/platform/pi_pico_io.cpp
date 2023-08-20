#include "cti/platform.h"
#include <pico/stdlib.h>

const uint8_t led = 25;

int CTI::PlatformIO::_getchar_timeout_us(uint32_t timeout_us) {
    int c;
    c = getchar_timeout_us(timeout_us);
    if (c == PICO_ERROR_TIMEOUT) {
        return -1;
    }

    return c;
}

void CTI::PlatformIO::InitStatusLED() {
    gpio_init(led);
    gpio_set_dir(led, true);
}

void CTI::PlatformIO::StatusLED(bool val) {
    gpio_put(led, val);
}