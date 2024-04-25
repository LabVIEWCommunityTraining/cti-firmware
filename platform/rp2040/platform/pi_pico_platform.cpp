#include "cti/platform.h"

#include <pico/stdlib.h>
#include <pico/unique_id.h>
#include "hardware/adc.h"

#include <cstring>

using namespace CTI;

const uint32_t unique_id_len = PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1;
char unique_id[unique_id_len] = "0000000000000000";

void Platform::Preinit() {
    pico_get_unique_board_id_string(unique_id, unique_id_len);

    //TODO: Switch between UART or USB based on defines from build config
    stdio_usb_init();

    // Flag for toggling LED while in startup delay
    bool status = false;
    
    //need to give time for USB init and enumeration on host side
    for (int i = 0; i < 12; ++i) {
        sleep_ms(400);
        status = !status;
        gPlatform.IO.StatusLED(status);
    }
}

void Platform::Init() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
}

const char* PlatformInfo::Model() const { return "RP2040-Visa"; };
const char* PlatformInfo::SerialNum() const { return unique_id; };