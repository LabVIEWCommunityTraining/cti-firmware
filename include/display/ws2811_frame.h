#ifndef CTI_WS2811_FRAME_H
#define CTI_WS2811_FRAME_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace CTI::WS2811 {

constexpr size_t MaxLeds = 4096;
constexpr size_t MaxChunk = 2000;
constexpr uint32_t ResetUs = 300;
constexpr uint32_t CyclesPerBit = 20;

enum class Order { RGB, RBG, GRB, GBR, BRG, BGR };

struct Configuration {
    size_t count = 64;
    unsigned pin = 2;
    unsigned rate = 800000;
    Order order = Order::RGB;
    size_t frameBytes() const { return count * 3; }
    uint32_t transferUs() const { return static_cast<uint32_t>(count * 24 * (rate == 800000 ? 125 : 250) / 100); }
    bool valid() const {
        return count >= 1 && count <= MaxLeds &&
            (pin <= 22 || (pin >= 26 && pin <= 28)) &&
            (rate == 400000 || rate == 800000);
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
