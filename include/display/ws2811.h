#ifndef CTI_WS2811_H
#define CTI_WS2811_H

#include "scpi/scpi_core.h"

namespace CTI::Visa { class Visa; }

namespace CTI::WS2811 {
#ifdef CTI_WS2811
void registerCommands(Visa::Visa* visa);
bool stop();
void service(SCPI::ScpiParser* parser);
bool allowPin(SCPI::ScpiParser* parser, int pin);
#else
inline bool stop() { return true; }
inline void service(SCPI::ScpiParser*) {}
inline bool allowPin(SCPI::ScpiParser*, int) { return true; }
#endif
}
#endif
