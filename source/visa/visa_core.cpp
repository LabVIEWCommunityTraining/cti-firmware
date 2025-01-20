#include "visa/visa_core.h"
#include "visa/digital.h"
#include "visa/analog.h"
#include "visa/pwm.h"
#include "visa/uart.h"
#include "visa/i2c.h"
#include "visa/spi.h"

#include "scpi.h"

#include <cstring>

namespace CTI {
namespace Visa {

    using namespace SCPI;

    void initPlatformCommands(Visa* visa);

    CommandResult SCPI_Reset(ScpiParser* scpi) {
        gPlatform.IO.Print("**Reset\n");
        return CommandResult::Success;
    }

    CommandResult scpi_LED(ScpiParser* scpi) {
        bool val;
        
        if (scpi->parseBool(val) != ParseResult::Success)  {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.StatusLED(val, User);

        return CommandResult::Success;
    }

    ScpiChoice statusSource[] = {
        { "COMMS", 0 },
        { "USER",  1 },

        EndScpiChoice
    };

    CommandResult scpi_cmdStatusSource(ScpiParser* scpi) {
        int32_t choice;

        // gPlatform.IO.Print("cmdStatusSource()\n");

        if (scpi->parseChoice(statusSource, choice) != ParseResult::Success) {
            return CommandResult::UnexpectedParam;
        }

        gPlatform.IO.SetStatusSource((StatusSource)choice);

        return CommandResult::Success;
    }

    QueryResult scpi_queryStatusSource(ScpiParser* scpi) {

        const char* str = statusSource[gPlatform.IO.GetStatusSource()].choiceString;
        gPlatform.IO.Print(str);
        gPlatform.IO.Print('\n');

        return QueryResult::Success;
    }

    CommandResult SCPI_CoreCls(ScpiParser* scpi) {

        return CommandResult::Success;
    }

    CommandResult SCPI_CoreEse(ScpiParser* scpi) {

        return CommandResult::Success;
    }

    QueryResult SCPI_CoreEseQ(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    QueryResult SCPI_CoreEsrQ(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    QueryResult SCPI_CoreIdnQ(ScpiParser* scpi) {
        gPlatform.IO.Print(gPlatform.Info.Vendor());
        gPlatform.IO.Print(',');
        gPlatform.IO.Print(gPlatform.Info.Model());
        gPlatform.IO.Print(',');
        gPlatform.IO.Print(gPlatform.Info.SerialNum());
        gPlatform.IO.Print(',');
        gPlatform.IO.Print(gPlatform.Info.Version());
        gPlatform.IO.Print('\n');
        
        return QueryResult::Success;
    }

    CommandResult SCPI_CoreOpc(ScpiParser* scpi) {

        return CommandResult::Success;
    }

    QueryResult SCPI_CoreOpcQ(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    CommandResult SCPI_CoreRst(ScpiParser* scpi) {

        return CommandResult::Success;
    }

    CommandResult SCPI_CoreSre(ScpiParser* scpi) {

        return CommandResult::Success;
    }

    QueryResult SCPI_CoreSreQ(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    QueryResult SCPI_CoreStbQ(ScpiParser* scpi) {

        return QueryResult::Success;
    }

    CommandResult SCPI_CoreWai(ScpiParser* scpi) {

        return CommandResult::Success;
    }

    void initCommonCommands(Visa* visa) {
        visa->addCommand("*CLS", SCPI_CoreCls, nullptr);
        visa->addCommand("*ESE", SCPI_CoreEse, SCPI_CoreEseQ);
        visa->addCommand("*ESR", nullptr, SCPI_CoreEsrQ);
        visa->addCommand("*IDN", nullptr, SCPI_CoreIdnQ);
        visa->addCommand("*OPC", SCPI_CoreOpc, SCPI_CoreOpcQ);
        visa->addCommand("*RST", SCPI_CoreRst, nullptr);
        visa->addCommand("*SRE", SCPI_CoreSre, SCPI_CoreSreQ);
        visa->addCommand("*STB", nullptr, SCPI_CoreStbQ);
        //visa->addCommand({"*TST?", My_CoreTstQ, nullptr);
        visa->addCommand("*WAI", SCPI_CoreWai, nullptr);

        // SET:LED is a common command as gPlatform has a status LED abstraction
        visa->addCommand("STATus:USER", scpi_LED, nullptr);
        visa->addCommand("STATus:SOURce", scpi_cmdStatusSource, scpi_queryStatusSource);
    }

    Visa::Visa():
        _parser(SCPI_INPUT_BUFFER_LENGTH) {
    }

    int Visa::Ready() {
        initCommonCommands(this);
        initPlatformCommands(this);
        _init();

        //mark _ready as true which means the command list is finalized
        _ready = true;
        _parser.finalize();

        return 0;
    }

    void Visa::MainLoop() {
        int val; // timeout read value, will be -1 if timed out
        char c;

        while(true) {
            val = gPlatform.IO.FGetCtimeout(20000); //20 ms

            if (val == -1) {
                gPlatform.IO.StatusLED(false, Comms);
            } else {
                c = val;
                gPlatform.IO.StatusLED(true, Comms);
                if (!_parser.bufferInput(&c, 1)) {
                    gPlatform.IO.Print("**ERROR: Input Overflow");

                    //consume rest of input and reset parser
                    while (true) {
                        val = gPlatform.IO.FGetCtimeout(20000);
                        if (val == -1 || val == '\n') {
                            _parser.reset();
                            break;
                        }
                    }
                }
            }
        }
    }

    const char* Visa::StatusText(int status) {
        return "";
    }

    void initPlatformCommands(Visa* visa) {
        initDigitalCommands(visa);
        initAnalogCommands(visa);
        initPWMCommands(visa);
        initUartCommands(visa);
        initI2CCommands(visa);
        initSPICommands(visa);
    }

    SCPI::QueryResult PrintBlock(size_t len, const uint8_t *data) {
        size_t len2 = len;
        size_t digits = 1;

        while (len2 > 10) {
            digits++;
            len2 /= 10;
        }

        gPlatform.IO.Printf("#%d%d", digits, len);

        for (size_t i = 0; i < len; ++i) {
            gPlatform.IO.Print(data[i]);
        }

        return QueryResult::Success;
    }

} //namespace Visa

    //The global Visa instance usable as CTI::gVisa
    Visa::Visa gVisa;
} //namespace CTI