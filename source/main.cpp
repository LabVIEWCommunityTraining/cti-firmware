#include <cstdio>

#include "cti/visa.h"
#include "cti/udaq.h"
#include "cti.h"

using namespace CTI;
using namespace Visa;

void platform_init();

int main() {

    gPlatform.IO.InitStatusLED();

    platform_init();
    
    gPlatform.Setup();

    //gVisa.ready();
    
    return 0;
}