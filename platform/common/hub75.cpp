#include "display/hub75.h"
#include "display/ws2811.h"
#include "display/hub75_frame.h"
#include "visa/visa_core.h"
#include "hub75.pio.h"

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/sync.h>
#include <pico/time.h>
#include <new>

namespace CTI::Hub75 {
namespace {

using namespace SCPI;

struct Buffers {
    explicit Buffers(const Configuration& config):
        upload(config.frameBytes()),
        scan{new (std::nothrow) uint32_t[config.scanWords()], new (std::nothrow) uint32_t[config.scanWords()]} {}
    ~Buffers() { delete[] scan[0]; delete[] scan[1]; }
    bool valid() const { return upload.rgb && scan[0] && scan[1]; }
    Upload upload;
    uint32_t* scan[2];
};

Configuration config;
Buffers* buffers = nullptr;
PIO pio = nullptr;
int sm = -1;
int dma = -1;
int programOffset = -1;
volatile int active = 0;
volatile int pending = -1;
volatile uint32_t frames = 0;
volatile uint32_t lastFrameUs = 0;
unsigned brightness = 25;
bool running = false;
bool ownsPins = false;
bool irqInstalled = false;
uint32_t stoppedBlankMask = 0;

CommandResult error(ScpiParser* parser, const char* text, int16_t code = -221) {
    parser->enqueueError(code, text);
    return CommandResult::Error;
}

void frameDone() {
    uint32_t mask = 1u << dma;
    if (!(dma_hw->ints0 & mask)) return;
    dma_hw->ints0 = mask;
    if (!running) return;
    if (pending >= 0) {
        active = pending;
        pending = -1;
    }
    frames = frames + 1;
    lastFrameUs = time_us_32();
    dma_channel_set_read_addr(dma, buffers->scan[active], false);
    dma_channel_set_trans_count(dma, config.scanWords(), true);
}

bool pinsFree() {
    for (unsigned pin : config.pins) {
        gpio_function_t function = gpio_get_function(pin);
        if ((stoppedBlankMask & (1u << pin)) && function == GPIO_FUNC_SIO &&
            gpio_get_dir(pin) && gpio_get_out_level(pin)) continue;
        if (function != GPIO_FUNC_NULL &&
            (function != GPIO_FUNC_SIO || gpio_get_dir(pin) ||
             gpio_is_pulled_up(pin) || gpio_is_pulled_down(pin))) return false;
    }
    return true;
}

bool claimPio() {
    PIO candidates[] = {
        pio0, pio1,
#if !PICO_RP2040
        pio2,
#endif
    };
    for (PIO candidate : candidates) {
        if (!pio_can_add_program(candidate, &hub75_program)) continue;
        int candidateSm = pio_claim_unused_sm(candidate, false);
        if (candidateSm < 0) continue;
        pio = candidate;
        sm = candidateSm;
        programOffset = pio_add_program(pio, &hub75_program);
        return true;
    }
    return false;
}

bool hasNoParams(ScpiParser* parser) {
    return parser->endOfParameters();
}

CommandResult init(ScpiParser* parser) {
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (running) return CommandResult::Success;
    if (buffers && !stop()) return error(parser, "HUB75 DMA shutdown still pending", -200);
    if (!config.validDimensions() || !config.validPins()) return error(parser, "Invalid HUB75 configuration", -222);
    for (unsigned pin : config.pins) {
        if (!WS2811::allowPin(parser, pin)) return CommandResult::Error;
    }
    if (!pinsFree()) return error(parser, "HUB75 pins are in use; release GPIO/peripherals first");
    if (irq_get_exclusive_handler(DMA_IRQ_0)) return error(parser, "DMA IRQ0 has an exclusive owner", -225);

    // Override the pad before changing mux or enabling any output driver.
    unsigned oe = config.pins[13];
    gpio_set_outover(oe, GPIO_OVERRIDE_HIGH);
    gpio_put(oe, true);
    gpio_set_dir(oe, GPIO_OUT);
    gpio_set_function(oe, GPIO_FUNC_SIO);
    gpio_set_outover(oe, GPIO_OVERRIDE_HIGH);
    ownsPins = true;
    stoppedBlankMask |= 1u << oe;
    buffers = new (std::nothrow) Buffers(config);
    if (!buffers || !buffers->valid()) {
        stop();
        return error(parser, "HUB75 framebuffer allocation failed", -225);
    }
    if (!claimPio()) {
        stop();
        return error(parser, "No HUB75 PIO state machine/instruction memory available", -225);
    }
    dma = dma_claim_unused_channel(false);
    if (dma < 0) {
        stop();
        return error(parser, "No HUB75 DMA channel available", -225);
    }
    brightness = 25;
    active = 0;
    pending = -1;
    frames = 0;
    pack(buffers->upload.rgb, brightness, buffers->scan[0], config);
    pack(buffers->upload.rgb, brightness, buffers->scan[1], config);

    pio_sm_config smConfig = hub75_program_get_default_config(programOffset);
    sm_config_set_out_pins(&smConfig, config.pins[0], 6);
    sm_config_set_set_pins(&smConfig, config.pins[6], 5);
    sm_config_set_sideset_pins(&smConfig, config.pins[11]);
    sm_config_set_out_shift(&smConfig, true, false, 30);
    sm_config_set_fifo_join(&smConfig, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&smConfig, static_cast<float>(clock_get_hz(clk_sys)) / PioClockHz);
    pio_sm_init(pio, sm, programOffset, &smConfig);
    pio_sm_set_pins_with_mask(pio, sm, 1u << oe, config.pinMask());
    pio_sm_set_consecutive_pindirs(pio, sm, config.pins[0], 6, true);
    pio_sm_set_consecutive_pindirs(pio, sm, config.pins[6], 5, true);
    pio_sm_set_consecutive_pindirs(pio, sm, config.pins[11], 3, true);
    for (unsigned pin : config.pins) {
        pio_gpio_init(pio, pin);
    }

    dma_channel_config dmaConfig = dma_channel_get_default_config(dma);
    channel_config_set_transfer_data_size(&dmaConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dmaConfig, true);
    channel_config_set_write_increment(&dmaConfig, false);
    channel_config_set_dreq(&dmaConfig, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma, &dmaConfig, &pio->txf[sm], buffers->scan[0], config.scanWords(), false);
    dma_hw->ints0 = 1u << dma;
    irq_add_shared_handler(DMA_IRQ_0, frameDone, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irqInstalled = true;
    dma_channel_set_irq0_enabled(dma, true);
    irq_set_enabled(DMA_IRQ_0, true);
    lastFrameUs = time_us_32();
    running = true;
    dma_start_channel_mask(1u << dma);
    gpio_set_outover(oe, GPIO_OVERRIDE_NORMAL);
    pio_sm_set_enabled(pio, sm, true);
    return CommandResult::Success;
}

CommandResult data(ScpiParser* parser) {
    uint32_t offset;
    char* bytes;
    int length;
    if (parser->parseInt(offset) != ParseResult::Success ||
        parser->parseBlock(&bytes, &length) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (!running) return error(parser, "HUB75 is stopped");
    if (!buffers->upload.write(offset, reinterpret_cast<const uint8_t*>(bytes), length)) {
        parser->enqueueError(-222, "HUB75 requires contiguous 1..2000-byte chunks within the configured RGB frame");
        return CommandResult::Error;
    }
    return CommandResult::Success;
}

void queue(int next) {
    __dmb();
    uint32_t saved = save_and_disable_interrupts();
    pending = next;
    restore_interrupts(saved);
}

CommandResult show(ScpiParser* parser) {
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (!running) return error(parser, "HUB75 is stopped");
    if (pending >= 0) return error(parser, "HUB75 frame swap pending");
    if (!buffers->upload.complete()) return error(parser, "HUB75 image incomplete for configured dimensions/format");
    int next = 1 - active;
    pack(buffers->upload.rgb, brightness, buffers->scan[next], config);
    buffers->upload.received = 0;
    queue(next);
    return CommandResult::Success;
}

CommandResult setLevel(ScpiParser* parser) {
    uint32_t percent;
    if (parser->parseInt(percent) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (percent > 100) return error(parser, "HUB75 brightness must be 0..100 percent", -222);
    if (!running) return error(parser, "HUB75 is stopped");
    if (pending >= 0) return error(parser, "HUB75 frame swap pending");
    int next = 1 - active;
    std::memcpy(buffers->scan[next], buffers->scan[active], config.scanWords() * sizeof(uint32_t));
    setBrightness(buffers->scan[next], percent, config);
    brightness = percent;
    queue(next);
    return CommandResult::Success;
}

QueryResult status(ScpiParser* parser) {
    if (!hasNoParams(parser)) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u,%u,%u,%u,%lu\n", running ? 1u : 0u,
        buffers ? static_cast<unsigned>(buffers->upload.received) : 0u,
        pending >= 0 ? 1u : 0u, brightness, static_cast<unsigned long>(frames));
    return QueryResult::Success;
}

QueryResult geometry(ScpiParser* parser) {
    if (!hasNoParams(parser)) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u,%u,%u,%u\n", static_cast<unsigned>(config.width), static_cast<unsigned>(config.height),
        static_cast<unsigned>(config.frameBytes()), static_cast<unsigned>(Planes));
    return QueryResult::Success;
}

bool configurable(ScpiParser* parser) {
    if (ownsPins || buffers) {
        error(parser, "STOP HUB75 before changing configuration");
        return false;
    }
    return true;
}

CommandResult dimensions(ScpiParser* parser) {
    uint32_t width, height;
    if (parser->parseInt(width) != ParseResult::Success ||
        parser->parseInt(height) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    Configuration next = config;
    next.width = width;
    next.height = height;
    if (!next.validDimensions()) return error(parser, "HUB75 width must be 8..256; height 16,32,64", -222);
    config = next;
    return CommandResult::Success;
}

QueryResult queryDimensions(ScpiParser* parser) {
    if (!hasNoParams(parser)) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u,%u\n", static_cast<unsigned>(config.width), static_cast<unsigned>(config.height));
    return QueryResult::Success;
}

CommandResult pins(ScpiParser* parser) {
    Configuration next = config;
    for (uint8_t& pin : next.pins) {
        uint32_t value;
        if (parser->parseInt(value) != ParseResult::Success) return CommandResult::SyntaxError;
        if (value > 28) return error(parser, "HUB75 pin is not a Pico header GPIO", -222);
        pin = static_cast<uint8_t>(value);
    }
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    if (!next.validPins()) return error(parser, "HUB75 requires distinct header pins in consecutive RGB(6), address(5), control(3) groups", -222);
    config = next;
    return CommandResult::Success;
}

QueryResult queryPins(ScpiParser* parser) {
    if (!hasNoParams(parser)) return QueryResult::UnexpectedParam;
    for (size_t i = 0; i < 14; ++i) {
        gPlatform.IO.Printf("%u%c", static_cast<unsigned>(config.pins[i]), i == 13 ? '\n' : ',');
    }
    return QueryResult::Success;
}

ScpiChoice formats[] = {{"RGB888", 0}, {"RGB332", 1}, EndScpiChoice};

CommandResult format(ScpiParser* parser) {
    int32_t choice;
    if (parser->parseChoice(formats, choice) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    config.format = choice == 0 ? PixelFormat::RGB888 : PixelFormat::RGB332;
    return CommandResult::Success;
}

QueryResult queryFormat(ScpiParser* parser) {
    if (!hasNoParams(parser)) return QueryResult::UnexpectedParam;
    gPlatform.IO.Print(config.format == PixelFormat::RGB888 ? "RGB888\n" : "RGB332\n");
    return QueryResult::Success;
}

CommandResult stopCommand(ScpiParser* parser) {
    if (!hasNoParams(parser)) return CommandResult::UnexpectedParam;
    if (!stop()) return error(parser, "HUB75 DMA shutdown timed out; display blanked, resources retained", -200);
    return CommandResult::Success;
}

} // namespace

bool stop() {
    unsigned oe = config.pins[13];
    if (ownsPins) gpio_set_outover(oe, GPIO_OVERRIDE_HIGH);
    uint32_t saved = save_and_disable_interrupts();
    running = false;
    if (dma >= 0) {
        uint32_t mask = 1u << dma;
        dma_channel_set_irq0_enabled(dma, false);
        dma_hw->abort = mask;
        uint32_t started = time_us_32();
        // ABORT completion, not just BUSY, guarantees no in-flight memory access.
        while (((dma_hw->abort & mask) || dma_channel_is_busy(dma)) &&
               static_cast<uint32_t>(time_us_32() - started) < 1000) {}
        if ((dma_hw->abort & mask) || dma_channel_is_busy(dma)) {
            if (sm >= 0) pio_sm_set_enabled(pio, sm, false);
            restore_interrupts(saved);
            return false;
        }
        dma_hw->ints0 = 1u << dma;
    }
    if (sm >= 0) {
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
    }
    if (irqInstalled) {
        irq_remove_handler(DMA_IRQ_0, frameDone);
        irqInstalled = false;
    }
    restore_interrupts(saved);
    if (dma >= 0) dma_channel_unclaim(dma);
    if (programOffset >= 0) pio_remove_program(pio, &hub75_program, programOffset);
    if (sm >= 0) pio_sm_unclaim(pio, sm);
    if (ownsPins) {
        for (size_t i = 0; i < 13; ++i) {
            unsigned pin = config.pins[i];
            gpio_set_dir(pin, GPIO_IN);
            gpio_deinit(pin);
            stoppedBlankMask &= ~(1u << pin);
        }
        gpio_put(oe, true);
        gpio_set_dir(oe, GPIO_OUT);
        gpio_set_function(oe, GPIO_FUNC_SIO);
        gpio_set_outover(oe, GPIO_OVERRIDE_NORMAL);
    }
    delete buffers;
    buffers = nullptr;
    pio = nullptr;
    sm = dma = programOffset = -1;
    ownsPins = false;
    pending = -1;
    return true;
}

bool allowPin(SCPI::ScpiParser* parser, int pin) {
    if (pin < 0 || pin > 29) {
        parser->enqueueError(-222, "GPIO must be in range 0..29");
        return false;
    }
    if (!WS2811::allowPin(parser, pin)) return false;
    if (ownsPins && (config.pinMask() & (1u << pin))) {
        parser->enqueueError(-221, "GPIO is owned by HUB75; STOP display first");
        return false;
    }
    stoppedBlankMask &= ~(1u << pin);
    return true;
}

bool allowAnalog(SCPI::ScpiParser* parser, int channel) {
#if PICO_RP2040
    constexpr int maxChannel = 3;
#else
    constexpr int maxChannel = 2;
#endif
    if (channel < 0 || channel > maxChannel) {
        parser->enqueueError(-222, "ADC input channel is out of range");
        return false;
    }
    return allowPin(parser, channel + 26);
}

void service(SCPI::ScpiParser* parser) {
    if (!running) {
        if (buffers) stop();
        return;
    }
    uint32_t control = dma_channel_hw_addr(dma)->ctrl_trig;
    uint32_t saved = save_and_disable_interrupts();
    uint32_t elapsed = time_us_32() - lastFrameUs;
    restore_interrupts(saved);
    if ((control & (DMA_CH0_CTRL_TRIG_READ_ERROR_BITS | DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS)) ||
        elapsed > 100000) {
        stop();
        parser->enqueueError(-200, "HUB75 refresh stalled or DMA failed; display stopped and blanked");
    }
}

void registerCommands(Visa::Visa* visa) {
    visa->addCommand("DISPlay:HUB75:INIT", init, nullptr);
    visa->addCommand("DISPlay:HUB75:START", init, nullptr);
    visa->addCommand("DISPlay:HUB75:DIMensions", dimensions, queryDimensions);
    visa->addCommand("DISPlay:HUB75:PINS", pins, queryPins);
    visa->addCommand("DISPlay:HUB75:FORMat", format, queryFormat);
    visa->addCommand("DISPlay:HUB75:DATA", data, nullptr);
    visa->addCommand("DISPlay:HUB75:SHOW", show, nullptr);
    visa->addCommand("DISPlay:HUB75:BRIGhtness", setLevel, nullptr);
    visa->addCommand("DISPlay:HUB75:STATus", nullptr, status);
    visa->addCommand("DISPlay:HUB75:GEOMetry", nullptr, geometry);
    visa->addCommand("DISPlay:HUB75:STOP", stopCommand, nullptr);
}

} // namespace CTI::Hub75
