#include "cti/platform.h"
#include "cti/version.h"

namespace CTI {

Platform gPlatform;

void Platform::Setup() {
    gPlatform.IO.Print(" == Initializing Firmware ==\n");
    gPlatform.IO.Printf("Vendor: %s\n", gPlatform.Info.Vendor());
    gPlatform.IO.Printf("Model: %s\n", gPlatform.Info.Model());
    gPlatform.IO.Printf("Serial: %s\n", gPlatform.Info.SerialNum());
    gPlatform.IO.Printf("Version: %s\n", CTI_VERSION_FULL);
    gPlatform.IO.Flush();
    gPlatform.Timer.SleepMilliseconds(1000);
}

}; //namespace CTI