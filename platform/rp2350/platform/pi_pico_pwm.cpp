#include "cti/platform.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CTI {

namespace {

constexpr uint16_t defaultTop = 9999;
constexpr float defaultFrequency = 10000.0f;
constexpr uint32_t dividerFixedPointMin = 16;
constexpr uint32_t dividerFixedPointMax = 4095;

float duty[NUM_PWM_SLICES] = {};

float systemClockHz() {
    return static_cast<float>(clock_get_hz(clk_sys));
}

uint32_t pwmPeriod(uint16_t top, bool phaseCorrect) {
    return phaseCorrect ? static_cast<uint32_t>(top) * 2u
                        : static_cast<uint32_t>(top) + 1u;
}

uint16_t levelForDuty(uint16_t top, float dutyPercent) {
    if (dutyPercent <= 0.0f) {
        return 0;
    }

    if (dutyPercent >= 1.0f) {
        return top == UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(top + 1u);
    }

    return static_cast<uint16_t>((static_cast<uint32_t>(top) + 1u) * dutyPercent);
}

} // namespace

LVBlock availablePWMs = { 18,
    { 0x00, 0x00, 0x00, 0x0E, 0x00, 0x02, 0x04, 0x06, 0x08,
      0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x1A, 0x1C }
};

LVBlock* PlatformPWM::Available() {
    return &availablePWMs;
}

bool PlatformPWM::InitPWM(ChanIndex gpio, bool phaseCorrect, bool enable) {
    if ((gpio & 1u) != 0u) {
        return false;
    }

    uint slice = pwm_gpio_to_slice_num(gpio);
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    pwm_config cfg = pwm_get_default_config();
    uint16_t top = defaultTop;
    float divider = systemClockHz() /
        (defaultFrequency * pwmPeriod(top, phaseCorrect));
    if (divider < 1.0f) {
        divider = 1.0f;
        top = static_cast<uint16_t>(std::round(
            systemClockHz() / (defaultFrequency * 2.0f)));
    }

    pwm_config_set_phase_correct(&cfg, phaseCorrect);
    pwm_config_set_wrap(&cfg, top);
    pwm_config_set_clkdiv(&cfg, divider);
    pwm_init(slice, &cfg, enable);
    duty[slice] = 0.0f;

    return true;
}

bool PlatformPWM::SetDuty(ChanIndex gpio, float dutyPercent) {
    if (!std::isfinite(dutyPercent) || dutyPercent < 0.0f || dutyPercent > 1.0f) {
        return false;
    }

    uint slice = pwm_gpio_to_slice_num(gpio);
    uint chan = pwm_gpio_to_channel(gpio);
    uint16_t top = static_cast<uint16_t>(pwm_hw->slice[slice].top);

    duty[slice] = dutyPercent;
    pwm_set_chan_level(slice, chan, levelForDuty(top, dutyPercent));

    return true;
}

bool PlatformPWM::SetTop(ChanIndex gpio, uint16_t top) {
    if (top == 0) {
        return false;
    }

    uint slice = pwm_gpio_to_slice_num(gpio);
    uint chan = pwm_gpio_to_channel(gpio);

    pwm_set_wrap(slice, top);
    pwm_set_chan_level(slice, chan, levelForDuty(top, duty[slice]));

    return true;
}

bool PlatformPWM::SetDivider(ChanIndex gpio, float divider) {
    if (!std::isfinite(divider) || divider < 1.0f || divider >= 256.0f) {
        return false;
    }

    pwm_set_clkdiv(pwm_gpio_to_slice_num(gpio), divider);
    return true;
}

void PlatformPWM::SetEnable(ChanIndex gpio, bool enable) {
    pwm_set_enabled(pwm_gpio_to_slice_num(gpio), enable);
}

float PlatformPWM::GetDuty(ChanIndex gpio) {
    uint slice = pwm_gpio_to_slice_num(gpio);
    uint chan = pwm_gpio_to_channel(gpio);
    uint32_t cc = pwm_hw->slice[slice].cc;
    uint16_t level = chan == PWM_CHAN_A ? static_cast<uint16_t>(cc)
                                        : static_cast<uint16_t>(cc >> 16);
    uint16_t top = static_cast<uint16_t>(pwm_hw->slice[slice].top);

    return static_cast<float>(level) / (static_cast<float>(top) + 1.0f);
}

float PlatformPWM::GetFreq(ChanIndex gpio) {
    uint slice = pwm_gpio_to_slice_num(gpio);
    bool phaseCorrect = (pwm_hw->slice[slice].csr & PWM_CH0_CSR_PH_CORRECT_BITS) != 0;
    float divider = static_cast<float>(pwm_hw->slice[slice].div) / 16.0f;
    uint16_t top = static_cast<uint16_t>(pwm_hw->slice[slice].top);

    return systemClockHz() / (divider * pwmPeriod(top, phaseCorrect));
}

bool PlatformPWM::SetFreq(ChanIndex gpio, float freq) {
    if (!std::isfinite(freq) || freq <= 0.0f) {
        return false;
    }

    uint slice = pwm_gpio_to_slice_num(gpio);
    bool phaseCorrect = (pwm_hw->slice[slice].csr & PWM_CH0_CSR_PH_CORRECT_BITS) != 0;
    float maximumPeriod = static_cast<float>(pwmPeriod(UINT16_MAX, phaseCorrect));
    float requiredDivider = systemClockHz() / (freq * maximumPeriod);
    float requiredDividerFixedPoint = std::ceil(requiredDivider * 16.0f);
    uint32_t dividerFixedPoint;
    if (requiredDividerFixedPoint <= dividerFixedPointMin) {
        dividerFixedPoint = dividerFixedPointMin;
    } else if (requiredDividerFixedPoint >= dividerFixedPointMax) {
        dividerFixedPoint = dividerFixedPointMax;
    } else {
        dividerFixedPoint = static_cast<uint32_t>(requiredDividerFixedPoint);
    }

    float divider = static_cast<float>(dividerFixedPoint) / 16.0f;
    float idealTop = phaseCorrect
        ? systemClockHz() / (freq * divider * 2.0f)
        : systemClockHz() / (freq * divider) - 1.0f;
    uint16_t top = static_cast<uint16_t>(
        std::clamp(std::round(idealTop), 10.0f, static_cast<float>(UINT16_MAX)));

    pwm_set_clkdiv_int_frac4(
        slice,
        static_cast<uint8_t>(dividerFixedPoint >> 4),
        static_cast<uint8_t>(dividerFixedPoint & 0x0f));
    pwm_set_wrap(slice, top);
    SetDuty(gpio, duty[slice]);

    return true;
}

} // namespace CTI
