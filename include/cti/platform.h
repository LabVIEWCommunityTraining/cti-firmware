#ifndef platform_h_
#define platform_h_

#include "version.h"

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

class PlatformDigital {
public:
    typedef enum { None, Up, Down, Both } PullDirection;

    void SetOutput(int channel, bool value);
    void SetDirection(int channel, bool output);
    void SetPull(int channel, PullDirection dir);

    void GetValue(int channel, bool* value);
    void GetDirection(int channel, bool* output);
    void GetPull(int channel, PullDirection* dir);
};

class PlatformAnalog {
public:

    void EnableInput(int channel);
    void GetInput(int channel, uint16_t* value);
};

typedef enum {
    Comms,
    User
} StatusSource;

class PlatformPWM {
public:
    bool InitPWM(int gpio, bool phaseCorrect, bool enable);
    bool SetDuty(int gpio, float dutyPercent);
    bool SetFreq(int gpio, float freq);
    void SetEnable(int gpio, bool enable);

    bool SetTop(int gpio, uint16_t top);
    bool SetDivider(int gpio, float divider);

    float GetDuty(int gpio);
    float GetFreq(int gpio);
};

class PlatformIO {
public:
    PlatformIO();
    void Print(int32_t len, const char* str);
    void Print(const char* str);
    void Print(char c);
    void Printf(const char* format, ...) const;
    void Flush();

    int FGetCtimeout(uint32_t timeout_us) { if (!timeout_us) { return FGetC(); } else { return _getchar_timeout_us(timeout_us);}}
    int FGetC() { return fgetc(stdin); };
    int FPutC(int c) { return fputc(c, stdout); };
    void ReadStdin(char** buffer, size_t maxLen);

    void InitStatusLED();
    void StatusLED(bool val, StatusSource source = User);
    void SetStatusSource(StatusSource source) { _statusSource = source; };
    StatusSource GetStatusSource() { return _statusSource; };

    PlatformDigital Digital;
    PlatformAnalog Analog;
    PlatformPWM PWM;

private:
    char _fgetc(FILE *file);
    int _getchar_timeout_us(uint32_t timeout_us);
    void _statusLED(bool val);

    StatusSource _statusSource;
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
    static void Setup();
    static void Shutdown();

    PlatformInfo Info;
    PlatformMemory Mem;
    PlatformIO IO;
    PlatformTimer Timer;
    PlatformEngine* pEngine;

private:
    void Init();
};

extern Platform gPlatform;

}; //namespace CTI

#endif //platform_h_