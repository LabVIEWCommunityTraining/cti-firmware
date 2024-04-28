#include "visa/visa_core.h"
#include "visa/digital.h"

#include "scpi.h"

#include <cstring>

namespace CTI {
namespace Visa {

    using namespace SCPI;

    void initPlatformCommands(Visa* visa);

    /*size_t SCPI_Write(scpi_t * context, const char * data, size_t len) {
        gPlatform.IO.Print(len, data);
        return len;
    }

    scpi_result_t SCPI_Flush(scpi_t * context) {
        (void) context;
        gPlatform.IO.Flush();
        return SCPI_RES_OK;
    }

    int SCPI_Error(scpi_t * context, int_fast16_t err) {
        (void) context;
        gPlatform.IO.Printf("**ERROR: %d, \"%s\"\n", err, SCPI_ErrorTranslate(err));
        return 0;
    }

    scpi_result_t SCPI_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
        (void) context;

        if (SCPI_CTRL_SRQ == ctrl) {
            gPlatform.IO.Printf("**SRQ: 0x%X(%d)\n", val, val);
        } else {
            gPlatform.IO.Printf("**CTRL: %X: 0x%X(%d)\n", ctrl, val, val);
        }
        return SCPI_RES_OK;
    }*/

    CommandResult SCPI_Reset(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {
        gPlatform.IO.Print("**Reset\n");
        return CommandResult::Success;
    }

    CommandResult scpi_LED(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {
        bool val;
        
        if (parser->parseBool(val) != ParseResult::Success)  {
            return CommandResult::MissingParam;
        }

        gPlatform.IO.StatusLED(val, User);

        return CommandResult::Success;
    }

    ScpiChoice statusSource[] = {
        { "COMMS", 0 },
        { "USER",  1 },

        NullScpiChoice
    };

    CommandResult scpi_cmdStatusSource(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {
        uint8_t choice;

        if (parser->parseChoice(statusSource, choice) != ParseResult::Success) {
            return CommandResult::UnexpectedParam;
        }

        gPlatform.IO.SetStatusSource((StatusSource)choice);

        return CommandResult::Success;
    }

    QueryResult scpi_queryStatusSource(ScpiNode* node, const NumParamVector& nodeNumbers) {

        const char* str = statusSource[gPlatform.IO.GetStatusSource()].choiceString;
        gPlatform.IO.Print(str);

        return QueryResult::Success;
    }

    CommandResult SCPI_CoreCls(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {

        return CommandResult::Success;
    }

    CommandResult SCPI_CoreEse(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {

        return CommandResult::Success;
    }

    QueryResult SCPI_CoreEseQ(ScpiNode* node, const NumParamVector& nodeNumbers) {

        return QueryResult::Success;
    }

    QueryResult SCPI_CoreEsrQ(ScpiNode* node, const NumParamVector& nodeNumbers) {

        return QueryResult::Success;
    }

    QueryResult SCPI_CoreIdnQ(ScpiNode* node, const NumParamVector& nodeNumbers) {
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

    CommandResult SCPI_CoreOpc(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {

        return CommandResult::Success;
    }

    QueryResult SCPI_CoreOpcQ(ScpiNode* node, const NumParamVector& nodeNumbers) {

        return QueryResult::Success;
    }

    CommandResult SCPI_CoreRst(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {

        return CommandResult::Success;
    }

    CommandResult SCPI_CoreSre(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {

        return CommandResult::Success;
    }

    QueryResult SCPI_CoreSreQ(ScpiNode* node, const NumParamVector& nodeNumbers) {

        return QueryResult::Success;
    }

    QueryResult SCPI_CoreStbQ(ScpiNode* node, const NumParamVector& nodeNumbers) {

        return QueryResult::Success;
    }

    CommandResult SCPI_CoreWai(ScpiNode* node, ScpiParser* parser, const NumParamVector& nodeNumbers) {

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
        char inputBuffer[255];
        int inputPosition = 0;

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
                    gPlatform.IO.Print("**ERR: Input Overflow");

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
        //initAnalogCommands(visa);
        //initPWMCommands(visa);
    }

} //namespace Visa

    //The global Visa instance usable as CTI::gVisa
    Visa::Visa gVisa;
} //namespace CTI