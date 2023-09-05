#include "visa/core.h"

namespace CTI {
namespace Visa {

    scpi_result_t pwm_init(scpi_t* context) {
        int32_t gpio;
        bool phaseCorrect = false;
        bool enable = false;

        if (!SCPI_ParamInt32(context, &gpio, true)) {
            return SCPI_RES_ERR;
        }

        if (SCPI_ParamBool(context, &phaseCorrect, false)) {
            SCPI_ParamBool(context, &enable, false);
        }

        if (!gPlatform.IO.PWM.InitPWM(gpio, phaseCorrect, enable)) {
            SCPI_ErrorPushEx(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE, 0, 0);
            return SCPI_RES_ERR;
        }

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_set_duty(scpi_t* context) {
        int32_t gpio;
        float duty;

        if (!SCPI_ParamInt32(context, &gpio, true)) {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamFloat(context, &duty, true)) {
            return SCPI_RES_ERR;
        }

        if (!gPlatform.IO.PWM.SetDuty(gpio, duty)) {
            SCPI_ErrorPushEx(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE, 0, 0);
            return SCPI_RES_ERR;
        }

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_set_freq(scpi_t* context) {
        int32_t gpio;
        float freq;

        if (!SCPI_ParamInt32(context, &gpio, true)) {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamFloat(context, &freq, true)) {
            return SCPI_RES_ERR;
        }

        if (!gPlatform.IO.PWM.SetFreq(gpio, freq)) {
            SCPI_ErrorPushEx(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE, 0, 0);
            return SCPI_RES_ERR;
        }

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_set_top(scpi_t* context) {
        int32_t gpio;
        uint32_t top;

        if (!SCPI_ParamInt32(context, &gpio, true)) {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamUInt32(context, &top, true)) {
            return SCPI_RES_ERR;
        }

        if (!gPlatform.IO.PWM.SetTop(gpio, top)) {
            SCPI_ErrorPushEx(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE, 0, 0);
            return SCPI_RES_ERR;
        }

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_get_duty(scpi_t * context) {
        int32_t gpio;
        float duty;
        
        if (!SCPI_ParamInt(context, &gpio, true))  {
            return SCPI_RES_ERR;
        }

        duty = gPlatform.IO.PWM.GetDuty(gpio);

        gPlatform.IO.Printf("%f", duty);

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_get_freq(scpi_t * context) {
        int32_t gpio;
        float freq;
        
        if (!SCPI_ParamInt(context, &gpio, true))  {
            return SCPI_RES_ERR;
        }

        freq = gPlatform.IO.PWM.GetFreq(gpio);

        gPlatform.IO.Printf("%f", freq);

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_set_enable(scpi_t* context) {
        int32_t gpio;
        bool enable;

        if (!SCPI_ParamInt32(context, &gpio, true)) {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamBool(context, &enable, true)) {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.PWM.SetEnable(gpio, enable);

        return SCPI_RES_OK;
    }

    scpi_result_t pwm_set_divider(scpi_t* context) {
        int32_t gpio;
        float divider;

        if (!SCPI_ParamInt32(context, &gpio, true)) {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamFloat(context, &divider, true)) {
            return SCPI_RES_ERR;
        }

        if (!gPlatform.IO.PWM.SetDivider(gpio, divider)) {
            SCPI_ErrorPushEx(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE, 0, 0);
            return SCPI_RES_ERR;
        }

        return SCPI_RES_OK;
    }

    void initPWMCommands(Visa* visa) {
        visa->addCommand({"PWM:INIT", pwm_init, 0});
        visa->addCommand({"PWM:DUTY", pwm_set_duty, 0});
        visa->addCommand({"PWM:TOP", pwm_set_top, 0});
        visa->addCommand({"PWM:DUTY?", pwm_get_duty, 0});
        visa->addCommand({"PWM:FREQ", pwm_set_freq, 0});
        visa->addCommand({"PWM:FREQ?", pwm_get_freq, 0});
        visa->addCommand({"PWM:ENable", pwm_set_enable, 0});
        visa->addCommand({"PWM:DIVider", pwm_set_divider, 0});
    }
}
}