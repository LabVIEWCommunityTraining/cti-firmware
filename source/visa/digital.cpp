#include "visa/visa_core.h"

#include <string.h>

namespace CTI {
namespace Visa {

    using namespace SCPI;

    ScpiChoice digitalDirectionOptions[] = {
        {"IN", 0},
        {"OUT", 1},
        NullScpiChoice
    };

    ScpiChoice digitalPullOptions[] = {
        {"NONE", (uint8_t)PlatformDigital::None},
        {"UP", (uint8_t)PlatformDigital::Up},
        {"DOWN", (uint8_t)PlatformDigital::Down},
        {"BOTH", (uint8_t)PlatformDigital::Both},
        NullScpiChoice
    };

    CommandResult digital_setValue(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {
        //DIGital:PIN#:VALue

        if (nodeNumbers[1] < 0) {
            return CommandResult::MissingParam;
        }

        ChanIndex channel = nodeNumbers[1];
        bool val;
        
        if (parser->parseBool(val) != ParseResult::Success)  {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.Digital.SetOutput(channel, val);

        return CommandResult::Success;
    }

    QueryResult digital_getValue(ScpiNode* node, const NumParamVector& nodeNumbers) {
        // DIGital:PIN#:VALue?

        if (nodeNumbers[1] < 0) {
            return QueryResult::Error;
        }

        ChanIndex channel;
        bool val;
        
        channel = nodeNumbers[1]; //Pull number from PIN part of tree

        gPlatform.IO.Digital.GetValue(channel, &val);
        
        if (val) {
            gPlatform.IO.Print("1");
        } else {
            gPlatform.IO.Print("0");
        }

        return QueryResult::Success;
    }

    CommandResult digital_setDirection(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {
        //DIGital:PIN#:DIRection

        if (nodeNumbers[1] < 0) {
            return CommandResult::MissingParam;
        }

        ChanIndex channel;
        uint8_t choice;
        
        channel = nodeNumbers[1]; // pull # from PIN portion of tree

        if (parser->parseChoice(digitalDirectionOptions, choice) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.Digital.SetDirection(channel, choice == 1);

        return CommandResult::Success;
    }

    QueryResult digital_getDirection(ScpiNode* node, const NumParamVector& nodeNumbers) {
        //DIGital:PIN#:DIRection?

        if (nodeNumbers[1] < 0) {
            return QueryResult::Error;
        }

        ChanIndex channel = nodeNumbers[1]; //extract # from PIN portion
        bool out;

        gPlatform.IO.Digital.GetDirection(channel, &out);

        if (out) {
            gPlatform.IO.Print("OUT");
        } else {
            gPlatform.IO.Print("IN");
        }

        return QueryResult::Success;
    }

    CommandResult digital_setPull(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {
        //DIGital:PIN#:PULL

        if (nodeNumbers[1] < 0) {
            return CommandResult::MissingParam;
        }

        ChanIndex channel = nodeNumbers[1];
        uint8_t choice;

        if (parser->parseChoice(digitalPullOptions, choice) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.Digital.SetPull(channel, (PlatformDigital::PullDirection)choice);

        return CommandResult::Success;
    }

    QueryResult digital_getPull(ScpiNode* node, const NumParamVector& nodeNumbers) {
        //DIGital:PIN#:PULL?

        if (nodeNumbers[1] < 0) {
            return QueryResult::Error;
        }

        ChanIndex channel = nodeNumbers[1];

        PlatformDigital::PullDirection pull;

        gPlatform.IO.Digital.GetPull(channel, &pull);

        switch (pull) {
            case PlatformDigital::None:
                gPlatform.IO.Print("NONE");
                break;
            
            case PlatformDigital::Up:
                gPlatform.IO.Print("UP");
                break;
            
            case PlatformDigital::Down:
                gPlatform.IO.Print("DOWN");
                break;
            
            case PlatformDigital::Both:
                gPlatform.IO.Print("BOTH");
                break;
        }

        return QueryResult::Success;
    }

    void initDigitalCommands(Visa* visa) {
        visa->addCommand("DIGital:PIN#:DIRection", digital_setDirection, digital_getDirection);
        visa->addCommand("DIGital:PIN#:VALue",     digital_setValue,     digital_getValue);
        visa->addCommand("DIGital:PIN#:PULL",      digital_setPull,      digital_getPull);
    }

} //namespace Visa
} //namespace CTI