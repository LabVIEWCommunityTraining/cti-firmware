#include "visa/visa_core.h"

namespace CTI {
namespace Visa {

    using namespace SCPI;

    CommandResult pwm_init(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);

        if (gpio < 0) {
            return CommandResult::MissingParam;
        }

        bool phaseCorrect = false;
        bool enable = false;

        ParseResult res = scpi->parseBool(phaseCorrect); //optional, don't care if not present

        if (res == ParseResult::Success) {
            scpi->parseBool(enable);
        }

        if (!gPlatform.IO.PWM.InitPWM(gpio, phaseCorrect, enable)) {
            return CommandResult::Error;
        }

        return CommandResult::Success;
    }

    CommandResult pwm_set_duty(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);

        if (gpio < 0) {
            return CommandResult::MissingParam;
        }

        float duty;

        if (scpi->parseReal(duty) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        if (!gPlatform.IO.PWM.SetDuty(gpio, duty)) {
            return CommandResult::Error;
        }

        return CommandResult::Success;
    }

    CommandResult pwm_set_freq(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);
        if (gpio < 0) {
            return CommandResult::MissingParam;
        }

        float freq;

        if (scpi->parseReal(freq) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        if (!gPlatform.IO.PWM.SetFreq(gpio, freq)) {
            return CommandResult::Error;
        }

        return CommandResult::Success;
    }

    CommandResult pwm_set_top(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);
        if (gpio < 0) {
            return CommandResult::MissingParam;
        }

        uint32_t top;

        if (scpi->parseInt(top) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        if (!gPlatform.IO.PWM.SetTop(gpio, top)) {
            return CommandResult::Error;
        }

        return CommandResult::Success;
    }

    QueryResult pwm_get_duty(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);
        if (gpio < 0) {
            return QueryResult::Error;
        }

        float duty;

        duty = gPlatform.IO.PWM.GetDuty(gpio);

        gPlatform.IO.Printf("%f", duty);

        return QueryResult::Success;
    }

    QueryResult pwm_get_freq(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);
        if (gpio < 0) {
            return QueryResult::Error;
        }

        float freq;

        freq = gPlatform.IO.PWM.GetFreq(gpio);

        gPlatform.IO.Printf("%f", freq);

        return QueryResult::Success;
    }

    CommandResult pwm_set_enable(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);
        if (gpio < 0) {
            return CommandResult::MissingParam;
        }

        bool enable;

        if (scpi->parseBool(enable) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.PWM.SetEnable(gpio, enable);

        return CommandResult::Success;
    }

    CommandResult pwm_set_divider(ScpiParser* scpi) {
        ChanIndex gpio = scpi->nodeNum(0);
        if (gpio < 0) {
            return CommandResult::MissingParam;
        }

        float divider;

        if (scpi->parseReal(divider) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        if (!gPlatform.IO.PWM.SetDivider(gpio, divider)) {
            return CommandResult::Error;
        }

        return CommandResult::Success;
    }

    void initPWMCommands(Visa* visa) {
        visa->addCommand("PWM#:INIT", pwm_init, nullptr);
        visa->addCommand("PWM#:DUTY", pwm_set_duty, pwm_get_duty);
        visa->addCommand("PWM#:FREQ", pwm_set_freq, pwm_get_freq);
        visa->addCommand("PWM#:TOP", pwm_set_top, nullptr);
        visa->addCommand("PWM#:ENable", pwm_set_enable, nullptr);
        visa->addCommand("PWM#:DIVider", pwm_set_divider, nullptr);
    }
}
}