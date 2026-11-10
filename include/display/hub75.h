#ifndef CTI_HUB75_H
#define CTI_HUB75_H

#include "scpi/scpi_core.h"

namespace CTI::Visa { class Visa; }

namespace CTI::Hub75 {
#ifdef CTI_HUB75
    void registerCommands(Visa::Visa* visa);
    bool stop();
    void service(SCPI::ScpiParser* parser);
    bool allowPin(SCPI::ScpiParser* parser, int pin);
    bool allowAnalog(SCPI::ScpiParser* parser, int channel);
#else
    inline bool stop() { return true; }
    inline void service(SCPI::ScpiParser*) {}
    inline bool allowPin(SCPI::ScpiParser*, int) { return true; }
    inline bool allowAnalog(SCPI::ScpiParser*, int) { return true; }
#endif
}
#endif
