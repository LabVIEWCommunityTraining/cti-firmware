#ifndef CTI_WS2811_FRAME_H
#define CTI_WS2811_FRAME_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace CTI::WS2811 {

constexpr size_t MaxLeds = 4096;
constexpr size_t MaxChunk = 2000;
constexpr uint32_t ResetUs = 400;
constexpr uint32_t PioClockHz = 16000000;
constexpr size_t ProgramLength = 9;

struct Timing {
    uint32_t zeroHigh = 4, zeroLow = 16, oneHigh = 10, oneLow = 10;
    uint32_t resetUs = ResetUs;
    static uint32_t cycles(uint32_t ns) {
        return static_cast<uint32_t>((static_cast<uint64_t>(ns) * 2 + 62) / 125);
    }
    static uint32_t nanoseconds(uint32_t cycles) { return (cycles * 125 + 1) / 2; }
    bool valid() const {
        return zeroHigh >= 1 && zeroHigh <= 16 &&
            oneHigh > zeroHigh && oneHigh - zeroHigh <= 16 &&
            zeroLow >= 4 && zeroLow <= 49 && oneLow >= 4 && oneLow <= 49 &&
            resetUs >= 50 && resetUs <= 100000;
    }
    uint32_t maxBitCycles() const {
        return zeroHigh + zeroLow > oneHigh + oneLow ?
            zeroHigh + zeroLow : oneHigh + oneLow;
    }
    uint32_t wireUs(size_t pixels) const {
        return static_cast<uint32_t>((static_cast<uint64_t>(pixels) * 24 * maxBitCycles() + 15) / 16);
    }
    void patch(const uint16_t* source, uint16_t* target) const {
        for (size_t i = 0; i < ProgramLength; ++i) target[i] = source[i];
        auto delay = [&](size_t index, uint32_t duration) {
            target[index] = (target[index] & ~0x0f00u) | ((duration - 1) << 8);
        };
        delay(1, zeroHigh);
        delay(2, oneHigh - zeroHigh);
        auto low = [&](size_t index, uint32_t duration) {
            // Three low instructions plus the following OUT form the low interval.
            uint32_t remaining = duration - 1;
            for (size_t part = 0; part < 3; ++part) {
                uint32_t reserve = 2 - part;
                uint32_t length = remaining - reserve > 16 ? 16 : remaining - reserve;
                delay(index + part, length);
                remaining -= length;
            }
        };
        low(3, oneLow);
        low(6, zeroLow);
    }
};

enum class Order { RGB, RBG, GRB, GBR, BRG, BGR };

struct Configuration {
    size_t count = 64;
    unsigned pin = 2;
    Timing timing;
    Order order = Order::RGB;
    size_t frameBytes() const { return count * 3; }
    uint32_t transferUs() const { return timing.wireUs(count); }
    void setRate(unsigned rate) {
        timing = Timing{};
        if (rate == 400000) {
            timing.zeroHigh = 8; timing.zeroLow = 32;
            timing.oneHigh = 20; timing.oneLow = 20;
        }
    }
    uint32_t rate() const { return PioClockHz / timing.maxBitCycles(); }
    bool valid() const {
        return count >= 1 && count <= MaxLeds &&
            (pin <= 22 || (pin >= 26 && pin <= 28)) &&
            timing.valid();
    }
};

struct Buffers {
    explicit Buffers(size_t count):
        capacity(count * 3),
        upload(new (std::nothrow) uint8_t[capacity]{}),
        displayed(new (std::nothrow) uint8_t[capacity]{}),
        wire(new (std::nothrow) uint32_t[count]{}) {}
    ~Buffers() { delete[] upload; delete[] displayed; delete[] wire; }
    Buffers(const Buffers&) = delete;
    Buffers& operator=(const Buffers&) = delete;
    const size_t capacity;
    uint8_t* upload;
    uint8_t* displayed;
    uint32_t* wire;
    size_t received = 0;
    bool valid() const { return upload && displayed && wire; }
    bool write(size_t offset, const uint8_t* data, size_t length) {
        if (!data || !upload || !length || length > MaxChunk ||
            offset > capacity || length > capacity - offset ||
            (offset && offset != received)) return false;
        std::memcpy(upload + offset, data, length);
        received = offset + length;
        return true;
    }
};

inline void pack(const uint8_t* rgb, uint32_t* wire, const Configuration& config, unsigned brightness) {
    constexpr unsigned channels[][3] = {{0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}};
    const unsigned* order = channels[static_cast<unsigned>(config.order)];
    for (size_t pixel = 0; pixel < config.count; ++pixel) {
        uint32_t word = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            unsigned level = (rgb[pixel * 3 + order[channel]] * brightness + 50) / 100;
            word |= level << (24 - channel * 8);
        }
        wire[pixel] = word;
    }
}

}
#endif
