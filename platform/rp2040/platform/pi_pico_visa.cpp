#include "cti/platform.h"
#include "visa/visa_core.h"

#include <hardware/adc.h>

namespace CTI {
namespace Visa {

    using namespace SCPI;

    QueryResult scpi_pico_temp(ScpiParser* scpi) {
        adc_select_input(4);
        uint16_t raw = adc_read();

        float temp = 27 - ((raw / 4096.0) * 3.3 - 0.706) / 0.001721;
        gPlatform.IO.Printf("%f\n", temp);

        return QueryResult::Success;
    }

    void Visa::_init() {
        addCommand("PICO:TEMP", nullptr, scpi_pico_temp);
    };
}
}