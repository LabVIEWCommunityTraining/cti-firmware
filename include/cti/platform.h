#ifndef platform_h_
#define platform_h_

#include <cstdint>
#include <cstdio>

#ifndef PlatformTickType
    typedef int64_t platform_tick_t;
#else
    typedef PlatformTickType platform_tick_t;
#endif



class PlatformMemory {
public:
    void* Malloc(size_t count);
    void* Realloc(void* buffer, size_t count);
    void Free(void* buffer);

    size_t TotalAllocated() const { return _totalAllocated; }

private:
    size_t _totalAllocated = 0;
};

class PlatformIO {
    PlatformIO();
    void Print(int32_t len, const char* str);
    void Print(const char* str);
    void Print(char c);
    void Printf(const char* format, ...) const;
    void ReadStdin(char** buffer, size_t maxLen);
    void InitStatusLED();
    void StatusLED(bool val);

private:
    char _fgetc(FILE *file);
    int _getchar_timeout_us(uint32_t timeout_us);
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
    static void Setup();
    static void Shutdown();

    PlatformMemory Mem;
    PlatformIO IO;
    PlatformTimer Timer;
};

extern Platform gPlatform;

#endif //platform_h_