#include "display/hub75.h"
#include "display/hub75_frame.h"
#include "visa/visa_core.h"
#include "fake_sdk.h"

#include <array>
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

CTI::Platform CTI::gPlatform;
using namespace CTI;
using namespace CTI::SCPI;

namespace Fake {
int allocationsBeforeFailure = -1;
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    if (Fake::allocationsBeforeFailure == 0) return nullptr;
    if (Fake::allocationsBeforeFailure > 0) --Fake::allocationsBeforeFailure;
    try {
        return ::operator new(bytes);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void* operator new[](std::size_t bytes, const std::nothrow_t& tag) noexcept {
    return ::operator new(bytes, tag);
}

namespace {
std::vector<uint8_t> captured;
uint32_t capturedOffset = 0;
int calls = 0, queries = 0;

void send(ScpiParser& parser, const std::string& message, size_t fragment = 1) {
    for (size_t offset = 0; offset < message.size(); offset += fragment) {
        size_t count = std::min(fragment, message.size() - offset);
        assert(parser.bufferInput(message.data() + offset, static_cast<int>(count)) == static_cast<int>(count));
    }
}

std::string block(const uint8_t* data, size_t length) {
    std::string digits = std::to_string(length);
    return "#" + std::to_string(digits.size()) + digits +
        std::string(reinterpret_cast<const char*>(data), length);
}

CommandResult capture(ScpiParser* parser) {
    char* data;
    int length;
    if (parser->parseInt(capturedOffset) != ParseResult::Success ||
        parser->parseBlock(&data, &length) != ParseResult::Success) return CommandResult::SyntaxError;
    captured.assign(data, data + length);
    assert(parser->position() > 255 || length < 255);
    ++calls;
    return CommandResult::Success;
}

QueryResult query(ScpiParser*) { ++queries; return QueryResult::Success; }

int popError(ScpiParser& parser) {
    int16_t code;
    char* text;
    parser.dequeueError(&code, &text);
    assert(text != nullptr);
    return code;
}

void parserTests() {
    ScpiParser parser(2048);
    assert(parser.registerNode("TEST:DATA", capture, nullptr) == RegistrationResult::Success);
    assert(parser.registerNode("TEST:STATus", nullptr, query) == RegistrationResult::Success);
    parser.finalize();
    assert(parser.registerNode("LATE", capture, nullptr) == RegistrationResult::AlreadyFinalized);
    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);
    for (size_t fragment : {1u, 7u, 64u, 4096u}) {
        send(parser, "TEST:DATA 8192," + block(data.data(), data.size()) + "\r\nTEST:STAT?\n", fragment);
        assert(captured == data && capturedOffset == 8192);
        assert(popError(parser) == 0);
    }
    assert(calls == 4 && queries == 4);
    send(parser, "TEST:DATA 0,#x\nTEST:DATA 0,#0abc\nTEST:DATA 0,#2x0\n");
    assert(popError(parser) != 0 && popError(parser) != 0 && popError(parser) != 0);
    assert(calls == 4);
    data.resize(3000, '\n');
    send(parser, "TEST:DATA 0," + block(data.data(), data.size()) + "\nTEST:STAT?\n", 13);
    assert(calls == 4 && queries == 5 && popError(parser) == -223);
    assert(popError(parser) == 0);
    send(parser, std::string(2600, 'X') + "\nTEST:STAT?\n");
    assert(queries == 6 && popError(parser) == -363);
    send(parser, "TEST:DATA 0,#41024abc");
    assert(parser.hasPendingInput() && calls == 4);
    parser.reset(); // MainLoop's bounded idle timeout calls this.
    send(parser, "TEST:STAT?\nTEST:DATA 4294967296,#10\n");
    assert(queries == 7 && calls == 4 && popError(parser) != 0);
    parser.reset();
    send(parser, "UNKNOWN?\nTEST:DATA 0,#10\nTEST:STAT?\n");
    assert(calls == 5 && queries == 8); // Bad query must not poison next command.
    assert(popError(parser) == -113);
    std::string hidden = "\nTEST:STAT?\n";
    send(parser, "UNKNOWN " + block(reinterpret_cast<const uint8_t*>(hidden.data()), hidden.size()) +
        "\nTEST:STAT?\n");
    assert(queries == 9 && popError(parser) == -113);
    parser.enqueueError(-200, "Persistent error text");
    int16_t code;
    char* text;
    parser.dequeueError(&code, &text);
    assert(code == -200 && std::string(text) == "Persistent error text");
    parser.enqueueError(-201, std::string(600, 'a').c_str());
    parser.dequeueError(&code, &text);
    assert(std::string(text).size() == SCPI_ERROR_STR_SIZE - 1);
    for (size_t length : {2028u, 2039u}) {
        std::vector<uint8_t> boundary(length);
        for (size_t i = 0; i < length; ++i) boundary[i] = static_cast<uint8_t>(i);
        std::string message = "TEST:DATA 0," + block(boundary.data(), length) + "\n";
        assert(length != 2028 || message.size() == 2047);
        for (size_t fragment : {1u, 63u, 512u, 4096u}) {
            send(parser, message + "TEST:STAT?\n", fragment);
            assert(captured == boundary && popError(parser) == 0);
        }
    }
    int before = calls;
    std::vector<uint8_t> tooLong(2040, '\n');
    send(parser, "TEST:DATA 0," + block(tooLong.data(), tooLong.size()) + "\nTEST:STAT?\n");
    assert(calls == before && popError(parser) == -363 && popError(parser) == 0);
    send(parser, "TEST:DATA 0," + block(tooLong.data(), 2038) + "\r\nTEST:STAT?\n");
    assert(calls == before + 1 && captured.size() == 2038 && popError(parser) == 0);
    std::cout << "Parser: binary bytes, fragmentation, >255-byte cursors, overflow, recovery and errors passed\n";
}

void frameTests() {
    using namespace Hub75;
    Upload upload;
    std::array<uint8_t, MaxChunk> chunk{};
    assert(!upload.complete());
    assert(!upload.write(1, chunk.data(), 3));
    assert(!upload.write(0, chunk.data(), 0));
    assert(!upload.write(0, chunk.data(), MaxChunk + 1));
    assert(!upload.write(FrameBytes + 1, chunk.data(), 1));
    for (size_t offset = 0; offset < FrameBytes; offset += MaxChunk) {
        assert(upload.write(offset, chunk.data(), std::min(MaxChunk, FrameBytes - offset)));
    }
    assert(upload.complete());
    assert(!upload.write(FrameBytes, chunk.data(), 1));
    assert(upload.write(0, chunk.data(), 13) && !upload.complete() && upload.received == 13);
    for (size_t i = 0; i < FrameBytes; ++i) upload.rgb[i] = static_cast<uint8_t>((i * 73 + i / 17) & 255);
    std::vector<uint32_t> scan(ScanWords);
    for (unsigned percent : {0u, 1u, 25u, 100u}) {
        pack(upload.rgb, percent, scan.data());
        uint32_t totalTicks = 0;
        for (size_t row = 0; row < RowPairs; ++row) {
            for (size_t plane = 0; plane < Planes; ++plane) {
                auto* words = scan.data() + (row * Planes + plane) * WordsPerRow;
                assert(words[0] == (RowSetInstruction | row | ((Width - 1) << 16)));
                for (size_t x = 0; x < Width; ++x) {
                    uint32_t color = words[1 + x / PixelsPerWord] >> (6 * (x % PixelsPerWord));
                    for (size_t half = 0; half < 2; ++half) {
                        for (size_t channel = 0; channel < 3; ++channel) {
                            uint8_t pixel = upload.rgb[((row + half * RowPairs) * Width + x) * 3 + channel];
                            assert(((color >> (half * 3 + channel)) & 1) == ((pixel >> (4 + plane)) & 1));
                        }
                    }
                }
                for (size_t i = 1; i <= PixelWords; ++i) assert((words[i] >> 30) == 0);
                uint32_t on = words[WordsPerRow - 2] ? words[WordsPerRow - 2] + 1 : 0;
                uint32_t ticks = BaseTicks * (1u << plane);
                assert(on == ticks * percent / 100);
                assert(words[WordsPerRow - 1] + on == ticks);
                totalTicks += RowOverheadTicks + on + words[WordsPerRow - 1];
            }
        }
        assert(totalTicks == FrameTicks);
        std::vector<uint32_t> original = scan;
        setBrightness(scan.data(), 50);
        for (size_t i = 0; i < ScanWords; ++i) {
            if (i % WordsPerRow < WordsPerRow - 2) assert(scan[i] == original[i]);
        }
    }
    if (Width == 64 && Height == 64) {
        assert(ScanWords == 2048 && FrameTicks == 275200);
        assert(FrameBytes + 2 * ScanWords * sizeof(uint32_t) == 28672);
    }
    std::cout << "Frame: " << Width << "x" << Height
        << " packed pixel/half/plane mapping, dwell timing and upload boundaries passed\n";
}

void lifecycleTests() {
    using namespace Hub75;
    Visa::Visa visa;
    registerCommands(&visa);
    assert(!visa.registrationFailed);
    visa.parser.finalize();
    auto& parser = visa.parser;
    auto command = [&](const std::string& message) { send(parser, "DISP:HUB75:" + message + "\n", 17); };
    auto clean = [&] {
        assert(Fake::channels == 0 && Fake::sms == 0 && Fake::programs == 0 && !Fake::handler);
        assert(!Fake::pioEnabled && !Fake::busy);
        assert(Fake::oeHigh());
    };
    command("STOP");
    command("SHOW");
    assert(popError(parser) == -221);
    Fake::functions[3] = GPIO_FUNC_UART;
    command("INIT");
    assert(popError(parser) == -221 && Fake::channels == 0);
    Fake::functions[3] = GPIO_FUNC_NULL;
    Fake::exclusiveIrq = true;
    command("INIT");
    assert(popError(parser) == -225);
    Fake::exclusiveIrq = false;

    for (int allocation = 0; allocation < 4; ++allocation) {
        Fake::allocationsBeforeFailure = allocation;
        command("INIT");
        Fake::allocationsBeforeFailure = -1;
        assert(popError(parser) == -225 && Fake::channels == 0 && Fake::sms == 0 &&
               Fake::programs == 0 && Fake::handler == nullptr && Fake::oeHigh());
    }
    Fake::pioAvailable = false;
    command("INIT");
    assert(popError(parser) == -225);
    clean();
    Fake::pioAvailable = true;
    Fake::dmaAvailable = false;
    command("INIT");
    assert(popError(parser) == -225);
    clean();
    Fake::dmaAvailable = true;
    for (int cycle = 0; cycle < 3; ++cycle) {
        command("INIT");
        command("INIT");
        assert(popError(parser) == 0);
        assert(Fake::channels == 1 && Fake::sms == 1 && Fake::programs == 1 && Fake::pioEnabled);
        assert(Fake::config.outBase == 0 && Fake::config.outCount == 6);
        assert(Fake::config.setBase == 6 && Fake::config.setCount == 5 && Fake::config.sideBase == 11);
        assert(Fake::config.shiftRight && !Fake::config.autoPull && Fake::config.shiftThreshold == 30);
        assert(Fake::oeHigh() && Fake::transfers == ScanWords);
        assert(!allowPin(&parser, 0) && !allowPin(&parser, 13) && allowPin(&parser, 14));
        assert(popError(parser) == -221 && popError(parser) == -221);
        command("SHOW");
        assert(popError(parser) == -221);
        std::array<uint8_t, MaxChunk> chunk;
        chunk.fill(255);
        const void* old = Fake::source;
        std::vector<uint32_t> activeBefore(static_cast<const uint32_t*>(old), static_cast<const uint32_t*>(old) + ScanWords);
        for (size_t offset = 0; offset < FrameBytes; offset += MaxChunk) {
            command("DATA " + std::to_string(offset) + "," +
                block(chunk.data(), std::min(MaxChunk, FrameBytes - offset)));
            assert(popError(parser) == 0 && Fake::source == old);
        }
        command("SHOW");
        assert(popError(parser) == 0 && Fake::source == old);
        assert(std::equal(activeBefore.begin(), activeBefore.end(), static_cast<const uint32_t*>(old)));
        command("BRIG 50");
        command("DATA 0," + block(chunk.data(), std::min(MaxChunk, FrameBytes)));
        assert(popError(parser) == -221 && popError(parser) == 0);
        Fake::completeFrame();
        assert(Fake::source != old);
        old = Fake::source;
        command("BRIG 0");
        assert(Fake::source == old);
        Fake::completeFrame();
        const auto* blank = static_cast<const uint32_t*>(Fake::source);
        for (size_t i = 0; i < ScanWords; i += WordsPerRow) assert(blank[i + WordsPerRow - 2] == 0);
        command("BRIG 101");
        command("BRIG -1");
        command("INIT extra");
        assert(popError(parser) == -222 && popError(parser) != 0 && popError(parser) == -108);
        gPlatform.IO.output.clear();
        command("STAT?");
        assert(gPlatform.IO.output == "1," + std::to_string(std::min(MaxChunk, FrameBytes)) + ",0,0,2\n");
        gPlatform.IO.output.clear();
        command("GEOM?");
        assert(gPlatform.IO.output == std::to_string(Width) + "," + std::to_string(Height) +
            "," + std::to_string(FrameBytes) + ",4\n");
        command("STOP");
        command("STOP");
        assert(popError(parser) == 0 && allowPin(&parser, 0));
        clean();
    }
    command("INIT");
    Fake::channel.ctrl_trig = DMA_CH0_CTRL_TRIG_READ_ERROR_BITS;
    service(&parser);
    assert(popError(parser) == -200);
    Fake::channel.ctrl_trig = 0;
    clean();
    command("INIT");
    Fake::now += 100001;
    service(&parser);
    assert(popError(parser) == -200);
    clean();
    command("INIT");
    Fake::stuck = true;
    command("STOP");
    assert(popError(parser) == -200 && Fake::channels == 1 && Fake::oeHigh());
    assert(!allowPin(&parser, 0) && popError(parser) == -221);
    Fake::stuck = false;
    service(&parser);
    clean();
    std::cout << "Lifecycle: repeated init/stop, pin conflicts, resource failures, asynchronous swaps, brightness, DMA errors and bounded abort cleanup passed\n";
}

void runtimeTests() {
    using namespace Hub75;
    Visa::Visa visa;
    registerCommands(&visa);
    visa.parser.finalize();
    auto& parser = visa.parser;
    auto command = [&](const std::string& text) { send(parser, "DISP:HUB75:" + text + "\n", 31); };
    auto queryText = [&](const std::string& text) {
        gPlatform.IO.output.clear();
        command(text);
        assert(popError(parser) == 0);
        return gPlatform.IO.output;
    };
    assert(Fake::channels == 0 && Fake::sms == 0);
    assert(queryText("FORM?") == "RGB888\n");
    const std::string defaultPins = "0,1,2,3,4,5,6,7,8,9,10,11,12,13";
    assert(queryText("PINS?") == defaultPins + "\n");
    command("DIM 64,64");
    command("FORM RGB332");
    assert(queryText("GEOM?") == "64,64,4096,4\n");
    for (const char* bad : {"DIM 0,64", "DIM 257,64", "DIM 64,48", "DIM 4294967296,64",
                                   "FORM RGB565", "DIM 32,32,1", "PINS 0,1,2", "FORM RGB332 extra",
                                   "PINS 0,1,2,3,4,5,6,7,8,9,10,11,12,12",
                                   "PINS 0,1,2,3,5,4,6,7,8,9,10,11,12,13",
                                   "PINS 0,1,2,3,4,5,6,7,8,9,10,23,24,25",
                                   "PINS 0,1,2,3,4,5,6,7,8,9,10,27,28,29"}) {
        command(bad);
        assert(popError(parser) != 0);
        assert(queryText("GEOM?") == "64,64,4096,4\n");
        assert(queryText("PINS?") == defaultPins + "\n");
        assert(Fake::channels == 0 && Fake::sms == 0);
    }

    // Different locations for all three pin groups, including the ADC pins as controls.
    const std::string remapped = "14,15,16,17,18,19,0,1,2,3,4,26,27,28";
    command("PINS " + remapped);
    assert(queryText("PINS?") == remapped + "\n");
    Fake::functions[26] = GPIO_FUNC_UART;
    command("START");
    assert(popError(parser) == -221 && Fake::channels == 0);
    Fake::functions[26] = GPIO_FUNC_NULL;
    command("START");
    assert(popError(parser) == 0 && Fake::channels == 1 && Fake::sms == 1);
    assert(Fake::config.outBase == 14 && Fake::config.setBase == 0 && Fake::config.sideBase == 26);
    assert(Fake::smPins == (1u << 28));
    for (const std::string& change : std::initializer_list<std::string>{"DIM 32,32", "PINS " + defaultPins, "FORM RGB888"}) {
        command(change);
        assert(popError(parser) == -221);
    }
    assert(queryText("GEOM?") == "64,64,4096,4\n");
    assert(allowPin(&parser, 13) && allowPin(&parser, 6));
    gpio_deinit(13); // Simulate the permitted GPIO command actually releasing the old blank output.
    assert(!allowPin(&parser, 14) && popError(parser) == -221);
    assert(!allowAnalog(&parser, 0) && popError(parser) == -221);
    assert(!allowAnalog(&parser, 2) && popError(parser) == -221);
#if PICO_RP2040
    assert(allowAnalog(&parser, 3));
#else
    assert(!allowAnalog(&parser, 3) && popError(parser) == -222);
#endif
    assert(!allowAnalog(&parser, 4) && popError(parser) == -222);

    std::array<uint8_t, 4096> image;
    for (size_t i = 0; i < image.size(); ++i) image[i] = static_cast<uint8_t>(i);
    const auto* old = static_cast<const uint32_t*>(Fake::source);
    for (size_t offset = 0; offset < image.size(); offset += MaxChunk) {
        command("DATA " + std::to_string(offset) + "," + block(image.data() + offset, std::min(MaxChunk, image.size() - offset)));
    }
    assert(queryText("STAT?") == "1,4096,0,25,0\n");
    command("DATA 4096," + block(image.data(), 1));
    assert(popError(parser) == -222);
    command("SHOW");
    assert(popError(parser) == 0 && old == Fake::source);
    // Stage the next frame before the pending frame's DMA boundary.
    command("DATA 0," + block(image.data(), MaxChunk));
    assert(popError(parser) == 0);
    command("SHOW");
    assert(popError(parser) == -221);
    Fake::completeFrame();
    const auto* displayed = static_cast<const uint32_t*>(Fake::source);
    assert(old != displayed);
    Configuration current;
    current.format = PixelFormat::RGB332;
    current.width = current.height = 64;
    std::vector<uint32_t> expected(current.scanWords());
    pack(image.data(), 25, expected.data(), current);
    assert(std::equal(expected.begin(), expected.end(), displayed));
    command("BRIG 50");
    assert(popError(parser) == 0);
    Fake::completeFrame();
    assert(queryText("STAT?") == "1," + std::to_string(MaxChunk) + ",0,50,2\n"); // Partial frame survives brightness.
    command("STOP");
    assert(popError(parser) == 0 && Fake::outputs[28] && Fake::dirs[28]);
    assert(Fake::channels == 0 && Fake::sms == 0 && Fake::programs == 0);
    assert(allowAnalog(&parser, 0) && allowAnalog(&parser, 2));
    gpio_deinit(28); // Simulate release after the permitted analog reconfiguration.
    command("START");
    assert(popError(parser) == 0);
    Fake::stuck = true;
    command("STOP");
    assert(popError(parser) == -200 && Fake::overrides[28] == GPIO_OVERRIDE_HIGH);
    Fake::busy = false; // BUSY can clear before an outstanding ABORT finishes.
    command("STOP");
    assert(popError(parser) == -200 && Fake::channels == 1);
    command("PINS " + defaultPins);
    assert(popError(parser) == -221);
    Fake::stuck = false;
    service(&parser);
    command("PINS " + defaultPins);
    assert(popError(parser) == 0);

    for (const auto& dimensions : {std::pair<size_t, size_t>{8,16}, {65,32}, {256,64}}) {
        command("DIM " + std::to_string(dimensions.first) + "," + std::to_string(dimensions.second));
        command("START");
        assert(popError(parser) == 0);
        Configuration geometry;
        geometry.width = dimensions.first;
        geometry.height = dimensions.second;
        assert(Fake::transfers == geometry.scanWords());
        command("STOP");
        assert(popError(parser) == 0 && Fake::channels == 0);
    }
    command("DIM " + std::to_string(Width) + "," + std::to_string(Height));
    command("FORM RGB888");
    assert(popError(parser) == 0);
    std::cout << "Runtime: RGB332 pipeline, remapped pin ownership, queries, atomic configuration, bounds, repeated START/STOP and geometry changes passed\n";
}

void rgb332Tests() {
    using namespace Hub75;
    for (unsigned byte = 0; byte <= 255; ++byte) {
        uint8_t input = static_cast<uint8_t>(byte);
        const uint8_t levels[] = {0, 2, 4, 6, 9, 11, 13, 15};
        assert(channelLevel(&input, 0, 0, PixelFormat::RGB332) == levels[byte >> 5]);
        assert(channelLevel(&input, 0, 1, PixelFormat::RGB332) == levels[(byte >> 2) & 7]);
        assert(channelLevel(&input, 0, 2, PixelFormat::RGB332) == (byte & 3) * 5);
    }
    for (const auto& size : {std::pair<size_t,size_t>{8,16}, {64,64}, {65,32}, {256,64}}) {
        Configuration config;
        config.width = size.first;
        config.height = size.second;
        config.format = PixelFormat::RGB332;
        Upload bytes(config.frameBytes());
        assert(bytes.rgb);
        std::vector<uint8_t> rgb(config.width * config.height * 3);
        for (size_t i = 0; i < config.frameBytes(); ++i) {
            uint8_t byte = static_cast<uint8_t>(i);
            bytes.rgb[i] = byte;
            const uint8_t levels[] = {0,2,4,6,9,11,13,15};
            rgb[3*i] = levels[byte >> 5] << 4;
            rgb[3*i+1] = levels[(byte >> 2) & 7] << 4;
            rgb[3*i+2] = (byte & 3) * 5 << 4;
        }
        std::vector<uint32_t> compact(config.scanWords()+2, 0xdeadbeef);
        std::vector<uint32_t> reference(config.scanWords());
        pack(bytes.rgb, 25, compact.data()+1, config);
        Configuration rgbConfig = config;
        rgbConfig.format = PixelFormat::RGB888;
        pack(rgb.data(), 25, reference.data(), rgbConfig);
        assert(std::equal(reference.begin(), reference.end(), compact.begin()+1));
        assert(compact.front() == 0xdeadbeef && compact.back() == 0xdeadbeef);
        assert(!bytes.write(config.frameBytes(), bytes.rgb, 1));
        if (config.width == 64 && config.height == 64) {
            assert(config.frameBytes() + 2 * config.scanWords() * sizeof(uint32_t) == 20480);
        }
    }
    std::cout << "RGB332: all 256 colors, both halves/all planes, runtime geometry and memory guards passed\n";
}
}

int main(int argc, char** argv) {
    if (argc >= 4 && std::string(argv[1]) == "--scan") {
        Hub75::Configuration config;
        if (argc >= 6) {
            config.width = std::stoul(argv[4]);
            config.height = std::stoul(argv[5]);
        }
        if (argc == 7) config.format = std::string(argv[6]) == "RGB332" ? Hub75::PixelFormat::RGB332 : Hub75::PixelFormat::RGB888;
        assert(config.validDimensions());
        Hub75::Upload upload(config.frameBytes());
        assert(upload.rgb);
        for (size_t i = 0; i < config.frameBytes(); ++i) {
            upload.rgb[i] = static_cast<uint8_t>((i * 73 + i / 17) & 255);
        }
        std::vector<uint32_t> scan(config.scanWords());
        unsigned brightness = static_cast<unsigned>(std::stoul(argv[3]));
        assert(brightness <= 100);
        Hub75::pack(upload.rgb, brightness, scan.data(), config);
        std::ofstream output(argv[2], std::ios::binary);
        assert(output.good());
        output.write(reinterpret_cast<const char*>(scan.data()), scan.size() * sizeof(uint32_t));
        assert(output.good());
        std::cout << config.width << "," << config.height << "\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--register") {
        std::ifstream input(argv[2]);
        assert(input.good());
        std::vector<std::string> names;
        std::string line;
        while (std::getline(input, line)) names.push_back(line);
        ScpiParser parser(2048);
        for (const auto& name : names) {
            assert(parser.registerNode(name.c_str(), capture, query) == RegistrationResult::Success);
        }
        assert(names.size() > 40);
        assert(parser.registerNode(names[0].c_str(), capture, query) == RegistrationResult::Ambiguous);
        parser.finalize();
        std::cout << names.size() << " actual firmware commands registered without collisions\n";
        return 0;
    }
    if (argc == 2) {
        std::ifstream input(argv[1], std::ios::binary);
        assert(input.good());
        std::string stream((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Visa::Visa visa;
        Hub75::registerCommands(&visa);
        assert(!visa.registrationFailed);
        visa.parser.finalize();
        send(visa.parser, stream, 29);
        assert(popError(visa.parser) == 0 && Fake::channels == 1);
        const void* before = Fake::source;
        Fake::completeFrame();
        assert(Fake::source != before);
        assert(Hub75::stop());
        std::cout << "Generated host SCPI stream accepted by actual parser/display handlers\n";
        return 0;
    }
    parserTests();
    frameTests();
    lifecycleTests();
    rgb332Tests();
    runtimeTests();
    std::cout << "All host tests passed (mock SDK; no physical panel timing validation)\n";
}
