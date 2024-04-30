#ifndef scpi_core_h_
#define scpi_core_h_

#include "cti/platform.h"

#include <cstdint>
#include <cstdlib>
#include <vector>

#ifndef SCPI_ERROR_QUEUE_SIZE
#define SCPI_ERROR_QUEUE_SIZE 10
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
        SyntaxError,
        NoHandler,
        Error
    };

    enum class QueryResult {
        Success,
        NoHandler,
        Error
    };

    enum class RegistrationResult {
        Success,
        SyntaxError,
        Ambiguous,
        InvalidHandler,
        AlreadyFinalized,
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

    class ScpiNode;
    class ScpiParser;

    class NumParamVector {
    public:
        NumParamVector() {
            _nums = nullptr;
            _count = 0;
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
            if (!_nums) {
                _nums = (int8_t*)malloc(size);
                _count = size;
                return true;
            }

            return false;
        }
    private:
        int8_t* _nums;
        uint8_t _count;
    };

    typedef struct {
        int16_t code;
        const char* str;
    } ScpiError;

    class ScpiErrorQueue {
    public:
        ScpiErrorQueue();
        
        bool enqueue(int16_t code, const char* str);
        bool dequeue(int16_t* code, const char ** str);

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

    class ScpiNode {
    public:
        ScpiNode();
        ScpiNode(const char* nodeString, uint8_t strLen, uint8_t depth, ScpiCommand cmdHandler, ScpiQuery queryHandler);
        
        RegistrationResult addChild(ScpiNode* node);

        bool matches (const char* candidate, uint8_t len);

        int8_t nodeNum (const char* str, uint8_t len);

        bool hasNum() {
            return _hasNum;
        }

        uint8_t depth() {
            return _depth;
        }

        ScpiNode* lookupChild(const char* str, uint8_t len);

        CommandResult invokeCommand(ScpiParser* parser) {
            if (_cmdHandler != nullptr) {
                return _cmdHandler(parser);
            }

            return CommandResult::NoHandler;
        };

        QueryResult invokeQuery(ScpiParser* parser) {
            if (_queryHandler != nullptr) {
                return _queryHandler(parser);
            }

            return QueryResult::NoHandler;
        };

        void printNode(bool recurse);

    private:
        const char* _nodeStr;
        uint8_t _strLen;
        
        uint8_t _reqLen;

        bool _hasNum;
        bool _optional;

        uint8_t _depth;

        ScpiNode* _parent;

        std::vector<ScpiNode*> _children;

        ScpiCommand _cmdHandler;

        ScpiQuery _queryHandler;
    };

    class ScpiParser {
    public:
        ScpiParser(int bufCapacity = 255);
        ~ScpiParser();

        void finalize();

        int bufferInput(const char* data, int n);

        RegistrationResult registerNode(const char* str, ScpiCommand cmdHandler, ScpiQuery queryHandler);

        ParseResult parseBool(bool& value);
        ParseResult parseBlock(char* buf, int len);
        ParseResult parseChoice(const ScpiChoice* choices, uint8_t& value);

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

        void reset();

        ScpiNode* curNode() {
            return _curNode;
        }

        ChanIndex nodeNum(uint8_t level) {
            return _nodeNums.get(level);
        }

    private:
        ParserStatus parseNode();
        ParserStatus invokeNode();

        void consumeWhiteSpace() {
            while (_paramPos < _bufSize &&
                (_buf[_paramPos] == ' ' || _buf[_paramPos] == '\t' || _buf[_paramPos] == '\r')) {

                _paramPos++;
            }
        }

        bool isEndOfParam() {
            char c = _buf[_paramPos];
            bool isEnd = (_paramPos == _bufSize || 
                c == ' ' ||
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
                _paramPos++;
                return NumberFormat::Dec;
            }

            if (_buf[_paramPos] == '-' || _buf[_paramPos] == '+') {
                _paramPos++;
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

            for (int i = 0; i < maxDigits & _paramPos < _bufSize; ++i) {
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

            for (int i = 0; i < maxDigits & _paramPos < _bufSize; ++i) {
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

            if (_buf[_paramPos] == '.') {
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

        char* _buf;
        int _bufSize;
        int _bufCapacity;

        uint8_t _paramPos;

        bool _finalized;

        bool _isQuery;

        uint8_t _maxDepth;

        ScpiNode* _treeRoot;

        NumParamVector _nodeNums;

        ParserState _state;

        ScpiNode* _curNode;

        ScpiErrorQueue _err;
    };

} // SCPI
} // CTI

#endif //scpi_core_h_