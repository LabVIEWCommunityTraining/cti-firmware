#pragma once
#include "fake_sdk.h"
constexpr int ws2811_wrap = 8;
inline constexpr uint16_t ws2811_program_instructions[] = {
    0x6021, 0x1326, 0xb542, 0xa642, 0xa042, 0x0000, 0xac42, 0xa042, 0x0000
};
inline pio_program ws2811_program{ws2811_program_instructions, 9};
inline pio_sm_config ws2811_program_get_default_config(int) { return {}; }
