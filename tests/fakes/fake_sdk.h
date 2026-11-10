#pragma once
#include <cstdint>
#include <cassert>

using uint = unsigned;
enum gpio_function_t { GPIO_FUNC_NULL, GPIO_FUNC_SIO, GPIO_FUNC_PIO0, GPIO_FUNC_UART };
constexpr int GPIO_OVERRIDE_HIGH = 1, GPIO_OVERRIDE_NORMAL = 0, GPIO_OVERRIDE_LOW = 2;
constexpr bool GPIO_OUT = true, GPIO_IN = false;
constexpr int DMA_IRQ_0 = 0, DMA_SIZE_32 = 2;
constexpr int PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY = 0;
constexpr uint32_t DMA_CH0_CTRL_TRIG_READ_ERROR_BITS = 1u << 30;
constexpr uint32_t DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS = 1u << 29;
struct pio_sm_config {
    int outBase = -1, outCount = -1, setBase = -1, setCount = -1;
    int sideBase = -1, shiftThreshold = -1;
    bool shiftRight = false, autoPull = true;
    float clockDivider = 0;
};
struct dma_channel_config {};
struct pio_program { const uint16_t* instructions = nullptr; uint8_t length = 0; };
struct FakePio { uint32_t txf[4] = {}; };
struct FakeDmaChannel { uint32_t ctrl_trig = 0; };

namespace Fake {
inline gpio_function_t functions[30] = {};
inline bool dirs[30] = {}, outputs[30] = {}, pullup[30] = {}, pulldown[30] = {};
inline int overrides[30] = {};
inline int sms = 0, programs = 0, channels = 0;
inline bool pioAvailable = true, dmaAvailable = true, busy = false, stuck = false;
inline bool pioEnabled = false, irqEnabled = false;
inline bool exclusiveIrq = false;
inline uint32_t now = 0, smPins = 0;
inline const void* source = nullptr;
inline unsigned transfers = 0;
inline void (*handler)() = nullptr;
inline FakePio pios[3];
inline unsigned claimedSms[3] = {}, enabledSms[3] = {};
inline uint16_t loadedInstructions[32] = {};
inline FakeDmaChannel channel;
struct DmaState {
    bool busy = false;
    const void* source = nullptr;
    unsigned transfers = 0;
    FakeDmaChannel channel;
};
inline DmaState secondaryDma[12];
inline uint32_t allocatedDma = 0;
inline pio_sm_config config;
inline bool oeHigh() {
    return overrides[13] == GPIO_OVERRIDE_HIGH ||
        (functions[13] == GPIO_FUNC_PIO0 ? (smPins & (1u << 13)) != 0 : outputs[13]);
}
}

struct FakeAck {
    uint32_t bits = 0;
    operator uint32_t() const { return bits; }
    void operator=(uint32_t mask) { bits &= ~mask; }
};
struct FakeAbort {
    uint32_t bits = 0;
    operator uint32_t() const { return bits; }
    void operator=(uint32_t mask) {
        bits = Fake::stuck ? mask : 0;
        if (!Fake::stuck) {
            if (mask & 1) Fake::busy = false;
            for (unsigned i = 1; i < 12; ++i)
                if (mask & (1u << i)) Fake::secondaryDma[i].busy = false;
        }
    }
};
struct FakeDma { FakeAck ints0; FakeAbort abort; };
inline FakeDma dmaInstance;
inline FakeDma* dma_hw = &dmaInstance;
using PIO = FakePio*;
inline PIO pio0 = &Fake::pios[0], pio1 = &Fake::pios[1];
#if !PICO_RP2040
inline PIO pio2 = &Fake::pios[2];
#endif

