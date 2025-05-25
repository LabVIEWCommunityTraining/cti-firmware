#include <stdio.h>

#ifdef CTI_VISA
    #include "visa.h"
#else
    #ifdef CTI_UDAQ
        #include "udaq.h"
    #else
        #error "No firmware mode set. Ensure a define has been configured for CTI_VISA or CTI_UDAQ"
    #endif
#endif

using namespace CTI;
using namespace Visa;

int main() {

    //Allow any board specific initialization to happen.
    gPlatform.BoardInit();

    //First interfacing for debugging capabilities is a built in LED, if available.
    // this can be blinked and much more robust than other comm mechanisms for
    // complete initialization failures.
    gPlatform.IO.InitStatusLED();

    //This kicks off any initialization the platform specific code needs to do
    // before initializing the platform abstraction code. This usually means
    // ensuring any communication and debugging hooks are available and
    // and customized startup logic for a specific platform.
    gPlatform.Preinit();

    //Initialize the global platform abstractions
    gPlatform.Setup();

    //TODO: Launch different engines on different cores when available.
    gPlatform.IO.Print("Starting engine\n");
    int status = gPlatform.pEngine->Ready();
    if (status == 0) {
        gPlatform.pEngine->MainLoop();
    } else {
        gPlatform.IO.Printf("Engine error: %s\n", gPlatform.pEngine->StatusText(status));
        gPlatform.IO.Flush();
        gPlatform.Timer.SleepMilliseconds(1000);
    }
    
    return 0;
}