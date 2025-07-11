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

    
}

void Platform::Init() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
}

const char* PlatformInfo::SerialNum() const { return unique_id; };