#include "cti/platform.h"
#include "cti_board.h"

#include <esp_adc/adc_oneshot.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_err.h>
#include <esp_rom_sys.h>

#include <cstdio>
#include <cstring>
#include <stdarg.h>

namespace CTI {

static adc_oneshot_unit_handle_t adc_unit = nullptr;
static bool gpio_output[GPIO_NUM_MAX] = {};
static bool console_uart_ready = false;

static LVBlock availableGPIOs = { 32, {
    0x00, 0x00, 0x00, 0x1C,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0x15, 0x16, 0x17, 0x19, 0x1A, 0x1B,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
}};

LVBlock* PlatformDigital::Available() { return &availableGPIOs; }

void PlatformDigital::SetOutput(ChanIndex channel, bool value) {
    gpio_set_level(static_cast<gpio_num_t>(channel), value);
}

void PlatformDigital::SetDirection(ChanIndex channel, bool output) {
    gpio_set_direction(static_cast<gpio_num_t>(channel),
        output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
    if (channel >= 0 && channel < GPIO_NUM_MAX) gpio_output[channel] = output;
}

void PlatformDigital::SetPull(ChanIndex channel, PullDirection dir) {
    gpio_set_pull_mode(static_cast<gpio_num_t>(channel),
        (dir == Up) ? GPIO_PULLUP_ONLY :
        (dir == Down) ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING);
}

void PlatformDigital::GetValue(ChanIndex channel, bool* value) {
    *value = gpio_get_level(static_cast<gpio_num_t>(channel)) != 0;
}

void PlatformDigital::GetDirection(ChanIndex channel, bool* output) {
    *output = channel >= 0 && channel < GPIO_NUM_MAX && gpio_output[channel];
}

void PlatformDigital::GetPull(ChanIndex, PullDirection* dir) {
    *dir = None;
}

static LVBlock availableAIs = { 12, {
    0x00, 0x00, 0x00, 0x04,
    0x00, CTI_BOARD_ADC0_GPIO,
    0x01, CTI_BOARD_ADC1_GPIO,
    0x02, CTI_BOARD_ADC2_GPIO,
    0x03, CTI_BOARD_ADC3_GPIO
} };

static constexpr adc_channel_t adc_channels[] = {
    ADC_CHANNEL_0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3
};

LVBlock* PlatformAnalog::Available() { return &availableAIs; }

void PlatformAnalog::EnableInput(ChanIndex channel) {
    if (channel < 0 || channel >= static_cast<ChanIndex>(sizeof(adc_channels) / sizeof(adc_channels[0]))) {
        return;
    }
    if (!adc_unit) {
        adc_oneshot_unit_init_cfg_t unit_config = {};
        unit_config.unit_id = ADC_UNIT_1;
        adc_oneshot_new_unit(&unit_config, &adc_unit);
    }
    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.atten = ADC_ATTEN_DB_12;
    channel_config.bitwidth = ADC_BITWIDTH_12;
    adc_oneshot_config_channel(adc_unit, adc_channels[channel], &channel_config);
}

void PlatformAnalog::GetInput(ChanIndex channel, uint16_t* value) {
    if (channel < 0 || channel >= static_cast<ChanIndex>(sizeof(adc_channels) / sizeof(adc_channels[0]))) {
        *value = 0;
        return;
    }
    if (!adc_unit) EnableInput(channel);
    int raw = 0;
    adc_oneshot_read(adc_unit, adc_channels[channel], &raw);
    *value = static_cast<uint16_t>(raw);
}

int PlatformIO::_getchar_timeout_us(uint32_t timeout_us) {
    static constexpr uint32_t kMaxWaitMs = 1000;

    if (!console_uart_ready) {
        const esp_err_t err = uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return -1;
        }
        console_uart_ready = true;
    }

    uint8_t c = 0;
    const uint32_t timeout_ms = (timeout_us > 1000U * 1000U) ? kMaxWaitMs : (timeout_us + 999U) / 1000U;

    const int count = uart_read_bytes(UART_NUM_0, &c, 1, timeout_ms);
    return (count == 1) ? c : -1;
}

void PlatformIO::InitStatusLED() {}

void PlatformIO::_statusLED(bool val) {
    gpio_set_level(GPIO_NUM_25, val ? 1 : 0);
}

} // namespace CTI
