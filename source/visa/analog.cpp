#include "visa/core.h"
#include "cti/platform.h"

#include <string.h>

namespace CTI {
namespace Visa {

    //scpi_result_t scpi_LED(scpi_t * context)

    scpi_result_t analog_out_setValue(scpi_t * context) {
        return SCPI_RES_ERR;
    }

    scpi_result_t analog_in_getValue(scpi_t * context) {
        int32_t channel;
        uint16_t val;

        if (!SCPI_ParamInt(context, &channel, true)) {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Analog.GetInput(channel, &val);

        gPlatform.IO.Printf("%d\n", val);

        return SCPI_RES_OK;
    }

    scpi_result_t analog_in_enable(scpi_t * context) {
        int32_t channel;
        
        if (!SCPI_ParamInt(context, &channel, true)) {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Analog.EnableInput(channel);

        return SCPI_RES_OK;
    }

    void initAnalogCommands(Visa* visa) {
        visa->addCommand({"ANAlog:OUTput:VALue", analog_out_setValue, 0});
        visa->addCommand({"ANAlog[:INput]:VALue?", analog_in_getValue, 0});
        visa->addCommand({"ANAlog[:INput]:ENable", analog_in_enable, 0});
    }

} //namespace Visa
} //namespace CTI