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

    bool status = false;
    bool usb_init = stdio_usb_init();

    stdio_set_translate_crlf(&stdio_usb, false);

    if (!usb_init) {
        while (1) {
            status = !status;
            gPlatform.IO.StatusLED(status);
            sleep_ms(100);
        }
    }

    for (int i = 0; i < 12; ++i) {
        sleep_ms(400);
        status = !status;
        gPlatform.IO.StatusLED(status);
    }

    setbuf(stdout, NULL);
}

void Platform::Init() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
}

const char* PlatformInfo::Model() const { return "RP2350-Visa"; }
const char* PlatformInfo::SerialNum() const { return unique_id; }
