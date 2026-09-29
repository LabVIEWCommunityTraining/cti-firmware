#include "cti/platform.h"

#include <esp_timer.h>
#include <esp_rom_sys.h>

void CTI::PlatformTimer::SleepMilliseconds(int64_t milliseconds) {
    esp_rom_delay_us(static_cast<uint32_t>(milliseconds * 1000));
}
