#include "cti/platform.h"

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

namespace CTI {

    float minDivMaxFreq = 125000000.0f / 65536.0f;               // 1907.3486 - higher freq = reduced TOP
    float maxDivMinFreq = 125000000.0f / (255.9375f * 65536.0f); // 7.4524  - min possible frequency

    //store requested duty % to prevent value walks due to numeric precision, per slice
    float _duty[NUM_PWM_SLICES];

    // Generated with CTI-Tools.lvlib
    LVBlock availablePWMs = { 18,
        { 0x00, 0x00, 0x00, 0x0E, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x1A, 0x1C }
    };

    LVBlock* PlatformPWM::Available() {
        return &availablePWMs;
    }

    bool PlatformPWM::InitPWM(ChanIndex gpio, bool phaseCorrect, bool enable) {
        //For simplicity, only support even numbered GPIO to only have 1 gpio per PWM config
        if (gpio & 1u == 1) {
            return false;
        }

        uint slice = pwm_gpio_to_slice_num(gpio);
        uint chan = 0; // locked to even GPIO which is first channel per PWM

        gpio_set_function(gpio, GPIO_FUNC_PWM);

        pwm_config cfg = pwm_get_default_config();
        pwm_config_set_phase_correct(&cfg, phaseCorrect);

        //Defaults PWM to an effective rate of 10kHz
        //and a level (duty) range from 0-10000 (~13.3 bits)
        pwm_config_set_clkdiv(&cfg, 1.25f);
        pwm_config_set_wrap(&cfg, 10000u);
        pwm_init(slice, &cfg, enable);
        _duty[slice] = 0.0f;

        return true;
    }

    bool PlatformPWM::SetDuty(ChanIndex gpio, float dutyPercent) {
        if (dutyPercent < 0.0f || dutyPercent > 1.0f) return false;

        uint slice = pwm_gpio_to_slice_num(gpio);
        uint chan = pwm_gpio_to_channel(gpio);

        uint16_t level = 0;

        if (dutyPercent == 1.0f) level = pwm_hw->slice[slice].top + 1;
        else if (dutyPercent > 0.0f) level = pwm_hw->slice[slice].top * dutyPercent;

        _duty[slice] = dutyPercent;

        pwm_set_chan_level(slice, chan, level);

        return true;
    }

    bool PlatformPWM::SetTop(ChanIndex gpio, uint16_t top) {
        uint slice = pwm_gpio_to_slice_num(gpio);
        uint chan = pwm_gpio_to_channel(gpio);
        uint16_t level = 0;

        if (_duty[slice] >= 1.0f) {
            level = top + 1;
        } else if (_duty[slice] > 0.0f) {
            level = top * _duty[slice];
        }

        pwm_set_wrap(slice, top);
        pwm_set_chan_level(slice, chan, level);

        return true;
    }

    bool PlatformPWM::SetDivider(ChanIndex gpio, float divider) {
        uint slice = pwm_gpio_to_slice_num(gpio);

        if (divider < 1.0f || 256.0f <= divider) {
            return false;
        }

        pwm_set_clkdiv(slice, divider);

        return true;
    }

    void PlatformPWM::SetEnable(ChanIndex gpio, bool enable) {
        uint slice = pwm_gpio_to_slice_num(gpio);
        pwm_set_enabled(slice, enable);
    }

    float PlatformPWM::GetDuty(ChanIndex gpio) {
        uint slice = pwm_gpio_to_slice_num(gpio);
        float duty = (float)pwm_hw->slice[slice].cc / (float)pwm_hw->slice[slice].top;
        return duty;
    }

    float PlatformPWM::GetFreq(ChanIndex gpio) {
        uint slice = pwm_gpio_to_slice_num(gpio);
        uint8_t phaseCorrect = !!(pwm_hw->slice[slice].csr & PWM_CH0_CSR_PH_CORRECT_BITS);
        float div = pwm_hw->slice[slice].div * 0.0625f;
        float f = (125000000.0) / (div * pwm_hw->slice[slice].top * (1 + phaseCorrect));

        return f;
    }

    bool PlatformPWM::SetFreq(ChanIndex gpio, float freq) {
        //Want to determine clock divider to maximize TOP
        uint32_t fxp = 16; // default to 1.0f value, 4 bits of decimal LSB = 1/16
        uint16_t top = 65535;

        //TODO: determine sys_clk programmatically, currently assuming pico default
        if (freq >= minDivMaxFreq) {
            // past full resolution frequency, need to reduce resolution to achieve frequency
            top = 125000000.0f / freq;
            if (top < 10) top = 10; //guarantee at least 10 steps of values ~3.322bits
        } else if (freq <= maxDivMinFreq) {
            fxp = 4095; // set divider to max and leave top max to give lowest possible freq
        } else {
            //used for calculations of fixedpoint representation, each fxp LSB is 1/16
            const float clkX16 = 125000000.0f * 16.0f;
            
            //best first guess at fixed-point value, rounded up
            fxp = (clkX16 / (freq * 65536.0f)) + 0.999f;

            //at this point we know fxp is not more than max of 4095 due to previous else-if

            //now determine closest top value based on fxp inaccuracy
            top = clkX16 / (freq * fxp);
        }

        uint slice = pwm_gpio_to_slice_num(gpio);
        pwm_hw->slice[slice].div = fxp;
        pwm_hw->slice[slice].top = top;

        SetDuty(gpio, _duty[slice]); // reapply duty%

        return true; // TODO: fail for enough error %?
    }

} // namsepace CTI