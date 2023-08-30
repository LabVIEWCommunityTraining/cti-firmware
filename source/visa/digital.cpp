#include "visa/core.h"

#include <string.h>

namespace CTI {
namespace Visa {

    scpi_choice_def_t digitalDirectionOptions[] = {
        {"IN", 0},
        {"OUT", 1},
        SCPI_CHOICE_LIST_END
    };

    scpi_choice_def_t digitalPullOptions[] = {
        {"NONE", (int32_t)PlatformDigital::None},
        {"UP", (int32_t)PlatformDigital::Up},
        {"DOWN", (int32_t)PlatformDigital::Down},
        {"BOTH", (int32_t)PlatformDigital::Both},
        SCPI_CHOICE_LIST_END
    };

    //scpi_result_t scpi_LED(scpi_t * context)

    scpi_result_t digital_setValue(scpi_t * context) {
        int32_t channel;
        bool val;
        
        if (!SCPI_ParamInt(context, &channel, true))  {
            return SCPI_RES_ERR;
        }
        
        if (!SCPI_ParamBool(context, &val, true))  {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Digital.SetOutput(channel, val);

        return SCPI_RES_OK;
    }

    scpi_result_t digital_getValue(scpi_t * context) {
        int32_t channel;
        bool val;
        
        if (!SCPI_ParamInt(context, &channel, true))  {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Digital.GetValue(channel, &val);
        
        if (val) {
            gPlatform.IO.Print("ON");
        } else {
            gPlatform.IO.Print("OFF");
        }

        return SCPI_RES_OK;
    }

    scpi_result_t digital_setDirection(scpi_t * context) {
        int32_t channel;
        int32_t choice;
        
        if (!SCPI_ParamInt(context, &channel, true))  {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamChoice(context, digitalDirectionOptions, &choice, true)) {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Digital.SetDirection(channel, choice == 1);

        return SCPI_RES_OK;
    }

    scpi_result_t digital_getDirection(scpi_t * context) {
        int32_t channel;
        bool out;
        
        if (!SCPI_ParamInt(context, &channel, true))  {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Digital.GetDirection(channel, &out);

        if (out) {
            gPlatform.IO.Print("OUT");
        } else {
            gPlatform.IO.Print("IN");
        }

        return SCPI_RES_OK;
    }

    scpi_result_t digital_setPull(scpi_t * context) {
        int32_t channel;
        int32_t choice;
        
        if (!SCPI_ParamInt(context, &channel, true))  {
            return SCPI_RES_ERR;
        }

        if (!SCPI_ParamChoice(context, digitalPullOptions, &choice, true)) {
            return SCPI_RES_ERR;
        }

        gPlatform.IO.Digital.SetPull(channel, (PlatformDigital::PullDirection)choice);

        return SCPI_RES_OK;
    }

    scpi_result_t digital_getPull(scpi_t * context) {
        int32_t channel;
        
        if (!SCPI_ParamInt(context, &channel, true))  {
            return SCPI_RES_ERR;
        }

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

        return SCPI_RES_OK;
    }

    void initDigitalCommands(Visa* visa) {
        visa->addCommand({"DIGital:DIRection", digital_setDirection, 0});
        visa->addCommand({"DIGital:DIRection?", digital_getDirection, 0});
        visa->addCommand({"DIGital[:OUTput]:VALue", digital_setValue, 0});
        visa->addCommand({"DIGital[:OUTput]:VALue?", digital_getValue, 0});
        visa->addCommand({"DIGital[:INput]:VALue?", digital_getValue, 0});
        visa->addCommand({"DIGital[:INput]:PULL", digital_setPull, 0});
        visa->addCommand({"DIGital[:INput]:PULL?", digital_getPull, 0});
    }

} //namespace Visa
} //namespace CTI