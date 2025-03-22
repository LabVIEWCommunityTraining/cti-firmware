#ifndef visa_core_h_
#define visa_core_h_

#include "scpi.h"
#include "cti/platform.h"

//#include <vector>

#define SCPI_INPUT_BUFFER_LENGTH 256

namespace CTI {
namespace Visa {
    
    class Visa : public PlatformEngine {
    public:
        Visa();

        virtual int Ready();
        virtual void MainLoop();
        virtual const char* StatusText(int status);

        SCPI::RegistrationResult addCommand(const char* cmdStr, SCPI::ScpiCommand cmd, SCPI::ScpiQuery query) {
            return _parser.registerNode(cmdStr, cmd, query);
        }

    private:
        void _init();

        SCPI::ScpiParser _parser;
        
        bool _ready;
        const char* _vendor;
        const char* _model;
        const char* _serial;
        const char* _version;
    };

    SCPI::QueryResult PrintBlock(size_t len, const uint8_t *data);

} //namespace Visa

    //make gVisa accessible as CTI::gVisa and not nested within Visa namespace.
    //the idea is that all higher level code uses the CTI namespace.
    // this will also be pointed to by gPlatform.pEngine
    extern Visa::Visa gVisa;

} //namespace CTI

#endif //visa_core_h_