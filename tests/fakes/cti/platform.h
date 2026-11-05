#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace CTI {
using ChanIndex = int8_t;
struct PlatformIO {
    std::string output;
    void Print(char c) { output += c; }
    void Print(const char* text) { output += text; }
    void Print(int len, const char* text) { output.append(text, len); }
    template <typename... Args>
    void Printf(const char* format, Args... args) {
        char text[512];
        std::snprintf(text, sizeof(text), format, args...);
        output += text;
    }
};
struct Platform { PlatformIO IO; };
extern Platform gPlatform;
}
