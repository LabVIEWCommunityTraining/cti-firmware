#ifndef scpi_core_h_
#define scpi_core_h_

#include "cti/platform.h"

#include <stdint.h>
#include <stdlib.h>

#ifndef SCPI_ERROR_QUEUE_SIZE
#define SCPI_ERROR_QUEUE_SIZE 10
#endif

#ifndef SCPI_ERROR_STR_SIZE
#define SCPI_ERROR_STR_SIZE 256
#endif

#ifndef SCPI_MAX_COMMANDS
#define SCPI_MAX_COMMANDS 40
#endif

#ifndef SCPI_MAX_DEPTH
#define SCPI_MAX_DEPTH 4
#endif

#ifndef SCPI_INPUT_BUFFER_LENGTH
#define SCPI_INPUT_BUFFER_LENGTH 2048
#endif

namespace CTI {
namespace SCPI {

    enum class ParserStatus {
        Success,
        Unknown,
        Incomplete,
        SyntaxError,
        UnknownCommand,
        MissingParameter
    };

    enum class CommandResult {
        Success,
        MissingParam,
        UnexpectedParam,
        SyntaxError,        // Try and avoid in favor of more specificity
        NoHandler,
        Error               // Used when a more specific error has been enqueued
    };

    enum class QueryResult {
        Success,
        NoHandler,
        MissingParam,
        UnexpectedParam,
        Error
    };

    enum class RegistrationResult {
        Success,
        SyntaxError,
        Ambiguous,
        InvalidHandler,
        AlreadyFinalized,
        CapacityExceeded,
    } ;

    enum class ParserState {
        Idle,
        ReadingCommand,
        FindCommand,
        FindEndOfLine,
        InvokeScpi,
        InvalidNode,
    };

    enum class ParseResult {
        Invalid,
        Success,
        InsufficientBuffer,
        EndOfData,
    };

    enum class NumberFormat {
        Invalid,
        Dec,
        Binary,
        Oct,
        Hex
    };

    enum class BufferResult {
        Success,
        UnmatchedCommand,
        BufferOverflow,
        Unknown,
    };

    class ScpiParser;

    class NumParamVector {
    public:
        NumParamVector() {
            _count = 0;
            for (uint8_t i = 0; i < SCPI_MAX_DEPTH; ++i) {
                _nums[i] = -1;
            }
        }

        int8_t get(uint8_t index) const {
            if (index < _count) {
                return _nums[index];
            }

            return -1;
        }

        bool set(uint8_t index, int8_t value) {
            if (index < _count) {
                _nums[index] = value;
                return true;
            }

            return false;
        }

        bool reserve(uint8_t size) {
            if (size > SCPI_MAX_DEPTH) {
                return false;
            }
            _count = size;
            return true;
        }
    private:
        int8_t _nums[SCPI_MAX_DEPTH];
        uint8_t _count;
    };

    typedef struct {
        int16_t code;
        char str[SCPI_ERROR_STR_SIZE];
    } ScpiError;

    class ScpiErrorQueue {
    public:
        ScpiErrorQueue();
        
        bool enqueue(int16_t code, const char* str);
        bool dequeue(int16_t* code, char ** str);

        void clear();
        
        bool full() {
            return _full;
        }

        bool overflow() {
            return _overflow;
        }
    
    private:
        ScpiError _err[SCPI_ERROR_QUEUE_SIZE];
        uint8_t _capacity;
        uint8_t _head;
        uint8_t _tail;

        bool _empty;
        bool _full;
        bool _overflow;
    };

    typedef CommandResult (*ScpiCommand)(ScpiParser* scpi);
    typedef QueryResult (*ScpiQuery)(ScpiParser* scpi);

    typedef struct {
        const char* choiceString;
        uint8_t value;
    } ScpiChoice;

    extern ScpiChoice EndScpiChoice;

    class ScpiParser {
    public:
        ScpiParser(int bufCapacity = 255);
        ~ScpiParser();

        void finalize();

        int bufferInput(const char* data, int n);

        RegistrationResult registerNode(const char* str, ScpiCommand cmdHandler, ScpiQuery queryHandler);

