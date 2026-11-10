#include "display/ws2811.h"
#include "display/ws2811_frame.h"
#include "display/hub75.h"
#include "visa/visa_core.h"
#include "fake_sdk.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace Fake { extern int allocationsBeforeFailure; }

namespace {
using namespace CTI;
using namespace CTI::SCPI;

void send(ScpiParser& parser, const std::string& message) {
    for (size_t i = 0; i < message.size(); i += 17) {
        int length = static_cast<int>(std::min<size_t>(17, message.size() - i));
        assert(parser.bufferInput(message.data() + i, length) == length);
    }
}

int pop(ScpiParser& parser) {
    int16_t code;
    char* text;
    parser.dequeueError(&code, &text);
    return code;
}

std::string block(size_t offset, const uint8_t* bytes, size_t length) {
    std::string digits = std::to_string(length);
    return "DATA " + std::to_string(offset) + ",#" + std::to_string(digits.size()) + digits +
        std::string(reinterpret_cast<const char*>(bytes), length);
}

void beginTransfer(ScpiParser& parser) {
    Fake::now += CTI::WS2811::ResetUs;
    CTI::WS2811::service(&parser);
}

void completeTransfer(ScpiParser& parser, unsigned dma = 0) {
    if (dma == 0) Fake::busy = false;
    else Fake::secondaryDma[dma].busy = false;
    CTI::WS2811::service(&parser);
    Fake::now += 1000;
    CTI::WS2811::service(&parser);
}
}

