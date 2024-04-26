#ifndef scpi_core_h_
#define scpi_core_h_

#include <cstdint>
#include <vector>

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
        InvalidHandler
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

    class ScpiNode;
    class ScpiParser;

    typedef std::vector<int8_t> NumParamVector;

    typedef CommandResult (*ScpiCommand)(ScpiNode* node, ScpiParser* parser, NumParamVector nodeNumbers);
    typedef QueryResult (*ScpiQuery)(ScpiNode* node, NumParamVector nodeNumbers);

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

        CommandResult invokeCommand(ScpiParser* parser, const NumParamVector& nodeNumbers) {
            if (_cmdHandler != nullptr) {
                return _cmdHandler(this, parser, nodeNumbers);
            }

            return CommandResult::NoHandler;
        };

        QueryResult invokeQuery(ScpiParser* parser, const NumParamVector& nodeNumbers) {
            if (_queryHandler != nullptr) {
                return _queryHandler(this, nodeNumbers);
            }

            return QueryResult::NoHandler;
        };

    private:
        /// @brief The original segment of the node string at this level as passed into the registration.
        const char* _nodeStr;
        uint8_t _strLen;
        
        /// @brief The number of characters that are the required portion of the string. Once beyond that, the entire
        /// remaining string must be matched. The node string match check always ignores the case of the character.
        uint8_t _reqLen;

        /// @brief Signifies whether the node segment ended with '#' and should track a number param
        bool _hasNum;

        /// @brief Whether the node at this level is option (was surrounded in '[]' during registration).
        bool _optional;

        uint8_t _depth;

        /// @brief Pointer to the parent level of the tree this node is nested under.
        /// Will be nullptr for the root node.
        ScpiNode* _parent;

        /// @brief  List of pointers to children nodes. Will be empty for leaf nodes.
        std::vector<ScpiNode*> _children;

        ScpiCommand _cmdHandler;

        ScpiQuery _queryHandler;
    };

    class ScpiParser {
    public:
        ScpiParser(int bufCapacity = 255);
        ~ScpiParser();

        void finalize();

        /// @brief Buffers new input characters. As soon as a query/command terminator is seen (space) a handler will be invoked if a match is found.
        /// @param data Buffer of characters to add to the parser.
        /// @param n The count of characters contained in data to buffer.
        /// @return The count of characters actually added, anything less than n means the buffer is out of room.
        int bufferInput(const char* data, int n);

        /// @brief Registers a new pair of command/query handler(s) to a SCPI tree string.
        /// @param str The null-terminated string to match against following the SCPI specifier syntax.
        /// @param cmdHandler The handler to invoke when the command is matched, specify nullptr to disable.
        /// @param queryHandler The handler to invoke when the query is matched, specify nullptr to disable.
        /// @return 
        RegistrationResult registerNode(const char* str, ScpiCommand cmdHandler, ScpiQuery queryHandler);

        ParseResult parseBool(bool& value);
        ParseResult parseU8(uint8_t& value);
        ParseResult parseI8(int8_t& value);
        ParseResult parseU16(uint16_t& value);
        ParseResult parseI16(int16_t& value);
        ParseResult parseU32(uint32_t& value);
        ParseResult parseI32(int32_t& value);
        ParseResult parseFloat(float& value);
        ParseResult parseDouble(double& value);
        ParseResult parseBlock(char* buf, int len);

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
            return (_paramPos == _bufSize || 
                _buf[_paramPos] == ' ' ||
                _buf[_paramPos] == '\n' ||
                _buf[_paramPos] == '\t' ||
                _buf[_paramPos] == '\r'
            );
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
        ParseResult parseInt(T& val, uint8_t maxDigits, bool sign = false) {
            consumeWhiteSpace();

            NumberFormat format = numberFormat();

            switch (format) {
                case NumberFormat::Binary:
                    return parseBinary(val, maxDigits, sign);

                case NumberFormat::Dec:
                    return parseDec(val, maxDigits);

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
        ParseResult parseReal(T& val) {
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

            if (!isEndOfParam()) {
                return ParseResult::Invalid;
            }

            return ParseResult::Success;
        }

        char* _buf;
        int _bufSize;
        int _bufCapacity;

        /// @brief Tracks the current position in the buffer as params are extracted
        uint8_t _paramPos;

        bool _finalized;

        bool _isQuery;

        uint8_t _maxDepth;

        ScpiNode* _treeRoot;

        NumParamVector _nodeNums;

        ParserState _state;

        ScpiNode* _curNode;
    };

} // SCPI
} // CTI

#endif //scpi_core_h_