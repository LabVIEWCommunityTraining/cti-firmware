#include "cti/platform.h"
#include <pico/stdlib.h>

using namespace CTI;

void PlatformTimer::SleepMilliseconds(int64_t milliseconds) { sleep_ms(milliseconds); }