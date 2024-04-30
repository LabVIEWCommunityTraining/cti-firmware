#include "visa/visa_core.h"

#include <string.h>

namespace CTI {
namespace Visa {

    using namespace SCPI;

    ScpiChoice digitalDirectionOptions[] = {
        {"IN",  0},
        {"OUT", 1},
        EndScpiChoice
    };

    ScpiChoice digitalPullOptions[] = {
        {"NONE", (uint8_t)PlatformDigital::None},
        {"UP",   (uint8_t)PlatformDigital::Up},
        {"DOWN", (uint8_t)PlatformDigital::Down},
        {"BOTH", (uint8_t)PlatformDigital::Both},
        EndScpiChoice
    };

    CommandResult digital_setValue(ScpiParser* scpi) {
        //DIGital:PIN#:VALue

        ChanIndex channel = scpi->nodeNum(1);

        // gPlatform.IO.Printf("channel: %d\n", channel);

        if (channel < 0) {
            return CommandResult::MissingParam;
        }

        bool val;
        if (scpi->parseBool(val) != ParseResult::Success)  {
            return CommandResult::MissingParam;
        }

        // gPlatform.IO.Printf("val: %d\n\n", val ? 1 : 0);

        gPlatform.IO.Digital.SetOutput(channel, val);

        return CommandResult::Success;
    }

    QueryResult digital_getValue(ScpiParser* scpi) {
        // DIGital:PIN#:VALue?
        ChanIndex channel = scpi->nodeNum(1);

        if (channel < 0) {
            return QueryResult::Error;
        }

        bool val;

        gPlatform.IO.Digital.GetValue(channel, &val);
        
        if (val) {
            gPlatform.IO.Print("1\n");
        } else {
            gPlatform.IO.Print("0\n");
        }

        return QueryResult::Success;
    }

    CommandResult digital_setDirection(ScpiParser* scpi) {
        //DIGital:PIN#:DIRection
        ChanIndex channel = scpi->nodeNum(1);

        if (channel < 0) {
            return CommandResult::MissingParam;
        }

        uint8_t choice;

        if (scpi->parseChoice(digitalDirectionOptions, choice) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.Digital.SetDirection(channel, choice == 1);

        return CommandResult::Success;
    }

    QueryResult digital_getDirection(ScpiParser* scpi) {
        //DIGital:PIN#:DIRection?
        ChanIndex channel = scpi->nodeNum(1);

        if (channel < 0) {
            return QueryResult::Error;
        }

        bool out;

        gPlatform.IO.Digital.GetDirection(channel, &out);

        if (out) {
            gPlatform.IO.Print("OUT\n");
        } else {
            gPlatform.IO.Print("IN\n");
        }

        return QueryResult::Success;
    }

    CommandResult digital_setPull(ScpiParser* scpi) {
        //DIGital:PIN#:PULL
        ChanIndex channel = scpi->nodeNum(1);

        if (channel < 0) {
            return CommandResult::MissingParam;
        }

        uint8_t choice;

        if (scpi->parseChoice(digitalPullOptions, choice) != ParseResult::Success) {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.Digital.SetPull(channel, (PlatformDigital::PullDirection)choice);

        return CommandResult::Success;
    }

    QueryResult digital_getPull(ScpiParser* scpi) {
        //DIGital:PIN#:PULL?
        ChanIndex channel = scpi->nodeNum(1);

        if (channel < 0) {
            return QueryResult::Error;
        }

        PlatformDigital::PullDirection pull;

        gPlatform.IO.Digital.GetPull(channel, &pull);

        switch (pull) {
            case PlatformDigital::None:
                gPlatform.IO.Print("NONE\n");
                break;
            
            case PlatformDigital::Up:
                gPlatform.IO.Print("UP\n");
                break;
            
            case PlatformDigital::Down:
                gPlatform.IO.Print("DOWN\n");
                break;
            
            case PlatformDigital::Both:
                gPlatform.IO.Print("BOTH\n");
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