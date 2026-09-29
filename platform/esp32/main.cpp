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

extern "C" void app_main() {
    gPlatform.BoardInit();
    gPlatform.IO.InitStatusLED();
    gPlatform.Preinit();
    gPlatform.Setup();

#ifndef CTI_SILENT_STARTUP
    gPlatform.IO.Print("Starting engine\n");
#endif
    int status = gPlatform.pEngine->Ready();
    if (status == 0) {
        gPlatform.pEngine->MainLoop();
    } else {
        gPlatform.IO.Printf("Engine error: %s\n", gPlatform.pEngine->StatusText(status));
        gPlatform.IO.Flush();
        gPlatform.Timer.SleepMilliseconds(1000);
    }
}
