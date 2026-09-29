#include "cti/platform.h"

#include <driver/ledc.h>

namespace CTI {

static float duty[16] = {};
static float frequency[16] = {};
static LVBlock availablePWMs = { 12, { 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x04, 0x0C, 0x0D, 0x10, 0x11, 0x17 } };

LVBlock* PlatformPWM::Available() { return &availablePWMs; }

bool PlatformPWM::InitPWM(ChanIndex gpio, bool, bool enable) {
    ledc_channel_config_t channel = {};
    channel.gpio_num = gpio;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = static_cast<ledc_channel_t>(gpio % 8);
    channel.timer_sel = LEDC_TIMER_0;
    channel.duty = 0;
    channel.hpoint = 0;
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.timer_num = LEDC_TIMER_0;
    timer.duty_resolution = LEDC_TIMER_13_BIT;
    timer.freq_hz = 10000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK || ledc_channel_config(&channel) != ESP_OK) return false;
    SetEnable(gpio, enable);
    frequency[gpio] = 10000;
    return true;
}

bool PlatformPWM::SetDuty(ChanIndex gpio, float value) {
    if (value < 0 || value > 1) return false;
    duty[gpio] = value;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(gpio % 8), value * 8191);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(gpio % 8));
    return true;
}

bool PlatformPWM::SetFreq(ChanIndex gpio, float freq) {
    if (freq <= 0) return false;
    if (ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, static_cast<uint32_t>(freq)) == 0) {
        frequency[gpio] = freq;
        return true;
    }
    return false;
}

void PlatformPWM::SetEnable(ChanIndex gpio, bool enable) {
    ledc_stop(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(gpio % 8), enable ? 0 : 1);
}

bool PlatformPWM::SetTop(ChanIndex, uint16_t) { return false; }
bool PlatformPWM::SetDivider(ChanIndex, float) { return false; }
float PlatformPWM::GetDuty(ChanIndex gpio) { return duty[gpio]; }
float PlatformPWM::GetFreq(ChanIndex gpio) { return frequency[gpio]; }

} // namespace CTI
