#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

    using namespace SCPI;

    QueryResult wifi_status(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    void initWifiCommands(Visa* visa) {
        visa->addCommand("WIFI:STATus",         nullptr,        wifi_status);
    }

}
}