#include "cti/platform.h"

#include <pico/stdlib.h>

const CTI::ChanIndex led = PICO_DEFAULT_LED_PIN;

void CTI::Platform::BoardInit() {
}

void CTI::PlatformIO::InitStatusLED() {
    gpio_init(led);
    gpio_set_dir(led, true);
}

void CTI::PlatformIO::_statusLED(bool val) {
    gpio_put(led, val);
}
