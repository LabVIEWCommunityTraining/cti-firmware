#include "visa/core.h"
#include "cti/platform.h"

#include <string.h>

namespace CTI {
namespace Visa {

    //scpi_result_t scpi_LED(scpi_t * context)

    scpi_result_t analog_out_setValue(scpi_t * context) {

    }

    scpi_result_t analog_out_getValue(scpi_t * context) {
        
    }

    scpi_result_t analog_in_getValue(scpi_t * context) {
        
    }

    void initAnalogCommands(Visa* visa) {
        visa->addCommand({"ANAlog:OUTput:VALue", analog_out_setValue, 0});
        visa->addCommand({"ANAlog:OUTput:VALue?", analog_out_getValue, 0});
        visa->addCommand({"ANAlog:INput:VALue?", analog_in_getValue, 0});
    }

} //namespace Visa
} //namespace CTI