void ws2811Tests() {
    using namespace CTI;
    using namespace CTI::WS2811;
    Configuration config;
    config.count = 1;
    const uint8_t input[] = {255, 128, 1};
    constexpr unsigned indices[][3] = {{0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}};
    for (unsigned order = 0; order < 6; ++order) {
        config.order = static_cast<Order>(order);
        for (unsigned level : {0u, 1u, 25u, 100u}) {
            std::array<uint32_t, 3> guarded{0xdeadbeef, 0, 0xdeadbeef};
            pack(input, &guarded[1], config, level);
            uint32_t expected = 0;
            for (unsigned channel = 0; channel < 3; ++channel)
                expected |= ((input[indices[order][channel]] * level + 50) / 100) << (24 - 8 * channel);
            assert(guarded[1] == expected && guarded.front() == 0xdeadbeef && guarded.back() == 0xdeadbeef);
        }
    }
    config.count = MaxLeds;
    assert(config.valid() && config.transferUs() == 122880);
    config.setRate(400000);
    assert(config.transferUs() == 245760);
    Buffers large(MaxLeds);
    assert(large.valid());
    std::vector<uint8_t> rgb(MaxLeds * 3);
    for (size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<uint8_t>(i);
    assert(!large.write(1, rgb.data(), 3) && !large.write(0, rgb.data(), 0));
    assert(!large.write(0, rgb.data(), MaxChunk + 1));
    for (size_t offset = 0; offset < rgb.size(); offset += MaxChunk)
        assert(large.write(offset, rgb.data() + offset, std::min(MaxChunk, rgb.size() - offset)));
    assert(large.received == rgb.size() && !large.write(rgb.size(), rgb.data(), 1));

    Visa::Visa visa;
    Hub75::registerCommands(&visa);
    registerCommands(&visa);
    assert(!visa.registrationFailed);
    auto& parser = visa.parser;
    parser.finalize();
    auto command = [&](const std::string& text) { send(parser, "DISP:WS2811:" + text + "\n"); };
    auto query = [&](const std::string& text) {
        gPlatform.IO.output.clear();
        command(text);
        assert(pop(parser) == 0);
        return gPlatform.IO.output;
    };
    assert(query("COUNT?") == "64\n" && query("PIN?") == "2\n");
    assert(query("RATE?") == "800000\n" && query("ORD?") == "RGB\n");
    assert(query("TIM?") == "250,1000,625,625,400\n");
    command("TIM 500,2000,1200,1300,500");
    assert(pop(parser) == 0 && query("TIM?") == "500,2000,1188,1313,500\n");
    assert(query("RATE?") == "400000\n");
    for (const char* invalid : {
        "TIM 0,1000,625,625,400", "TIM 2000,1000,2200,625,400",
        "TIM 250,100,625,625,400", "TIM 250,4000,625,625,400",
        "TIM 250,1000,250,625,400", "TIM 250,1000,2000,625,400",
        "TIM 250,1000,625,625,49", "TIM 250,1000,625,625,100001",
        "TIM 250,1000,625,625", "TIM 250,1000,625,625,400,1",
        "TIM 4294967295,1000,625,625,400"}) {
        command(invalid);
        assert(pop(parser) != 0 && query("TIM?") == "500,2000,1188,1313,500\n");
    }
    command("RATE 800000");
    assert(pop(parser) == 0 && query("TIM?") == "250,1000,625,625,400\n");
    for (const char* invalid : {"COUNT 0", "COUNT 4097", "PIN 23", "PIN 29", "RATE 1000000",
                                 "ORD RGBW", "COUNT 1,2", "START extra"}) {
        command(invalid);
        assert(pop(parser) != 0 && Fake::channels == 0 && Fake::sms == 0);
    }
    assert(query("COUNT?") == "64\n");
    command("COUNT 4");
    assert(pop(parser) == 0);
    for (int allocation = 0; allocation < 4; ++allocation) {
        Fake::allocationsBeforeFailure = allocation;
        command("START");
        Fake::allocationsBeforeFailure = -1;
        assert(pop(parser) == -225 && Fake::channels == 0 && Fake::sms == 0);
    }
    Fake::pioAvailable = false;
    command("START");
    assert(pop(parser) == -225);
    Fake::pioAvailable = true;
    Fake::dmaAvailable = false;
    command("START");
    assert(pop(parser) == -225 && Fake::sms == 0 && Fake::programs == 0);
    Fake::dmaAvailable = true;
    Fake::functions[2] = GPIO_FUNC_UART;
    command("START");
    assert(pop(parser) == -221);
    gpio_deinit(2);
    command("START");
    assert(pop(parser) == 0 && Fake::channels == 1 && Fake::sms == 1);
    assert(Fake::config.sideBase == 2 && Fake::config.autoPull &&
        !Fake::config.shiftRight && Fake::config.shiftThreshold == 24);
    assert(std::abs(Fake::config.clockDivider - clock_get_hz(clk_sys) / 16000000.0f) < 0.001f);
    assert(query("STAT?") == "1,0,1,25,0\n");
    assert(!Hub75::allowPin(&parser, 2) && pop(parser) == -221);
    assert(Hub75::allowPin(&parser, 20));
    send(parser, "DISP:HUB75:START\n");
    assert(pop(parser) == -221 && Fake::channels == 1);
    for (const char* change : {"COUNT 5", "PIN 3", "RATE 400000", "ORD GRB",
                              "TIM 250,1000,625,625,500"}) {
        command(change);
        assert(pop(parser) == -221);
    }
    service(&parser);
    assert(!Fake::busy);
    beginTransfer(parser);
    assert(Fake::busy && Fake::transfers == 4 && Fake::handler == nullptr);
    Fake::busy = false;
    service(&parser);
    Fake::now += 500;
    service(&parser);
    assert(query("STAT?") == "1,0,1,25,0\n"); // FIFO/OSR drain plus reset-low is not complete.
    Fake::now += 200;
    service(&parser);
    assert(query("STAT?") == "1,0,0,25,1\n");
    command("SHOW");
    assert(pop(parser) == -221);
    const uint8_t pixels[] = {255,0,0, 0,255,0, 0,0,255, 0,0,0};
    command(block(0, pixels, 5));
    command(block(5, pixels + 5, 7));
    assert(pop(parser) == 0 && query("STAT?") == "1,12,0,25,1\n");
    command("SHOW");
    assert(pop(parser) == 0);
    const uint32_t* wire = static_cast<const uint32_t*>(Fake::source);
    assert(wire[0] == 0x40000000 && wire[1] == 0x00400000 && wire[2] == 0x00004000 && wire[3] == 0);
    command(block(0, pixels, 3)); // Next frame staging is independent of the wire buffer.
    command("BRIG 50");
    assert(pop(parser) == -221 && wire[0] == 0x40000000);
    beginTransfer(parser);
    completeTransfer(parser);
    command("BRIG 100");
    assert(pop(parser) == 0 && wire[0] == 0xff000000 && wire[1] == 0x00ff0000);
    beginTransfer(parser);
    completeTransfer(parser);
    assert(query("STAT?") == "1,3,0,100,3\n");
    command("START"); // Idempotent; retains current brightness and upload.
    assert(pop(parser) == 0 && query("STAT?") == "1,3,0,100,3\n");
    command("BRIG 0");
    assert(pop(parser) == 0 && wire[0] == 0);
    beginTransfer(parser);
    Fake::stuck = true;
    command("STOP");
    assert(pop(parser) == -200 && Fake::overrides[2] == GPIO_OVERRIDE_LOW && Fake::channels == 1);
    command("PIN 20");
    assert(pop(parser) == -221 && !Hub75::allowPin(&parser, 2) && pop(parser) == -221);
    Fake::stuck = false;
    service(&parser);
    assert(Fake::channels == 0 && Fake::sms == 0 && Fake::programs == 0);
    command("STOP");
    assert(pop(parser) == 0);
    command("START");
    beginTransfer(parser);
    Fake::channel.ctrl_trig = DMA_CH0_CTRL_TRIG_READ_ERROR_BITS;
    service(&parser);
    assert(pop(parser) == -200 && Fake::channels == 0);
    Fake::channel.ctrl_trig = 0;
    command("START");
    beginTransfer(parser);
    Fake::now += 200000;
    service(&parser);
    assert(pop(parser) == -200 && Fake::channels == 0);

    // Both services can coexist on disjoint GPIOs and independent DMA channels.
    send(parser, "DISP:HUB75:START\n");
    assert(pop(parser) == 0 && Fake::channels == 1);
    command("START");
    assert(pop(parser) == -221);
    command("PIN 20");
    command("RATE 400000");
    command("ORD GRB");
    command("START");
    assert(pop(parser) == 0 && Fake::channels == 2 && Fake::sms == 2);
    assert(Fake::claimedSms[0] == 3 && Fake::enabledSms[0] == 3);
    assert(query("ORD?") == "GRB\n" && query("RATE?") == "400000\n");
    beginTransfer(parser);
    assert(Fake::busy && Fake::secondaryDma[1].busy);
    completeTransfer(parser, 1);
    assert(query("STAT?") == "1,0,0,25,1\n");
    Fake::completeFrame();
    assert(!Hub75::allowPin(&parser, 20) && pop(parser) == -221);
    command("STOP");
    assert(pop(parser) == 0 && Fake::channels == 1 && Fake::handler != nullptr);
    assert(Fake::claimedSms[0] == 1 && Fake::enabledSms[0] == 1);
    Fake::completeFrame();
    assert(Hub75::stop() && Fake::channels == 0);
    command("PIN 26");
    command("START");
    assert(pop(parser) == 0);
    assert(!Hub75::allowAnalog(&parser, 0) && pop(parser) == -221);
    assert(stop());

    command("PIN 2");
    command("COUNT 1024");
    command("RATE 800000");
    command("ORD RGB");
    command("START");
    assert(pop(parser) == 0);
    beginTransfer(parser);
    completeTransfer(parser);
    std::vector<uint8_t> binary(3072);
    for (size_t i = 0; i < binary.size(); ++i) binary[i] = static_cast<uint8_t>(i);
    command(block(0, binary.data(), 2001));
    assert(pop(parser) == -222);
    command(block(0, binary.data(), 2000));
    command(block(2000, binary.data() + 2000, 1072));
    assert(pop(parser) == 0 && query("STAT?") == "1,3072,0,25,1\n");
    command("SHOW");
    assert(pop(parser) == 0);
    wire = static_cast<const uint32_t*>(Fake::source);
    Configuration expectedConfig;
    expectedConfig.count = 1024;
    std::vector<uint32_t> expected(1024);
    pack(binary.data(), expected.data(), expectedConfig, 25);
    assert(std::equal(expected.begin(), expected.end(), wire));
    assert(stop());
    command("COUNT 4");
    command("START");
    beginTransfer(parser);
    completeTransfer(parser);
    command("DATA 0,#212@!!!@!!!@@@@");
    command("SHOW");
    assert(pop(parser) == 0);
    beginTransfer(parser);
    wire = static_cast<const uint32_t*>(Fake::source);
    assert(wire[0] == 0x10080800 && wire[1] == 0x08100800);
    assert(wire[2] == 0x08081000 && wire[3] == 0x10101000);
    completeTransfer(parser);
    assert(stop());

    command("COUNT 4096");
    command("TIMING 1000,3062,2000,3062,5000");
    assert(pop(parser) == 0 && query("TIMING?") == "1000,3063,2000,3063,5000\n");
    assert(query("RATE?") == "197530\n");
    command("START");
    assert(pop(parser) == 0);
    auto duration = [](unsigned index) { return 1u + ((Fake::loadedInstructions[index] >> 8) & 15u); };
    assert(duration(1) == 16 && duration(2) == 16);
    assert(duration(3) + duration(4) + duration(5) + 1 == 49);
    assert(duration(6) + duration(7) + duration(8) + 1 == 49);
    Fake::now += 4900;
    service(&parser);
    assert(!Fake::busy);
    Fake::now += 100;
    service(&parser);
    assert(Fake::busy);
    Fake::now += 250000;
    service(&parser);
    assert(pop(parser) == 0 && Fake::busy); // Custom wire budget, not the old 800k timeout.
    Fake::busy = false;
    service(&parser);
    Fake::now += 6000;
    service(&parser);
    assert(query("STAT?") == "1,0,1,25,0\n"); // 1094us worst-case tail plus 5000us reset.
    Fake::now += 100;
    service(&parser);
    assert(query("STAT?") == "1,0,0,25,1\n");
    command("STOP");
    assert(pop(parser) == 0 && query("TIM?") == "1000,3063,2000,3063,5000\n");
    command("TIM 250,1000,625,625,50");
    command("START");
    assert(pop(parser) == 0);
    Fake::now += 50;
    service(&parser);
    assert(Fake::busy); // A shorter explicit reset replaces the default too.
    assert(stop());
    std::cout << "WS2811: encoding/orders/brightness, fragmented uploads, timing states, ownership, coexistence and failures passed\n";
}
