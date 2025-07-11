#include "visa/visa_core.h"
#include "visa/digital.h"
#include "visa/analog.h"
#include "visa/pwm.h"
#include "visa/uart.h"
#include "visa/i2c.h"
#include "visa/spi.h"

#ifdef CTI_FEATURE_WIFI
#include "visa/wifi.h"
#endif

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
        scpi->clearErrors();

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

    QueryResult SCPI_SystErr(ScpiParser* scpi) {
        int16_t code;
        char* str;

        scpi->dequeueError(&code, &str);

        gPlatform.IO.Printf("%d, %s\n", code, str);

        return QueryResult::Success;
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

        visa->addCommand("SYSTem:ERRor", nullptr, SCPI_SystErr);

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

        // store the stream that was set before reading data, if it changes
        // the parser will need to be reset. In the future maybe there could
        // just be a parser implementation per stream but for now we still
        // have a single parser instance.

        while(true) {
            CTI_DEBUG("I");
            int curStream = gPlatform.IO.CurStream();

            // Allow checking all streams if current one times out
            val = gPlatform.IO.FGetCtimeout(20000, true); //1 ms

            if (curStream != gPlatform.IO.CurStream()) {
                CTI_DEBUG("R");
                // stream changed, need to reset parser
                _parser.reset();
            }

            CTI_DEBUG("\n");

            if (val == -1) {
                gPlatform.IO.StatusLED(false, Comms);
            } else {
                c = val;
                gPlatform.IO.StatusLED(true, Comms);
                if (!_parser.bufferInput(&c, 1)) {
                    gPlatform.IO.Print("**ERROR: Input Overflow");

                    //consume rest of input and reset parser
                    while (true) {
                        // don't allow changing streams so we don't have to
                        // handle data from a changed stream here
                        // just want to finish up the current exchange
                        val = gPlatform.IO.FGetCtimeout(20000, false);
                        if (val == -1 || val == '\n') {
                            _parser.reset();
                            break;
                        }
                    }
                }

                gPlatform.IO.Flush(); //ensure all buffered output is sent out.
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
        size_t digits = 1; // all lens are at least length 1 in digits

        //Determine the additional # of digits in the len
        while (len2 >= 10) {
            digits++;
            len2 /= 10;
        }

        //Arbitrary block header #<LenDigitCount><Len>
        gPlatform.IO.Printf("#%d%d", digits, len);

        //iterate over data portion of block and send as-is
        for (size_t i = 0; i < len; ++i) {
            gPlatform.IO.Print(data[i]);
        }

        return QueryResult::Success;
    }

} //namespace Visa

    //The global Visa instance usable as CTI::gVisa
    Visa::Visa gVisa;
} //namespace CTI