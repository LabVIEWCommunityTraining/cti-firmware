#include "cti/platform.h"

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

void CTI::Platform::BoardInit() {
    cyw43_arch_init();
}

void CTI::PlatformIO::InitStatusLED() {
    //nothing to do with PicoW, is handled by initializing cyw43 in BoardInit
}

void CTI::PlatformIO::_statusLED(bool val) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val);
}