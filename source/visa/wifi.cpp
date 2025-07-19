#include "visa/visa_core.h"
#include "cti/platform.h"

namespace CTI {
namespace Visa {

    using namespace SCPI;

    QueryResult wifi_status(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    CommandResult wifi_connect(ScpiParser* scpi) {

    }

    QueryResult wifi_scan(ScpiParser* scpi) {

    }

    CommandResult wifi_save(ScpiParser* scpi) {
        
    }

    void initWifiCommands(Visa* visa) {
        visa->addCommand("WIFI:STATus",         nullptr,        wifi_status);
        visa->addCommand("WIFI:SCAN",           nullptr,          wifi_scan);
        visa->addCommand("WIFI:CONNect",   wifi_connect,            nullptr);
        visa->addCommand("WIFI:SAVE",         wifi_save,            nullptr);
    }

}
}