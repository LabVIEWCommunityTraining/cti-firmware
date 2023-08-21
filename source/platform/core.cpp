#include "cti/platform.h"
#include "cti/version.h"

#ifdef CTI_VISA
    #include "visa/core.h"
    #define CTI_ENGINE gVisa
#else
    #ifdef CTI_UDAQ
        #include "udaq/core.h"
        #define CTI_ENGINE gUdaq
    #else
        #error "No firmware mode set. Ensure a define has been configured for CTI_VISA or CTI_UDAQ"
    #endif
#endif

namespace CTI {

Platform gPlatform;

void Platform::Setup() {

    //Display the startup message and info.
    //Platform::Preinit() which is implemented by a specific platform
    // should already have ensured that IO functionality is ready.
    // Output availability will depend on the communication method
    // and the connection being active.

    const char* engine = CTI_STR(CTI_ENGINE);

    gPlatform.IO.Print(" == Initializing Firmware ==\n");
    gPlatform.IO.Printf("Vendor: %s\n", gPlatform.Info.Vendor());
    gPlatform.IO.Printf("Model: %s\n", gPlatform.Info.Model());
    gPlatform.IO.Printf("Serial: %s\n", gPlatform.Info.SerialNum());
    gPlatform.IO.Printf("Version: %s\n", gPlatform.Info.Version());
    gPlatform.IO.Printf("Engine: %s\n", engine ? engine : "NULL");
    gPlatform.IO.Flush();

    gPlatform.Timer.SleepMilliseconds(2000);

    // Set the engine pointer to the specific type
    gPlatform.pEngine = &CTI_ENGINE;
}

}; //namespace CTI