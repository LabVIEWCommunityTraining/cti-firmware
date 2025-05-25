#include "cti/platform.h"

#include <pico/stdlib.h>

//On non-WiFi boards the LED is connected to pin 25
const CTI::ChaneIndex led = 25;

void CTI::PlatformIO::InitStatusLED() {
    gpio_init(led);
    gpio_set_dir(led, true);
}

void CTI::PlatformIO::_statusLED(bool val) {
    gpio_put(led, val);
}