        ParseResult parseBool(bool& value);
        ParseResult parseBlock(char** buf, int* len);
        ParseResult parseChoice(const ScpiChoice* choices, int32_t& value);

        ParseResult parseInt(uint8_t& value) {
            return parseIntFormat(value, 3);
        }

        ParseResult parseInt(int8_t& value) {
            return parseIntFormat(value, 3, true);
        }

        ParseResult parseInt(uint16_t& value) {
            return parseIntFormat(value, 5);
        }

        ParseResult parseInt(int16_t& value) {
            return parseIntFormat(value, 5, true);
        }

        ParseResult parseInt(uint32_t& value) {
            return parseIntFormat(value, 10);
        }

        ParseResult parseInt(int32_t& value) {
            return parseIntFormat(value, 10, true);
        }

        ParseResult parseReal(float& value) {
            return parseRealFormat(value);
        }

        ParseResult parseReal(double& value) {
            return parseRealFormat(value);
        }

        bool enqueueError(int16_t code, const char* str);

        bool dequeueError(int16_t* code, char** str) {
            return _err.dequeue(code, str);
        }

        void clearErrors() {
            _err.clear();
        }

        void reset();

        int16_t position() {
            return _paramPos;
        }

        char curChar() {
            return _buf[_paramPos];
        }



        ChanIndex nodeNum(uint8_t level) {
            return _nodeNums.get(level);
        }

    private:
        ParserStatus parseNode();
        ParserStatus invokeNode();
        bool isMessageTerminator() const;

        void consumeWhiteSpace() {
            while (_paramPos < _bufSize &&
                (_buf[_paramPos] == ' ' || _buf[_paramPos] == '\t' || _buf[_paramPos] == '\r')) {

                _paramPos++;
            }
        }

        bool isEndOfParam() {
            if (_paramPos >= _bufSize) {
                return true;
            }
            char c = _buf[_paramPos];
            bool isEnd = (c == ' ' ||
                c == '\n' ||
                c == '\t' ||
                c == '\r' ||
                c == ','
            );

            if (c == ',') {
                _paramPos++; //consume separator comma
            }

            return isEnd;
        }

        NumberFormat numberFormat() {
            if (_paramPos >= _bufSize) {
                return NumberFormat::Invalid;
            }
            if (_buf[_paramPos] == '#') {
                //Has a numeric format specifier
                _paramPos++;
                if (_paramPos == _bufSize) {
                    return NumberFormat::Invalid;
                }

                if (_buf[_paramPos] == 'B' || _buf[_paramPos] == 'b') {
                    _paramPos++;
                    return NumberFormat::Binary;
                } else if (_buf[_paramPos] == 'H' || _buf[_paramPos] == 'h') {
                    _paramPos++;
                    return NumberFormat::Hex;
                } else if (_buf[_paramPos] == 'Q' || _buf[_paramPos] == 'q') {
                    _paramPos++;
                    return NumberFormat::Oct;
                }

                //Unexpected character after #
                return NumberFormat::Invalid;
            }

            if (_buf[_paramPos] <= '9' && _buf[_paramPos] >= '0') {
                return NumberFormat::Dec;
            }

            if (_buf[_paramPos] == '-' || _buf[_paramPos] == '+') {
                return NumberFormat::Dec;
            }

            //No match, unexpected input
            return NumberFormat::Invalid;
        }

        template <class T>
        ParseResult parseIntFormat(T& val, uint8_t maxDigits, bool sign = false) {
            consumeWhiteSpace();

            NumberFormat format = numberFormat();

            switch (format) {
                case NumberFormat::Binary:
                    return parseBinary(val, maxDigits);

                case NumberFormat::Dec:
                    return parseDec(val, maxDigits, sign);

                case NumberFormat::Hex:
                    return parseHex(val, maxDigits);
                
                case NumberFormat::Oct:
                    return parseOct(val, maxDigits);
                
                default:
                    return ParseResult::Invalid;
            }
        }

