#include "cti/platform.h"
#include "avr_timer.h"

#include <util/delay.h>

using namespace CTI;

platform_tick_t PlatformTimer::TickCount() {
    return micros();
}

platform_tick_t PlatformTimer::MicrosecondsToTickCount(int64_t microseconds) {
    return static_cast<platform_tick_t>(microseconds);
}

int64_t PlatformTimer::TickCountToMilliseconds(platform_tick_t ticks) {
    return ticks / 1000;
}

int64_t PlatformTimer::TickCountToMicroseconds(platform_tick_t ticks) {
    return ticks;
}

platform_tick_t PlatformTimer::MillisecondsFromNowToTickCount(int64_t milliseconds) {
    return TickCount() + static_cast<platform_tick_t>(milliseconds * 1000);
}

platform_tick_t PlatformTimer::MicrosecondsFromNowToTickCount(int64_t microseconds) {
    return TickCount() + static_cast<platform_tick_t>(microseconds);
}

void PlatformTimer::SleepMilliseconds(int64_t milliseconds) {
    while (milliseconds-- > 0) {
        _delay_ms(1);
    }
}
