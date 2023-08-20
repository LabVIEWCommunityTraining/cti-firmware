#include "cti/platform.h"

#include <pico/stdlib.h>
#include <pico/unique_id.h>

#include <cstring>

using namespace CTI;

const uint32_t unique_id_len = PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1;
char unique_id[unique_id_len] = "0000000000000000";

void platform_init() {
    pico_get_unique_board_id_string(unique_id, unique_id_len);

    stdio_usb_init();

    bool status = false;
    
    //need to give time for USB init and enumeration
    for (int i = 0; i < 12; ++i) {
        sleep_ms(400);
        status = !status;
        gPlatform.IO.StatusLED(status);
    }
}

void Platform::init() {
}

const char* PlatformInfo::Model() const { return "RP2040_Visa-uDAQ"; };

//TODO: Proper rp2040 unique ID
const char* PlatformInfo::SerialNum() const {
    return unique_id;
};