        template <class T>
        ParseResult parseDec(T& val, uint8_t maxDigits, bool sign) {
            val = 0;

            bool neg = false;

            if (sign && _buf[_paramPos] == '-') {
                neg = true;
                _paramPos++;
            } else if (_buf[_paramPos] == '+') { //consume optional +
                _paramPos++;
            }

            for (int i = 0; i < maxDigits && _paramPos < _bufSize; ++i) {
                if (_buf[_paramPos] <= '9' && _buf[_paramPos] >= '0') {
                    val *= 10;
                    val += (_buf[_paramPos] - '0');
                    _paramPos++;
                } else {
                    break;
                }
            }

            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }

            if (neg) {
                val *= -1;
            }

            return ParseResult::Success;
        }

        template <class T>
        ParseResult parseBinary(T& val, uint8_t maxDigits) {
            val = 0;

            for (int i = 0; i < maxDigits && _paramPos < _bufSize; ++i) {
                char c = _buf[_paramPos];
                val = val << 1;
                
                if (c == '1') {
                    val += 1;
                    _paramPos++;
                } else if (c == '0') {
                    _paramPos++;
                } else {
                    break;
                }
            }

            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }

            return ParseResult::Success;
        }

        template <class T>
        ParseResult parseHex(T& val, uint8_t maxDigits) {
            val = 0;

            for (int i = 0; i < maxDigits && _paramPos < _bufSize; ++i) {
                char c = _buf[_paramPos];
                val = val << 4;

                if (c <= '9' && c >= '0') {
                    val += (c - '0');
                    _paramPos++;
                } else if (c <= 'F' && c >= 'A') {
                    val += (c - 'A' + 10);
                    _paramPos++;
                } else if (c <= 'f' && c >= 'a') {
                    val += (c - 'a' + 10);
                    _paramPos++;
                } else {
                    break;
                }
            }

            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }

            return ParseResult::Success;
        }

        template <class T>
        ParseResult parseOct(T& val, uint8_t maxDigits) {
            val = 0;

            for (int i = 0; i < maxDigits && _paramPos < _bufSize; ++i) {
                char c = _buf[_paramPos];
                val = val << 3;

                if (c <= '7' && c >= '0') {
                    val += (c - '0');
                    _paramPos++;
                } else {
                    break;
                }
            }

            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }

            return ParseResult::Success;
        }
        
        template <class T>
        ParseResult parseRealFormat(T& val) {
            consumeWhiteSpace();
            if (_paramPos >= _bufSize) {
                return ParseResult::EndOfData;
            }

            val = 0;
            bool neg = false;

            if (_buf[_paramPos] == '-') {
                _paramPos++;
                neg = true;
            } else if (_buf[_paramPos] == '+') { //optional '+'
                _paramPos++;
            }

            while (_paramPos < _bufSize) {
                if (_buf[_paramPos] <= '9' && _buf[_paramPos] >= '0') {
                    val *= 10;
                    val += (_buf[_paramPos] - '0');
                    _paramPos++;
                } else {
                    break;
                }
            }

            if (_paramPos < _bufSize && _buf[_paramPos] == '.') {
                _paramPos++;

                int frac = 0;
                int div = 1;

                while (_paramPos < _bufSize) {
                    if (_buf[_paramPos] <= '9' && _buf[_paramPos] >= '0') {
                        frac *= 10;
                        div *= 10;
                        frac += (_buf[_paramPos] - '0');
                        _paramPos++;
                    } else {
                        break;
                    }
                }

                val += (T)frac / div;
            }

            if (neg) {
                val *= -1;
            }

            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }

            return ParseResult::Success;
        }

        struct CommandEntry {
            const char* pattern;
            ScpiCommand command;
            ScpiQuery query;
        };

        char _bufferStorage[SCPI_INPUT_BUFFER_LENGTH];
        char* _buf;
        int _bufSize;
        int _bufCapacity;

        uint8_t _paramPos;

        bool _finalized;

        bool _isQuery;

        uint8_t _maxDepth;

        NumParamVector _nodeNums;

        ParserState _state;

        CommandEntry _commands[SCPI_MAX_COMMANDS];
        uint8_t _commandCount;
        CommandEntry* _curCommand;

        ScpiErrorQueue _err;
    };

    bool cmpIChar(char a, char b);

} // SCPI
} // CTI

#endif //scpi_core_h_