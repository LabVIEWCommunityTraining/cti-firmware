#include "cti/platform.h"
#include "cti_board.h"

#include <esp_system.h>
#include <esp_timer.h>
#include <esp_idf_version.h>
#include <esp_mac.h>
#include <esp_log.h>

#include <cstdio>

using namespace CTI;

static char unique_id[18] = "000000000000";

void Platform::Preinit() {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    std::snprintf(unique_id, sizeof(unique_id), "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    setvbuf(stdout, nullptr, _IONBF, 0);
}

void Platform::Init() {
}

void Platform::Shutdown() {
}

const char* PlatformInfo::Model() const { return CTI_HW_BOARD_NAME; }
const char* PlatformInfo::SerialNum() const { return unique_id; }

CTI::platform_tick_t CTI::PlatformTimer::TickCount() {
    return esp_timer_get_time();
}

CTI::platform_tick_t CTI::PlatformTimer::MicrosecondsToTickCount(int64_t microseconds) {
    return microseconds;
}

int64_t CTI::PlatformTimer::TickCountToMilliseconds(platform_tick_t ticks) {
    return ticks / 1000;
}

int64_t CTI::PlatformTimer::TickCountToMicroseconds(platform_tick_t ticks) {
    return ticks;
}

CTI::platform_tick_t CTI::PlatformTimer::MillisecondsFromNowToTickCount(int64_t milliseconds) {
    return TickCount() + milliseconds * 1000;
}

CTI::platform_tick_t CTI::PlatformTimer::MicrosecondsFromNowToTickCount(int64_t microseconds) {
    return TickCount() + microseconds;
}
