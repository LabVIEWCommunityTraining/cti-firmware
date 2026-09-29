#include "cti/platform.h"
#include "cti_board.h"

#include <driver/gpio.h>

void CTI::Platform::BoardInit() {
    gpio_reset_pin(static_cast<gpio_num_t>(CTI_BOARD_STATUS_LED_GPIO));
    gpio_set_direction(static_cast<gpio_num_t>(CTI_BOARD_STATUS_LED_GPIO), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(CTI_BOARD_STATUS_LED_GPIO), 0);
}