inline gpio_function_t gpio_get_function(uint pin) { return Fake::functions[pin]; }
inline bool gpio_get_dir(uint pin) { return Fake::dirs[pin]; }
inline bool gpio_get_out_level(uint pin) { return Fake::outputs[pin]; }
inline bool gpio_is_pulled_up(uint pin) { return Fake::pullup[pin]; }
inline bool gpio_is_pulled_down(uint pin) { return Fake::pulldown[pin]; }
inline void gpio_set_function(uint pin, gpio_function_t function) {
    Fake::functions[pin] = function;
    Fake::overrides[pin] = GPIO_OVERRIDE_NORMAL;
}
inline void gpio_set_outover(uint pin, int value) { Fake::overrides[pin] = value; }
inline void gpio_put(uint pin, bool value) { Fake::outputs[pin] = value; }
inline void gpio_set_dir(uint pin, bool value) { Fake::dirs[pin] = value; }
inline void gpio_deinit(uint pin) { gpio_set_function(pin, GPIO_FUNC_NULL); }
inline bool pio_can_add_program(PIO, const pio_program*) { return Fake::pioAvailable; }
inline int pio_claim_unused_sm(PIO pio, bool) {
    if (!Fake::pioAvailable) return -1;
    for (unsigned sm = 0; sm < 4; ++sm) {
        unsigned& mask = Fake::claimedSms[pio - Fake::pios];
        if (mask & (1u << sm)) continue;
        mask |= 1u << sm;
        ++Fake::sms;
        return sm;
    }
    return -1;
}
inline int pio_add_program(PIO, const pio_program* program) {
    for (unsigned i = 0; i < program->length; ++i) Fake::loadedInstructions[i] = program->instructions[i];
    ++Fake::programs;
    return 3;
}
inline void pio_remove_program(PIO, const pio_program*, int) { --Fake::programs; }
inline void pio_sm_unclaim(PIO pio, int sm) {
    unsigned& mask = Fake::claimedSms[pio - Fake::pios];
    assert(mask & (1u << sm));
    mask &= ~(1u << sm);
    --Fake::sms;
}
inline void sm_config_set_out_pins(pio_sm_config* config, int base, int count) {
    config->outBase = base;
    config->outCount = count;
}
inline void sm_config_set_set_pins(pio_sm_config* config, int base, int count) {
    config->setBase = base;
    config->setCount = count;
}
inline void sm_config_set_sideset_pins(pio_sm_config* config, int base) { config->sideBase = base; }
inline void sm_config_set_out_shift(pio_sm_config* config, bool right, bool automatic, int threshold) {
    config->shiftRight = right;
    config->autoPull = automatic;
    config->shiftThreshold = threshold;
}
constexpr int PIO_FIFO_JOIN_TX = 1;
inline void sm_config_set_fifo_join(pio_sm_config*, int) {}
inline void sm_config_set_clkdiv(pio_sm_config* config, float divider) { config->clockDivider = divider; }
inline void pio_sm_init(PIO, int, int, const pio_sm_config* config) { Fake::config = *config; }
inline void pio_sm_set_pins_with_mask(PIO, int, uint32_t pins, uint32_t) { Fake::smPins = pins; }
inline void pio_sm_set_consecutive_pindirs(PIO, int, int, int, bool) {}
inline void pio_gpio_init(PIO, uint pin) { gpio_set_function(pin, GPIO_FUNC_PIO0); }
inline void pio_sm_set_enabled(PIO pio, int sm, bool enabled) {
    Fake::pioEnabled = enabled;
    unsigned& mask = Fake::enabledSms[pio - Fake::pios];
    if (enabled) mask |= 1u << sm;
    else mask &= ~(1u << sm);
}
inline void pio_sm_clear_fifos(PIO, int) {}
inline int pio_get_dreq(PIO, int, bool) { return 0; }
inline int dma_claim_unused_channel(bool) {
    if (!Fake::dmaAvailable) return -1;
    for (unsigned i = 0; i < 12; ++i) {
        if (Fake::allocatedDma & (1u << i)) continue;
        Fake::allocatedDma |= 1u << i;
        ++Fake::channels;
        return i;
    }
    return -1;
}
inline void dma_channel_unclaim(int channel) {
    assert(Fake::allocatedDma & (1u << channel));
    Fake::allocatedDma &= ~(1u << channel);
    --Fake::channels;
}
inline dma_channel_config dma_channel_get_default_config(int) { return {}; }
inline void channel_config_set_transfer_data_size(dma_channel_config*, int) {}
inline void channel_config_set_read_increment(dma_channel_config*, bool) {}
inline void channel_config_set_write_increment(dma_channel_config*, bool) {}
inline void channel_config_set_dreq(dma_channel_config*, int) {}
inline void dma_channel_configure(int channel, const dma_channel_config*, void*, const void* source, unsigned count, bool) {
    if (channel == 0) { Fake::source = source; Fake::transfers = count; }
    else { Fake::secondaryDma[channel].source = source; Fake::secondaryDma[channel].transfers = count; }
}
inline void dma_channel_set_read_addr(int channel, const void* source, bool) {
    if (channel == 0) Fake::source = source;
    else Fake::secondaryDma[channel].source = source;
}
inline void dma_channel_set_trans_count(int channel, unsigned count, bool start) {
    if (channel == 0) { Fake::transfers = count; Fake::busy = start; }
    else { Fake::secondaryDma[channel].transfers = count; Fake::secondaryDma[channel].busy = start; }
}
inline void dma_channel_set_irq0_enabled(int, bool enabled) { Fake::irqEnabled = enabled; }
inline void dma_start_channel_mask(uint32_t) { Fake::busy = true; }
inline bool dma_channel_is_busy(int channel) {
    return channel == 0 ? Fake::busy : Fake::secondaryDma[channel].busy;
}
inline FakeDmaChannel* dma_channel_hw_addr(int channel) {
    return channel == 0 ? &Fake::channel : &Fake::secondaryDma[channel].channel;
}
inline void irq_add_shared_handler(int, void (*handler)(), int) { assert(!Fake::handler); Fake::handler = handler; }
inline void irq_remove_handler(int, void (*)()) { Fake::handler = nullptr; }
inline void irq_set_enabled(int, bool) {}
inline void (*irq_get_exclusive_handler(int))() { return Fake::exclusiveIrq ? +[] {} : nullptr; }
inline uint32_t save_and_disable_interrupts() { return 0; }
inline void restore_interrupts(uint32_t) {}
inline void __dmb() {}
constexpr int clk_sys = 0;
inline uint32_t clock_get_hz(int) {
#if PICO_RP2040
    return 125000000;
#else
    return 150000000;
#endif
}
inline uint32_t time_us_32() { return Fake::now++; }

namespace Fake {
inline void completeFrame() {
    assert(busy && handler && irqEnabled);
    busy = false;
    dma_hw->ints0.bits |= 1;
    handler();
}
}
