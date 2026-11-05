#pragma once
#include "scpi.h"

namespace CTI::Visa {
class Visa {
public:
    SCPI::ScpiParser parser{2048};
    bool registrationFailed = false;
    SCPI::RegistrationResult addCommand(const char* text, SCPI::ScpiCommand command, SCPI::ScpiQuery query) {
        auto result = parser.registerNode(text, command, query);
        registrationFailed |= result != SCPI::RegistrationResult::Success;
        return result;
    }
};
}
