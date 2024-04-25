#include "visa/visa_core.h"
#include "visa/digital.h"
#include "visa/analog.h"
#include "visa/pwm.h"

#include "scpi.h"

#include <string.h>

namespace CTI {
namespace Visa {

    scpi_choice_def_t statusSources[] = {
        {"COMMS", 0},
        {"USER", 1},
        SCPI_CHOICE_LIST_END
    };

    void initPlatformCommands(Visa* visa);

    size_t SCPI_Write(scpi_t * context, const char * data, size_t len) {
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
    }

    scpi_result_t SCPI_Reset(scpi_t * context) {
        (void) context;

        gPlatform.IO.Print("**Reset\n");
        return SCPI_RES_OK;
    }

    scpi_result_t scpi_LED(scpi_t * context) {
        bool val;
        
        if (!SCPI_ParamBool(context, &val, true))  {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.StatusLED(val, User);

        return SCPI_RES_OK;
    }

    scpi_result_t scpi_cmdStatusSource(scpi_t* context) {
        int32_t choice;

        if (!SCPI_ParamChoice(context, statusSources, &choice, true)) {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.SetStatusSource((StatusSource)choice);

        return SCPI_RES_OK;
    }

    scpi_result_t scpi_queryStatusSource(scpi_t* context) {
        switch(gPlatform.IO.GetStatusSource()) {
        case Comms:
            gPlatform.IO.Print("COMMS");
            break;

        case User:
            gPlatform.IO.Print("USER");
            break;
        }

        return SCPI_RES_OK;
    }

    void initCommonCommands(Visa* visa) {
        //standard SCPI commands
        visa->addCommand({"*CLS", SCPI_CoreCls, 0});
        visa->addCommand({"*ESE", SCPI_CoreEse, 0});
        visa->addCommand({"*ESE?", SCPI_CoreEseQ, 0});
        visa->addCommand({"*ESR?", SCPI_CoreEsrQ, 0});
        visa->addCommand({"*IDN?", SCPI_CoreIdnQ, 0});
        visa->addCommand({"*OPC", SCPI_CoreOpc, 0});
        visa->addCommand({"*OPC?", SCPI_CoreOpcQ, 0});
        visa->addCommand({"*RST", SCPI_CoreRst, 0});
        visa->addCommand({"*SRE", SCPI_CoreSre, 0});
        visa->addCommand({"*SRE?", SCPI_CoreSreQ, 0});
        visa->addCommand({"*STB?", SCPI_CoreStbQ, 0});
        //visa->addCommand({"*TST?", My_CoreTstQ, 0});
        visa->addCommand({"*WAI", SCPI_CoreWai, 0});

        // SET:LED is a common command as gPlatform has a status LED abstraction
        visa->addCommand({"STATus:USER", scpi_LED, 0});
        visa->addCommand({"STATus:SOURce", scpi_cmdStatusSource, 0});
        visa->addCommand({"STATus:SOURce?", scpi_queryStatusSource, 0});
    }

    Visa::Visa() {
        _interface = {
            SCPI_Error,
            SCPI_Write,
            SCPI_Control,
            SCPI_Flush,
            SCPI_Reset
        };

        _nextCmdI = 0;

        initCommonCommands(this);
        initPlatformCommands(this);
        _init();
    }
        
    Status Visa::addCommand(scpi_command_t command) {
        if (_ready) {
            return AlreadyReady;
        }

        if (_nextCmdI == SCPI_MAX_COMMANDS) {
            return TooManyCommands;
        }

        _commands[_nextCmdI++] = command;

        return Success;
    }

    int Visa::Ready() {
        //mark _ready as true which means the command list is finalized
        _ready = true;

        //put the end of the command list special item at the end to use with the scpi-parser lib
        _commands[_nextCmdI] = SCPI_CMD_LIST_END;

        SCPI_Init(&_context,
            _commands,
            &_interface,
            scpi_units_def,
            gPlatform.Info.Vendor(),
            gPlatform.Info.Model(),
            gPlatform.Info.SerialNum(),
            gPlatform.Info.Version(),
            _input_buffer, SCPI_INPUT_BUFFER_LENGTH,
            _error_queue, SCPI_ERROR_QUEUE_SIZE);
        

        return (int)Success;
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
                SCPI_Input(&_context, &c, 1);
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
    }

} //namespace Visa

    //The global Visa instance usable as CTI::gVisa
    Visa::Visa gVisa;
} //namespace CTI