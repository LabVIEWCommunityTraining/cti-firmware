#ifndef platform_h_
#define platform_h_

#include "version.h"

#include "platform/types.h"
#include "platform/io.h"
#include "platform/comms.h"

#ifdef CTI_FEATURE_WIFI
#include "platform/wifi.h"
#endif

#include <stdint.h>
#include <stdio.h>

#define CTI_VENDOR "CTI"

namespace CTI {

#ifndef PlatformTickType
    typedef int64_t platform_tick_t;
#else
    typedef PlatformTickType platform_tick_t;
#endif

class PlatformInfo {    
public:
    PlatformInfo();
    const char* Vendor() const;
    const char* SerialNum() const;
    const char* Model() const;
    const char* Version() const;
};

class PlatformMemory {
public:
    void* Malloc(size_t count);
    void* Realloc(void* buffer, size_t count);
    void Free(void* buffer);

    size_t TotalAllocated() const { return _totalAllocated; }

private:
    size_t _totalAllocated = 0;
};


class PlatformEngine {
public:
    virtual int Ready() = 0;
    virtual void MainLoop() = 0;
    virtual const char* StatusText(int status) { return ""; };
};

class PlatformTimer {
public:
    static platform_tick_t TickCount();
    static platform_tick_t MicrosecondsToTickCount(int64_t microseconds);
    static int64_t TickCountToMilliseconds(platform_tick_t ticks);
    static int64_t TickCountToMicroseconds(platform_tick_t ticks);
    static platform_tick_t MillisecondsFromNowToTickCount(int64_t milliseconds);
    static platform_tick_t MicrosecondsFromNowToTickCount(int64_t microseconds);
    static void SleepMilliseconds(int64_t milliseconds);
};

class Platform {
public:
    static void Preinit();
    static void BoardInit();
    static void Setup();
    static void Shutdown();

    PlatformInfo Info;
    PlatformMemory Mem;
    PlatformIO IO;
    PlatformTimer Timer;
    PlatformUART UART;
    PlatformI2C I2C;
    PlatformSPI SPI;

#ifdef CTI_FEATURE_WIFI
    PlatformWifi WIFI;
#endif

    PlatformEngine* pEngine;

private:
    void Init();
};

extern Platform gPlatform;

}; //namespace CTI

#endif //platform_h_