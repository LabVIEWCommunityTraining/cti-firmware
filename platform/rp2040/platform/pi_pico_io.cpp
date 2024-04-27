#include "cti/platform.h"
#include <pico/stdlib.h>
#include <hardware/adc.h>

const ChanIndex led = 22;

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

void CTI::PlatformIO::_statusLED(bool val) {
    gpio_put(led, val);
}

void CTI::PlatformDigital::SetOutput(ChanIndex channel, bool value) {
    gpio_put(channel, value);
}

void CTI::PlatformDigital::SetDirection(ChanIndex channel, bool output) {
    gpio_init(channel);
    gpio_set_dir(channel, output);
}

void CTI::PlatformDigital::SetPull(ChanIndex channel, CTI::PlatformDigital::PullDirection dir) {
    bool up = dir & CTI::PlatformDigital::Up;
    bool down = dir & CTI::PlatformDigital::Down;
    gpio_set_pulls(channel, up, down);
}

void CTI::PlatformDigital::GetValue(ChanIndex channel, bool* value) {
    *value = gpio_get(channel);
}

void CTI::PlatformDigital::GetDirection(ChanIndex channel, bool* output) {
    *output = gpio_get_dir(channel) == 1;
}

void CTI::PlatformDigital::GetPull(ChanIndex channel, CTI::PlatformDigital::PullDirection* dir) {
    bool up = gpio_is_pulled_up(channel);
    bool down = gpio_is_pulled_down(channel);

    *dir = (CTI::PlatformDigital::PullDirection)((up ? CTI::PlatformDigital::Up : CTI::PlatformDigital::None) +
        (down ? CTI::PlatformDigital::Down : CTI::PlatformDigital::None));
}

void CTI::PlatformAnalog::EnableInput(ChanIndex channel) {
    static uint adcToGpio[] = {26, 27, 28, 29};
    adc_gpio_init(adcToGpio[channel]);
}

void CTI::PlatformAnalog::GetInput(ChanIndex channel, uint16_t* value) {
    adc_select_input(channel);
    *value = adc_read();
}