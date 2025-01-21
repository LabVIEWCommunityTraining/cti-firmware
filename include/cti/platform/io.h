#ifndef platform_io_h_
#define platform_io_h_

#include <stdio.h>

#include "types.h"

namespace CTI {

typedef int8_t ChanIndex;

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

} // namespace CTI

#endif //platform_io_h_