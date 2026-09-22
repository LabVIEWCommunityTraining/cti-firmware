#include "cti/platform.h"

#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

void CTI::Platform::BoardInit() {
    cyw43_arch_init();
}

void CTI::PlatformIO::InitStatusLED() {
}

void CTI::PlatformIO::_statusLED(bool val) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val);
}
