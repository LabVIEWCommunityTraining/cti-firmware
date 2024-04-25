#include "cti/platform.h"
#include "visa/visa_core.h"

#include <hardware/adc.h>

namespace CTI {
namespace Visa {

    scpi_result_t scpi_pico_temp(scpi_t* context) {
        adc_select_input(4);
        uint16_t raw = adc_read();

        float temp = 27 - ((raw / 4096.0) * 3.3 - 0.706) / 0.001721;
        gPlatform.IO.Printf("%f", temp);

        return SCPI_RES_OK;
    }

    void Visa::_init() {
        addCommand({"PICO:TEMP?", scpi_pico_temp, 0});
    };
}
}