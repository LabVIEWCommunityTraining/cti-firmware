#ifndef platform_io_h_
#define platform_io_h_

#include <stdio.h>
#include <stdarg.h>

#include "types.h"


// Defines the number of implementations for how many ways there are to
// communicate with the firmware from a host device. An example could be
// USB STDIO and WiFi TCP for a count of 2
#ifndef CTI_NUM_STREAMS
#define CTI_NUM_STREAMS 1
#endif

#if CTI_FEATURE_DEBUG==1
#define CTI_DEBUG(...) CTI::gPlatform.IO.DPrintF(__VA_ARGS__)
#else
#define CTI_DEBUG(...) (void)0
#endif

#ifndef CTI_DEBUG_STREAM
#define CTI_DEBUG_STREAM 0
#endif

namespace CTI {

typedef int8_t ChanIndex;

typedef int32_t (*vprintf_impl)(const char* fmt, va_list args);
typedef void (*printchar_impl)(char c);
typedef void (*printstr_impl)(const char* str);
typedef void (*printnstr_impl)(int32_t len, const char* str);
typedef int32_t (*getchar_impl)(uint32_t timeout);
typedef void (*flush_impl)();

typedef struct {
    vprintf_impl vprintf;
    printchar_impl printchar;
    printstr_impl printstr;
    printnstr_impl printnstr;
    getchar_impl getchar;
    flush_impl flush;
} StreamIOImpl;

class PlatformDigital {
public:
    typedef enum { None, Up, Down, Both } PullDirection;

    void SetOutput(ChanIndex channel, bool value);
    void SetDirection(ChanIndex channel, bool output);
    void SetPull(ChanIndex channel, PullDirection dir);

    void GetValue(ChanIndex channel, bool* value);
    void GetDirection(ChanIndex channel, bool* output);
    void GetPull(ChanIndex channel, PullDirection* dir);

    LVBlock* Available();
};

class PlatformAnalog {
public:

    void EnableInput(ChanIndex channel);
    void GetInput(ChanIndex channel, uint16_t* value);

    LVBlock* Available();
};

class PlatformPWM {
public:
    bool InitPWM(ChanIndex gpio, bool phaseCorrect, bool enable);
    bool SetDuty(ChanIndex gpio, float dutyPercent);
    bool SetFreq(ChanIndex gpio, float freq);
    void SetEnable(ChanIndex gpio, bool enable);

    bool SetTop(ChanIndex gpio, uint16_t top);
    bool SetDivider(ChanIndex gpio, float divider);

    float GetDuty(ChanIndex gpio);
    float GetFreq(ChanIndex gpio);

    LVBlock* Available();
};

typedef enum {
    Comms,
    User
} StatusSource;

class PlatformIO {
public:
    PlatformIO();
    void Print(int32_t len, const char* str) { _streams[_curStream]->printnstr(len, str); }
    void Print(const char* str) { _streams[_curStream]->printstr(str); }
    void Print(char c) { _streams[_curStream]->printchar(c); }
    void Printf(const char* format, ...) const {
        va_list args;
        va_start(args, format);
        _streams[_curStream]->vprintf(format, args);
        va_end(args);
    }

    void DPrintF(const char* format, ...) const {
        va_list args;
        va_start(args, format);
        _streams[CTI_DEBUG_STREAM]->vprintf(format, args);
        va_end(args);
    }

    void Flush() { _streams[_curStream]->flush(); }

    int FGetCtimeout(uint32_t timeout_us, bool allStreams);

    int CurStream() const { return _curStream; }

    void InitStatusLED();
    void StatusLED(bool val, StatusSource source = User);
    void SetStatusSource(StatusSource source) { _statusSource = source; };
    StatusSource GetStatusSource() { return _statusSource; };

    void RegisterStreams(StreamIOImpl *streams[CTI_NUM_STREAMS]);

    PlatformDigital Digital;
    PlatformAnalog Analog;
    PlatformPWM PWM;

private:
    //char _fgetc(FILE *file);
    //int _getchar_timeout_us(uint32_t timeout_us);
    void _statusLED(bool val);

    StatusSource _statusSource;

    // Array of the stream IO implementations to cycle through when checking for
    // communication from a host system.
    StreamIOImpl **_streams;
    int _curStream;
};

} // namespace CTI

#endif //platform_io_h_