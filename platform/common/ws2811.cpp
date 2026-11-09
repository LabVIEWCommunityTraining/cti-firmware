#include "display/ws2811.h"
#include "display/ws2811_frame.h"
#include "display/hub75.h"
#include "visa/visa_core.h"
#include "ws2811.pio.h"

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/time.h>

namespace CTI::WS2811 {
namespace {
using namespace SCPI;

Configuration config;
Buffers* buffers = nullptr;
PIO pio = nullptr;
int sm = -1, dma = -1, offset = -1;
bool running = false, ownsPin = false;
enum class Phase { Idle, Reset, Sending, Draining };
Phase phase = Phase::Idle;
uint32_t phaseAt = 0, frames = 0;
unsigned brightness = 25;

CommandResult error(ScpiParser* parser, const char* text, int16_t code = -221) {
    parser->enqueueError(code, text);
    return CommandResult::Error;
}

bool configurable(ScpiParser* parser) {
    if (!ownsPin && !buffers) return true;
    error(parser, "STOP WS2811 before changing configuration");
    return false;
}

void submit() {
    pack(buffers->displayed, buffers->wire, config, brightness);
    phase = Phase::Reset;
    phaseAt = time_us_32();
}

CommandResult start(ScpiParser* parser) {
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (running) return CommandResult::Success;
    if (buffers && !stop()) return error(parser, "WS2811 DMA shutdown still pending", -200);
    if (!config.valid()) return error(parser, "Invalid WS2811 configuration", -222);
    if (!Hub75::allowPin(parser, config.pin)) return CommandResult::Error;
    gpio_function_t function = gpio_get_function(config.pin);
    if (function != GPIO_FUNC_NULL &&
        (function != GPIO_FUNC_SIO || gpio_get_dir(config.pin) ||
         gpio_is_pulled_up(config.pin) || gpio_is_pulled_down(config.pin))) {
        return error(parser, "WS2811 data pin is in use; release it first");
    }
    // Keep DIN low through mux changes, allocation failures and shutdown.
    gpio_set_outover(config.pin, GPIO_OVERRIDE_LOW);
    gpio_put(config.pin, false);
    gpio_set_dir(config.pin, GPIO_OUT);
    gpio_set_function(config.pin, GPIO_FUNC_SIO);
    gpio_set_outover(config.pin, GPIO_OVERRIDE_LOW);
    ownsPin = true;
    buffers = new (std::nothrow) Buffers(config.count);
    if (!buffers || !buffers->valid()) {
        stop();
        return error(parser, "WS2811 framebuffer allocation failed", -225);
    }
    PIO candidates[] = {pio0, pio1,
#if !PICO_RP2040
        pio2,
#endif
    };
    for (PIO candidate : candidates) {
        if (!pio_can_add_program(candidate, &ws2811_program)) continue;
        int candidateSm = pio_claim_unused_sm(candidate, false);
        if (candidateSm < 0) continue;
        pio = candidate;
        sm = candidateSm;
        offset = pio_add_program(pio, &ws2811_program);
        break;
    }
    if (sm < 0) {
        stop();
        return error(parser, "No WS2811 PIO state machine/instruction memory available", -225);
    }
    dma = dma_claim_unused_channel(false);
    if (dma < 0) {
        stop();
        return error(parser, "No WS2811 DMA channel available", -225);
    }
    pio_sm_config smConfig = ws2811_program_get_default_config(offset);
    sm_config_set_sideset_pins(&smConfig, config.pin);
    sm_config_set_out_shift(&smConfig, false, true, 24);
    sm_config_set_fifo_join(&smConfig, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&smConfig, static_cast<float>(clock_get_hz(clk_sys)) /
        (config.rate * CyclesPerBit));
    pio_sm_init(pio, sm, offset, &smConfig);
    pio_sm_set_pins_with_mask(pio, sm, 0, 1u << config.pin);
    pio_sm_set_consecutive_pindirs(pio, sm, config.pin, 1, true);
    pio_gpio_init(pio, config.pin);

    dma_channel_config dmaConfig = dma_channel_get_default_config(dma);
    channel_config_set_transfer_data_size(&dmaConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dmaConfig, true);
    channel_config_set_write_increment(&dmaConfig, false);
    channel_config_set_dreq(&dmaConfig, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma, &dmaConfig, &pio->txf[sm], buffers->wire, config.count, false);
    brightness = 25;
    frames = 0;
    running = true;
    pio_sm_set_enabled(pio, sm, true);
    gpio_set_outover(config.pin, GPIO_OVERRIDE_NORMAL);
    submit(); // Initial reset interval, then a black frame.
    return CommandResult::Success;
}

CommandResult count(ScpiParser* parser) {
    uint32_t value;
    if (parser->parseInt(value) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    if (!value || value > MaxLeds) return error(parser, "WS2811 count must be 1..4096", -222);
    config.count = value;
    return CommandResult::Success;
}

QueryResult queryCount(ScpiParser* parser) {
    if (!parser->endOfParameters()) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u\n", static_cast<unsigned>(config.count));
    return QueryResult::Success;
}

CommandResult pin(ScpiParser* parser) {
    uint32_t value;
    if (parser->parseInt(value) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    Configuration next = config;
    next.pin = value;
    if (!next.valid()) return error(parser, "WS2811 requires a header GPIO 0..22 or 26..28", -222);
    config = next;
    return CommandResult::Success;
}

QueryResult queryPin(ScpiParser* parser) {
    if (!parser->endOfParameters()) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u\n", config.pin);
    return QueryResult::Success;
}

CommandResult rate(ScpiParser* parser) {
    uint32_t value;
    if (parser->parseInt(value) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    if (value != 400000 && value != 800000) return error(parser, "WS2811 rate must be 400000 or 800000", -222);
    config.rate = value;
    return CommandResult::Success;
}

QueryResult queryRate(ScpiParser* parser) {
    if (!parser->endOfParameters()) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u\n", config.rate);
    return QueryResult::Success;
}

const ScpiChoice orders[] = {{"RGB",0}, {"RBG",1}, {"GRB",2}, {"GBR",3}, {"BRG",4}, {"BGR",5}, EndScpiChoice};

CommandResult order(ScpiParser* parser) {
    int32_t value;
    if (parser->parseChoice(orders, value) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!configurable(parser)) return CommandResult::Error;
    config.order = static_cast<Order>(value);
    return CommandResult::Success;
}

QueryResult queryOrder(ScpiParser* parser) {
    if (!parser->endOfParameters()) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%s\n", orders[static_cast<unsigned>(config.order)].choiceString);
    return QueryResult::Success;
}

CommandResult data(ScpiParser* parser) {
    uint32_t byteOffset;
    char* bytes;
    int length;
    if (parser->parseInt(byteOffset) != ParseResult::Success ||
        parser->parseBlock(&bytes, &length) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!running) return error(parser, "WS2811 is stopped");
    if (!buffers->write(byteOffset, reinterpret_cast<uint8_t*>(bytes), length))
        return error(parser, "WS2811 requires contiguous 1..2000-byte chunks within count*3 bytes", -222);
    return CommandResult::Success;
}

CommandResult show(ScpiParser* parser) {
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!running) return error(parser, "WS2811 is stopped");
    if (phase != Phase::Idle) return error(parser, "WS2811 transmission/reset pending");
    if (buffers->received != config.frameBytes()) return error(parser, "WS2811 image incomplete; need count*3 RGB bytes");
    std::memcpy(buffers->displayed, buffers->upload, config.frameBytes());
    buffers->received = 0;
    submit();
    return CommandResult::Success;
}

CommandResult level(ScpiParser* parser) {
    uint32_t value;
    if (parser->parseInt(value) != ParseResult::Success) return CommandResult::SyntaxError;
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (value > 100) return error(parser, "WS2811 brightness must be 0..100", -222);
    if (!running) return error(parser, "WS2811 is stopped");
    if (phase != Phase::Idle) return error(parser, "WS2811 transmission/reset pending");
    brightness = value;
    submit();
    return CommandResult::Success;
}

QueryResult status(ScpiParser* parser) {
    if (!parser->endOfParameters()) return QueryResult::UnexpectedParam;
    gPlatform.IO.Printf("%u,%u,%u,%u,%lu\n", running ? 1u : 0u,
        buffers ? static_cast<unsigned>(buffers->received) : 0u,
        phase != Phase::Idle ? 1u : 0u, brightness, static_cast<unsigned long>(frames));
    return QueryResult::Success;
}

CommandResult stopCommand(ScpiParser* parser) {
    if (!parser->endOfParameters()) return CommandResult::UnexpectedParam;
    if (!stop()) return error(parser, "WS2811 DMA shutdown timed out; data low, resources retained", -200);
    return CommandResult::Success;
}
}

bool allowPin(SCPI::ScpiParser* parser, int pin) {
    if (ownsPin && pin == static_cast<int>(config.pin)) {
        parser->enqueueError(-221, "GPIO is owned by WS2811; STOP strip first");
        return false;
    }
    return true;
}

bool stop() {
    if (ownsPin) gpio_set_outover(config.pin, GPIO_OVERRIDE_LOW);
    running = false;
    if (dma >= 0) {
        uint32_t mask = 1u << dma;
        dma_hw->abort = mask;
        uint32_t started = time_us_32();
        while (((dma_hw->abort & mask) || dma_channel_is_busy(dma)) &&
               static_cast<uint32_t>(time_us_32() - started) < 1000) {}
        if ((dma_hw->abort & mask) || dma_channel_is_busy(dma)) {
            if (sm >= 0) pio_sm_set_enabled(pio, sm, false);
            return false;
        }
        dma_channel_unclaim(dma);
    }
    if (sm >= 0) {
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
        pio_remove_program(pio, &ws2811_program, offset);
        pio_sm_unclaim(pio, sm);
    }
    if (ownsPin) {
        gpio_put(config.pin, false);
        gpio_set_dir(config.pin, GPIO_IN);
        gpio_deinit(config.pin);
        gpio_set_outover(config.pin, GPIO_OVERRIDE_NORMAL);
    }
    delete buffers;
    buffers = nullptr;
    pio = nullptr;
    sm = dma = offset = -1;
    ownsPin = false;
    phase = Phase::Idle;
    return true;
}

void service(SCPI::ScpiParser* parser) {
    if (!running) {
        if (buffers) stop();
        return;
    }
    uint32_t now = time_us_32();
    uint32_t elapsed = now - phaseAt;
    if (phase == Phase::Reset && elapsed >= ResetUs) {
        dma_channel_set_read_addr(dma, buffers->wire, false);
        dma_channel_set_trans_count(dma, config.count, true);
        phase = Phase::Sending;
        phaseAt = now;
    } else if (phase == Phase::Sending) {
        uint32_t control = dma_channel_hw_addr(dma)->ctrl_trig;
        if ((control & (DMA_CH0_CTRL_TRIG_READ_ERROR_BITS | DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS)) ||
            (dma_channel_is_busy(dma) && elapsed > config.transferUs() + 100000)) {
            stop();
            parser->enqueueError(-200, "WS2811 DMA failed or stalled; output stopped");
        } else if (!dma_channel_is_busy(dma)) {
            phase = Phase::Draining;
            phaseAt = now;
        }
    } else if (phase == Phase::Draining) {
        // DMA completion precedes up to eight TX FIFO words plus one OSR word.
        uint32_t tailUs = 9 * 24 * (config.rate == 800000 ? 125 : 250) / 100;
        if (elapsed >= tailUs + ResetUs) {
            phase = Phase::Idle;
            ++frames;
        }
    }
}

void registerCommands(Visa::Visa* visa) {
    visa->addCommand("DISPlay:WS2811:COUNt", count, queryCount);
    visa->addCommand("DISPlay:WS2811:PIN", pin, queryPin);
    visa->addCommand("DISPlay:WS2811:RATE", rate, queryRate);
    visa->addCommand("DISPlay:WS2811:ORDer", order, queryOrder);
    visa->addCommand("DISPlay:WS2811:START", start, nullptr);
    visa->addCommand("DISPlay:WS2811:DATA", data, nullptr);
    visa->addCommand("DISPlay:WS2811:SHOW", show, nullptr);
    visa->addCommand("DISPlay:WS2811:BRIGhtness", level, nullptr);
    visa->addCommand("DISPlay:WS2811:STATus", nullptr, status);
    visa->addCommand("DISPlay:WS2811:STOP", stopCommand, nullptr);
}
}
