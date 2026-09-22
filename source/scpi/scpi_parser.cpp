#include "scpi/scpi_core.h"
#include "scpi/scpi_errors.h"

#include <ctype.h>
#include <string.h>

namespace CTI {
namespace SCPI {

ScpiChoice EndScpiChoice {nullptr, 0};

namespace {
bool matchSegment(
    const char* pattern, uint8_t patternLength,
    const char* candidate, uint8_t candidateLength,
    int8_t& number) {
    bool hasNumber = patternLength && pattern[patternLength - 1] == '#';
    if (hasNumber) {
        --patternLength;
    }

    uint8_t textLength = candidateLength;
    number = -1;
    if (hasNumber) {
        while (textLength && isdigit(candidate[textLength - 1])) {
            --textLength;
        }
        if (textLength == candidateLength) {
            return false;
        }

        int16_t parsed = 0;
        for (uint8_t i = textLength; i < candidateLength; ++i) {
            parsed = static_cast<int16_t>(parsed * 10 + candidate[i] - '0');
            if (parsed > 127) {
                return false;
            }
        }
        number = static_cast<int8_t>(parsed);
    }

    uint8_t requiredLength = 0;
    while (requiredLength < patternLength && !islower(pattern[requiredLength])) {
        ++requiredLength;
    }
    if (textLength != requiredLength && textLength != patternLength) {
        return false;
    }

    for (uint8_t i = 0; i < textLength; ++i) {
        if (toupper(candidate[i]) != toupper(pattern[i])) {
            return false;
        }
    }
    return true;
}

bool matchCommand(
    const char* pattern, const char* candidate, uint8_t candidateLength,
    NumParamVector& numbers, uint8_t& depth) {
    uint8_t patternPosition = pattern[0] == ':' ? 1 : 0;
    uint8_t candidatePosition = candidateLength && candidate[0] == ':' ? 1 : 0;
    depth = 0;

    while (pattern[patternPosition] != 0 && candidatePosition < candidateLength) {
        uint8_t patternEnd = patternPosition;
        while (pattern[patternEnd] != 0 && pattern[patternEnd] != ':') {
            ++patternEnd;
        }

        uint8_t candidateEnd = candidatePosition;
        while (candidateEnd < candidateLength && candidate[candidateEnd] != ':') {
            ++candidateEnd;
        }

        int8_t number;
        if (depth >= SCPI_MAX_DEPTH ||
            !matchSegment(
                pattern + patternPosition,
                static_cast<uint8_t>(patternEnd - patternPosition),
                candidate + candidatePosition,
                static_cast<uint8_t>(candidateEnd - candidatePosition),
                number)) {
            return false;
        }
        numbers.set(depth++, number);

        bool patternDone = pattern[patternEnd] == 0;
        bool candidateDone = candidateEnd == candidateLength;
        if (patternDone || candidateDone) {
            return patternDone && candidateDone;
        }
        patternPosition = static_cast<uint8_t>(patternEnd + 1);
        candidatePosition = static_cast<uint8_t>(candidateEnd + 1);
    }
    return pattern[patternPosition] == 0 && candidatePosition == candidateLength;
}
}

ScpiParser::ScpiParser(int bufferCapacity) {
    _buf = _bufferStorage;
    _bufCapacity = bufferCapacity < SCPI_INPUT_BUFFER_LENGTH
        ? bufferCapacity
        : SCPI_INPUT_BUFFER_LENGTH;
    _bufSize = 0;
    _maxDepth = 0;
    _commandCount = 0;
    _curCommand = nullptr;
    _state = ParserState::FindCommand;
    _paramPos = 0;
    _finalized = false;
    _isQuery = false;
}

ScpiParser::~ScpiParser() {
}

void ScpiParser::reset() {
    _bufSize = 0;
    _state = ParserState::FindCommand;
    _curCommand = nullptr;
    _isQuery = false;
}

int ScpiParser::bufferInput(const char* data, int count) {
    for (int i = 0; i < count; ++i) {
        if (_state == ParserState::FindCommand && _bufSize == 0 &&
            (data[i] == '\r' || data[i] == '\n')) {
            continue;
        }

        if (_bufSize >= _bufCapacity) {
            return i;
        }

        _buf[_bufSize++] = data[i];
        if (_state == ParserState::FindCommand) {
            if (data[i] == ' ' || data[i] == '?' || data[i] == '\n') {
                ParserStatus result = parseNode();
                if (result != ParserStatus::Success) {
                    errUndefinedHeader(this);
                    _state = ParserState::InvalidNode;
                }
                _bufSize = 0;
                if (data[i] == '\n') {
                    if (result == ParserStatus::Success) {
                        invokeNode();
                    }
                    reset();
                }
            }
        } else if (_state == ParserState::FindEndOfLine) {
            if (data[i] == '\n' && isMessageTerminator()) {
                invokeNode();
                reset();
            }
        } else if (_state == ParserState::InvalidNode && data[i] == '\n') {
            reset();
        }
    }
    return count;
}

bool ScpiParser::isMessageTerminator() const {
    if (_bufSize == 0 || _buf[_bufSize - 1] != '\n') {
        return false;
    }

    int position = 0;
    const int end = _bufSize - 1;
    while (position < end) {
        if (_buf[position] != '#' || position + 1 >= end ||
            !isdigit(static_cast<unsigned char>(_buf[position + 1]))) {
            ++position;
            continue;
        }

        int digitCount = _buf[position + 1] - '0';
        int lengthStart = position + 2;
        int payloadStart = lengthStart + digitCount;
        if (digitCount == 0 || payloadStart > end) {
            return false;
        }

        int payloadLength = 0;
        for (int digit = lengthStart; digit < payloadStart; ++digit) {
            if (!isdigit(static_cast<unsigned char>(_buf[digit]))) {
                return true;
            }
            payloadLength = payloadLength * 10 + _buf[digit] - '0';
        }

        if (payloadStart + payloadLength > end) {
            return false;
        }
        position = payloadStart + payloadLength;
    }
    return true;
}

ParseResult ScpiParser::parseChoice(const ScpiChoice* choices, int32_t& value) {
    consumeWhiteSpace();
    int32_t choice = 0;

    while (choices[choice].choiceString != nullptr) {
        uint8_t current = 0;
        const char* text = choices[choice].choiceString;
        while (_paramPos + current < _bufSize && text[current] != 0 &&
               cmpIChar(_buf[_paramPos + current], text[current])) {
            ++current;
        }
        if (text[current] == 0) {
            _paramPos += current;
            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }
            value = choices[choice].value;
            return ParseResult::Success;
        }
        ++choice;
    }
    return ParseResult::Invalid;
}

ParseResult ScpiParser::parseBool(bool& value) {
    consumeWhiteSpace();
    if (_paramPos >= _bufSize) {
        return ParseResult::EndOfData;
    }

    if (_buf[_paramPos] == '0') {
        value = false;
        ++_paramPos;
    } else if (_buf[_paramPos] == '1') {
        value = true;
        ++_paramPos;
    } else if (_paramPos + 1 < _bufSize &&
               toupper(_buf[_paramPos]) == 'O' &&
               toupper(_buf[_paramPos + 1]) == 'N') {
        value = true;
        _paramPos += 2;
    } else if (_paramPos + 2 < _bufSize &&
               toupper(_buf[_paramPos]) == 'O' &&
               toupper(_buf[_paramPos + 1]) == 'F' &&
               toupper(_buf[_paramPos + 2]) == 'F') {
        value = false;
        _paramPos += 3;
    } else {
        return ParseResult::Invalid;
    }
    return isEndOfParam() ? ParseResult::Success : ParseResult::Invalid;
}

ParseResult ScpiParser::parseBlock(char** buffer, int* length) {
    consumeWhiteSpace();
    if (_paramPos + 2 > _bufSize || _buf[_paramPos++] != '#') {
        return ParseResult::Invalid;
    }

    uint8_t digitCount = static_cast<uint8_t>(_buf[_paramPos++] - '0');
    if (digitCount > 5 || _paramPos + digitCount > _bufSize) {
        return ParseResult::Invalid;
    }

    int dataLength = 0;
    for (uint8_t i = 0; i < digitCount; ++i) {
        if (!isdigit(_buf[_paramPos])) {
            return ParseResult::Invalid;
        }
        dataLength = dataLength * 10 + _buf[_paramPos++] - '0';
    }
    if (dataLength < 0 || _paramPos + dataLength > _bufSize) {
        return ParseResult::InsufficientBuffer;
    }

    *buffer = _buf + _paramPos;
    *length = dataLength;
    _paramPos += dataLength;
    return isEndOfParam() ? ParseResult::Success : ParseResult::Invalid;
}

ParserStatus ScpiParser::parseNode() {
    _curCommand = nullptr;

    uint8_t commandLength = _bufSize;
    char terminator = _buf[commandLength - 1];
    --commandLength;
    _isQuery = terminator == '?';

    for (uint8_t i = 0; i < _commandCount; ++i) {
        uint8_t depth = 0;
        if (matchCommand(_commands[i].pattern, _buf, commandLength, _nodeNums, depth)) {
            _curCommand = &_commands[i];
            _state = ParserState::FindEndOfLine;
            return ParserStatus::Success;
        }
    }
    return ParserStatus::UnknownCommand;
}

ParserStatus ScpiParser::invokeNode() {
    if (_curCommand == nullptr) {
        errUndefinedHeader(this);
        return ParserStatus::Unknown;
    }

    _paramPos = 0;
    if (_isQuery) {
        if (_curCommand->query == nullptr) {
            errNoQuery(this);
            return ParserStatus::Unknown;
        }
        if (_curCommand->query(this) != QueryResult::Success) {
            return ParserStatus::Unknown;
        }
    } else {
        if (_curCommand->command == nullptr) {
            errNoCommand(this);
            return ParserStatus::UnknownCommand;
        }
        CommandResult result = _curCommand->command(this);
        if (result != CommandResult::Success) {
            if (result == CommandResult::MissingParam) {
                errMissingParam(this);
            } else if (result == CommandResult::UnexpectedParam) {
                errTooManyParams(this);
            } else if (result == CommandResult::SyntaxError) {
                errSyntax(this);
            }
            return ParserStatus::Unknown;
        }
    }
    return ParserStatus::Success;
}

bool ScpiParser::enqueueError(int16_t code, const char* text) {
    return _err.enqueue(code, text);
}

void ScpiParser::finalize() {
    _finalized = true;
    _nodeNums.reserve(_maxDepth);
}

RegistrationResult ScpiParser::registerNode(
    const char* pattern, ScpiCommand commandHandler, ScpiQuery queryHandler) {
    if (_finalized) {
        return RegistrationResult::AlreadyFinalized;
    }
    if (!commandHandler && !queryHandler) {
        return RegistrationResult::InvalidHandler;
    }
    if (_commandCount >= SCPI_MAX_COMMANDS) {
        return RegistrationResult::CapacityExceeded;
    }

    for (uint8_t i = 0; i < _commandCount; ++i) {
        if (strcmp(_commands[i].pattern, pattern) == 0) {
            return RegistrationResult::Ambiguous;
        }
    }

    uint8_t depth = 1;
    for (const char* current = pattern; *current; ++current) {
        if (*current == ':') {
            ++depth;
        }
    }
    if (depth > _maxDepth) {
        _maxDepth = depth;
    }
    if (depth > SCPI_MAX_DEPTH) {
        return RegistrationResult::CapacityExceeded;
    }

    _commands[_commandCount++] = {pattern, commandHandler, queryHandler};
    return RegistrationResult::Success;
}

bool cmpIChar(char first, char second) {
    return toupper(first) == toupper(second);
}

} // namespace SCPI
} // namespace CTI
