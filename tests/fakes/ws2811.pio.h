#pragma once
#include "fake_sdk.h"
inline pio_program ws2811_program;
inline pio_sm_config ws2811_program_get_default_config(int) { return {}; }
