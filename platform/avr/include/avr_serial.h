#ifndef CTI_AVR_SERIAL_H
#define CTI_AVR_SERIAL_H

#include <stdint.h>

void initSerial(uint32_t baud);
int avrSerialGetcharTimeout(uint32_t timeoutUs);

#endif
