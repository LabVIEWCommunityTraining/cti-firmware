#include "cti/platform.h"
#include "avr_serial.h"
#include "avr_timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdlib.h>

namespace {
char unavailableSerial[] = "UNAVAILABLE";
}

void* operator new(size_t size) {
    return malloc(size);
}

void* operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void* pointer) {
    free(pointer);
}

void operator delete[](void* pointer) {
    free(pointer);
}

void operator delete(void* pointer, size_t) {
    free(pointer);
}

void operator delete[](void* pointer, size_t) {
    free(pointer);
}

void CTI::Platform::Preinit() {
    initTimer();
    initSerial(115200);
    sei();
}

void CTI::Platform::Init() {
    ADMUX = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

void CTI::Platform::Shutdown() {
    cli();
}

const char* CTI::PlatformInfo::Model() const {
    return "Arduino-Uno-ATmega328P";
}

const char* CTI::PlatformInfo::SerialNum() const {
    return unavailableSerial;
}

void* CTI::PlatformMemory::Malloc(size_t count) {
    void* result = malloc(count);
    if (result) {
        _totalAllocated += count;
    }
    return result;
}

void* CTI::PlatformMemory::Realloc(void* buffer, size_t count) {
    return realloc(buffer, count);
}

void CTI::PlatformMemory::Free(void* buffer) {
    free(buffer);
}
