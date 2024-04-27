#include "cti/platform.h"
#include <pico/stdlib.h>
#include <hardware/adc.h>

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

void CTI::PlatformIO::_statusLED(bool val) {
    gpio_put(led, val);
}

void CTI::PlatformDigital::SetOutput(uint16_t channel, bool value) {
    gpio_put(channel, value);
}

void CTI::PlatformDigital::SetDirection(int channel, bool output) {
    gpio_init(channel);
    gpio_set_dir(channel, output);
}

void CTI::PlatformDigital::SetPull(int channel, CTI::PlatformDigital::PullDirection dir) {
    bool up = dir & CTI::PlatformDigital::Up;
    bool down = dir & CTI::PlatformDigital::Down;
    gpio_set_pulls(channel, up, down);
}

void CTI::PlatformDigital::GetValue(int channel, bool* value) {
    *value = gpio_get(channel);
}

void CTI::PlatformDigital::GetDirection(int channel, bool* output) {
    *output = gpio_get_dir(channel) == 1;
}

void CTI::PlatformDigital::GetPull(int channel, CTI::PlatformDigital::PullDirection* dir) {
    bool up = gpio_is_pulled_up(channel);
    bool down = gpio_is_pulled_down(channel);

    *dir = (CTI::PlatformDigital::PullDirection)((up ? CTI::PlatformDigital::Up : CTI::PlatformDigital::None) +
        (down ? CTI::PlatformDigital::Down : CTI::PlatformDigital::None));
}

void CTI::PlatformAnalog::EnableInput(int channel) {
    static uint adcToGpio[] = {26, 27, 28, 29};
    adc_gpio_init(adcToGpio[channel]);
}

void CTI::PlatformAnalog::GetInput(int channel, uint16_t* value) {
    adc_select_input(channel);
    *value = adc_read();
}