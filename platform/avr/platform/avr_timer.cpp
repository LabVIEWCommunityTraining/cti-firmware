#include "cti/platform.h"

#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>

using namespace CTI;

void PlatformTimer::SleepMilliseconds(int64_t milliseconds) { _delay_ms(milliseconds); }