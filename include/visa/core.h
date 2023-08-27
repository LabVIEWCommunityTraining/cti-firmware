#ifndef visa_core_h_
#define visa_core_h_

#include "scpi/scpi.h"
#include "cti/platform.h"

//#include <vector>

#define SCPI_INPUT_BUFFER_LENGTH 256
#define SCPI_ERROR_QUEUE_SIZE 17
#define SCPI_MAX_COMMANDS 1000

namespace CTI {
namespace Visa {

    typedef enum {
        Success,
        AmbiguousCommand,
        DuplicateCommand,
        AmbiguousOptionalSegment,
        InvalidPattern,
        AlreadyReady,
        TooManyCommands
    } Status;
    
    class Visa : public PlatformEngine {
    public:
        Visa();
        
        Status addCommand(scpi_command_t command);

        virtual int Ready();
        virtual void MainLoop();
        virtual const char* StatusText(int status);

    private:
        bool _ready;
        scpi_t _context;
        const char* _vendor;
        const char* _model;
        const char* _serial;
        const char* _version;
        scpi_command_t _commands[SCPI_MAX_COMMANDS + 1];
        int _nextCmdI;
        char _input_buffer[SCPI_INPUT_BUFFER_LENGTH];
        scpi_error_t _error_queue[SCPI_ERROR_QUEUE_SIZE];
        scpi_interface_t _interface;
    };

} //namespace Visa

    //make gVisa accessible as CTI::gVisa and not nested within Visa namespace.
    //the idea is that all higher level code uses the CTI namespace.
    // this will also be pointed to by gPlatform.pEngine
    extern Visa::Visa gVisa;

} //namespace CTI

#endif //visa_core_h_