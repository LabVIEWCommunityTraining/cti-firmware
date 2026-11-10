#ifndef CTI_HUB75_FRAME_H
#define CTI_HUB75_FRAME_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace CTI::Hub75 {

#ifndef CTI_HUB75_WIDTH
#define CTI_HUB75_WIDTH 64
#endif
#ifndef CTI_HUB75_HEIGHT
#define CTI_HUB75_HEIGHT 64
#endif

constexpr size_t Width = CTI_HUB75_WIDTH;
constexpr size_t Height = CTI_HUB75_HEIGHT;
static_assert(Width >= 8 && Width <= 256, "HUB75 width must be 8..256");
static_assert(Height == 16 || Height == 32 || Height == 64, "HUB75 height must be 16, 32 or 64");
constexpr size_t RowPairs = Height / 2;
constexpr size_t FrameBytes = Width * Height * 3;
constexpr size_t MaxChunk = 2000;
constexpr size_t Planes = 4;
constexpr size_t PixelsPerWord = 5;
constexpr size_t PixelWords = (Width + PixelsPerWord - 1) / PixelsPerWord;
constexpr size_t WordsPerRow = PixelWords + 3;
constexpr size_t ScanWords = RowPairs * Planes * WordsPerRow;
constexpr uint32_t OutputDisable = 1u << 13;
constexpr uint32_t Clock = 1u << 11;
constexpr uint32_t Latch = 1u << 12;
// SET PINS, row with side-set 4: row address changes only while /OE is high.
constexpr uint32_t RowSetInstruction = 0xf000;
constexpr uint32_t BaseTicks = 400;
constexpr uint32_t PioClockHz = 20000000;
constexpr uint32_t RowOverheadTicks = Width * 10 + 10;
constexpr uint32_t FrameTicks = RowPairs * (Planes * RowOverheadTicks + BaseTicks * 15);

enum class PixelFormat { RGB888, RGB332 };

struct Configuration {
    size_t width = Width;
    size_t height = Height;
    PixelFormat format = PixelFormat::RGB888;
    uint8_t pins[14] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

    bool validDimensions() const {
        return width >= 8 && width <= 256 && (height == 16 || height == 32 || height == 64);
    }

    bool validPins() const {
        uint32_t used = 0;
        for (size_t i = 0; i < 14; ++i) {
            unsigned pin = pins[i];
            // Pico-family header GPIOs only, never onboard power/LED/wireless pins.
            if (pin > 28 || (pin > 22 && pin < 26) || (used & (1u << pin))) return false;
            used |= 1u << pin;
        }
        for (size_t i = 1; i < 6; ++i) if (pins[i] != pins[0] + i) return false;
        for (size_t i = 7; i < 11; ++i) if (pins[i] != pins[6] + i - 6) return false;
        return pins[12] == pins[11] + 1 && pins[13] == pins[11] + 2;
    }

    uint32_t pinMask() const {
        uint32_t mask = 0;
        for (uint8_t pin : pins) mask |= 1u << pin;
        return mask;
    }

    size_t frameBytes() const { return width * height * (format == PixelFormat::RGB888 ? 3 : 1); }
    size_t rowPairs() const { return height / 2; }
    size_t pixelWords() const { return (width + PixelsPerWord - 1) / PixelsPerWord; }
    size_t wordsPerRow() const { return pixelWords() + 3; }
    size_t scanWords() const { return rowPairs() * Planes * wordsPerRow(); }
    uint32_t frameTicks() const { return rowPairs() * (Planes * (width * 10 + 10) + BaseTicks * 15); }
};

struct Upload {
    explicit Upload(size_t bytes = FrameBytes): rgb(new (std::nothrow) uint8_t[bytes]{}), capacity(bytes) {}
    ~Upload() { delete[] rgb; }
    Upload(const Upload&) = delete;
    Upload& operator=(const Upload&) = delete;
    uint8_t* rgb;
    const size_t capacity;
    size_t received = 0;

    bool write(size_t offset, const uint8_t* data, size_t length) {
        if (!data || length == 0 || length > MaxChunk ||
            !rgb || offset > capacity || length > capacity - offset ||
            (offset != 0 && offset != received)) {
            return false;
        }
        std::memcpy(rgb + offset, data, length);
        received = offset + length;
        return true;
    }

    bool complete() const { return received == capacity; }
};

inline void setRowBrightness(uint32_t* row, unsigned plane, unsigned percent, size_t wordsPerRow = WordsPerRow) {
    uint32_t ticks = BaseTicks * (1u << plane);
    uint32_t on = ticks * percent / 100;
    row[wordsPerRow - 2] = on ? on - 1 : 0;
    row[wordsPerRow - 1] = ticks - on;
}

inline void setBrightness(uint32_t* scan, unsigned percent, const Configuration& config = {}) {
    for (size_t row = 0; row < config.rowPairs(); ++row) {
        for (size_t plane = 0; plane < Planes; ++plane) {
            setRowBrightness(scan + (row * Planes + plane) * config.wordsPerRow(), plane, percent, config.wordsPerRow());
        }
    }
}

inline uint8_t channelLevel(const uint8_t* image, size_t pixel, size_t channel, PixelFormat format) {
    if (format == PixelFormat::RGB888) return image[pixel * 3 + channel] >> 4;
    uint8_t byte = image[pixel];
    // Scale RGB332 levels to the existing 4-bit BCM range, including full black/white.
    if (channel == 0) return (((byte >> 5) & 7) * 15 + 3) / 7;
    if (channel == 1) return (((byte >> 2) & 7) * 15 + 3) / 7;
    return (byte & 3) * 5;
}

inline void pack(const uint8_t* rgb, unsigned percent, uint32_t* scan, const Configuration& config = {}) {
    for (size_t row = 0; row < config.rowPairs(); ++row) {
        for (size_t plane = 0; plane < Planes; ++plane) {
            uint32_t* out = scan + (row * Planes + plane) * config.wordsPerRow();
            out[0] = RowSetInstruction | static_cast<uint32_t>(row) |
                (static_cast<uint32_t>(config.width - 1) << 16);
            std::memset(out + 1, 0, config.pixelWords() * sizeof(uint32_t));
            for (size_t x = 0; x < config.width; ++x) {
                uint32_t color = 0;
                for (size_t half = 0; half < 2; ++half) {
                    for (size_t channel = 0; channel < 3; ++channel) {
                        size_t pixel = (row + half * config.rowPairs()) * config.width + x;
                        color |= ((channelLevel(rgb, pixel, channel, config.format) >> plane) & 1u) << (half * 3 + channel);
                    }
                }
                out[1 + x / PixelsPerWord] |= color << (6 * (x % PixelsPerWord));
            }
            setRowBrightness(out, plane, percent, config.wordsPerRow());
        }
    }
}

} // namespace CTI::Hub75
#endif
