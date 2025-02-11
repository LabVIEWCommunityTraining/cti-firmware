#include "visa/visa_core.h"
#include "cti/platform.h"

#include <string.h>

namespace CTI {
namespace Visa {

    using namespace SCPI;

    //scpi_result_t scpi_LED(scpi_t * context)

    CommandResult analog_out_setValue(ScpiParser* scpi) {
        errNoCommand(scpi);
        return CommandResult::Error;
    }

    QueryResult analog_available(ScpiParser* scpi) {
        LVBlock* adcs = gPlatform.IO.Analog.Available();

        PrintBlock(adcs->len, adcs->buffer);

        return QueryResult::Success;
    }

    QueryResult analog_in_getValue(ScpiParser* scpi) {
        ChanIndex channel = scpi->nodeNum(1);
        if (channel < 0) {
            errSuffixOutOfRange(scpi);
            return QueryResult::Error;
        }

        uint16_t val;
        gPlatform.IO.Analog.GetInput(channel, &val);

        gPlatform.IO.Printf("%d\n", val);

        return QueryResult::Success;
    }

    CommandResult analog_in_enable(ScpiParser* scpi) {
        ChanIndex channel = scpi->nodeNum(1);
        if (channel < 0) {
            errSuffixOutOfRange(scpi);
            return CommandResult::Error;
        }

        gPlatform.IO.Analog.EnableInput(channel);

        return CommandResult::Success;
    }

    void initAnalogCommands(Visa* visa) {
        visa->addCommand("ANAlog:AVAILable",     nullptr,             analog_available);
        visa->addCommand("ANAlog:OUTput#:VALue", analog_out_setValue, nullptr);
        visa->addCommand("ANAlog:INput#:VALue",  nullptr,             analog_in_getValue);
        visa->addCommand("ANAlog:INput#:ENable", analog_in_enable,    nullptr);
    }

} //namespace Visa
} //namespace CTI