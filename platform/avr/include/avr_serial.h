#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdio.h>

void initSerial(long baud);

void printDec(const float &val);
void printDec(float *val);
void printDec(volatile float *val);

#endif //SERIAL_H_
