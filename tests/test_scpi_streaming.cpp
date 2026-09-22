#include "scpi/scpi_core.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using namespace CTI::SCPI;

namespace {
int idnCalls;
int blockCalls;
uint8_t blockData[8];
int blockLength;

QueryResult idnQuery(ScpiParser*) {
    ++idnCalls;
    return QueryResult::Success;
}

CommandResult blockCommand(ScpiParser* parser) {
    char* data = nullptr;
    int length = 0;
    if (parser->parseBlock(&data, &length) != ParseResult::Success) {
        return CommandResult::SyntaxError;
    }
    assert(length <= static_cast<int>(sizeof(blockData)));
    std::memcpy(blockData, data, length);
    blockLength = length;
    ++blockCalls;
    return CommandResult::Success;
}

void feed(ScpiParser& parser, const char* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        assert(parser.bufferInput(data + i, 1) == 1);
    }
}

template <size_t Size>
void feed(ScpiParser& parser, const char (&data)[Size]) {
    feed(parser, data, Size - 1);
}

void expectNoError(ScpiParser& parser) {
    int16_t code = -1;
    char* text = nullptr;
    assert(!parser.dequeueError(&code, &text));
    assert(code == 0);
}

void expectError(ScpiParser& parser, int16_t expected) {
    int16_t code = 0;
    char* text = nullptr;
    assert(parser.dequeueError(&code, &text));
    assert(code == expected);
}

void registerCommands(ScpiParser& parser) {
    assert(parser.registerNode("*IDN", nullptr, idnQuery) == RegistrationResult::Success);
    assert(
        parser.registerNode("BINary:DATA", blockCommand, nullptr) ==
        RegistrationResult::Success);
    parser.finalize();
}
}

int main() {
    {
        ScpiParser parser(64);
        registerCommands(parser);
        feed(parser, "*IDN?\n");
        assert(idnCalls == 1);
        expectNoError(parser);
    }

    {
        ScpiParser parser(64);
        registerCommands(parser);
        feed(parser, "*IDN?\r\n");
        feed(parser, "*IDN?\n");
        feed(parser, "*IDN?\r\n");
        assert(idnCalls == 4);
        expectNoError(parser);
    }

    {
        ScpiParser parser(64);
        registerCommands(parser);
        parser.reset();
        feed(parser, "*");
        feed(parser, "ID");
        feed(parser, "N?");
        feed(parser, "\r");
        feed(parser, "\n");
        assert(idnCalls == 5);
        expectNoError(parser);
    }

    {
        ScpiParser parser(64);
        registerCommands(parser);
        feed(parser, "\n\r\n");
        feed(parser, "*IDN?\n");
        assert(idnCalls == 6);
        expectNoError(parser);
    }

    {
        ScpiParser parser(64);
        registerCommands(parser);
        feed(parser, "BOGUS?\n");
        feed(parser, "*IDN?\n");
        assert(idnCalls == 7);
        expectError(parser, -113);
        expectNoError(parser);
    }

    {
        int expectedCalls = idnCalls;
        for (const char* truncated : {"IDN?\n", "DN?\n", "N?\n"}) {
            ScpiParser parser(64);
            registerCommands(parser);
            feed(parser, truncated, std::strlen(truncated));
            expectError(parser, -113);
            feed(parser, "*IDN?\n");
            assert(idnCalls == ++expectedCalls);
            expectNoError(parser);
        }
    }

    {
        ScpiParser parser(64);
        registerCommands(parser);
        const char message[] = {
            'B', 'I', 'N', ':', 'D', 'A', 'T', 'A', ' ',
            '#', '1', '3', 'A', '\n', 'B', '\n'
        };
        feed(parser, message, sizeof(message));
        assert(blockCalls == 1);
        assert(blockLength == 3);
        assert(blockData[0] == 'A');
        assert(blockData[1] == '\n');
        assert(blockData[2] == 'B');
        feed(parser, "*IDN?\n");
        assert(idnCalls == 11);
        expectNoError(parser);
    }

    return 0;
}
