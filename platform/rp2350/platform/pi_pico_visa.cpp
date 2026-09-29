#include "cti/platform.h"
#include "visa/visa_core.h"

#include <hardware/adc.h>

namespace CTI {
namespace Visa {

using namespace SCPI;

QueryResult scpi_pico_temp(ScpiParser* scpi) {
    adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);
    uint16_t raw = adc_read();

    float temp = 27.0f - ((raw / 4096.0f) * 3.3f - 0.706f) / 0.001721f;
    gPlatform.IO.Printf("%f\n", temp);

    return QueryResult::Success;
}

void Visa::_init() {
    addCommand("PICO:TEMP", nullptr, scpi_pico_temp);
}

} // namespace Visa
} // namespace CTI
