#include "scpi/scpi_core.h"
#include "scpi/scpi_errors.h"

#include <cstdlib>
#include <cstring>

namespace CTI {
namespace SCPI {

    // ScpiChoice array terminator has a null string
    ScpiChoice EndScpiChoice { nullptr, 0 };

    ScpiParser::ScpiParser(int bufCapacity) {
        _buf = (char*)calloc(bufCapacity + 1, 1);
        _bufCapacity = bufCapacity;
        _bufSize = 0;

        _maxDepth = 0;

        _treeRoot = new ScpiNode();
        reset();
    }

    ScpiParser::~ScpiParser() {
        if (_buf) {
            free(_buf);
        }
    }

    void ScpiParser::reset() {
        _bufSize = 0;
        _state = ParserState::FindCommand;
        _curNode = nullptr;
        _paramPos = 0;
        _isQuery = false;
        _blockDigits = _blockLength = _blockRemaining = 0;
        _afterHash = _discard = false;
        _buf[0] = 0;
    }

    int ScpiParser::bufferInput(const char* data, int n) {
        for (int i = 0; i < n; ++i) {
            if (_bufSize >= _bufCapacity && !_discard) {
                enqueueError(-363, "Input buffer overflow; message discarded");
                _discard = true;
            }

            if (!_discard) {
                _buf[_bufSize++] = data[i];
                _buf[_bufSize] = 0;
            }

            //Parse the tree portion separately from the full message so the
            //buffer can be reset and fully used for any supplied parameters.
            //This lets the command tree length not eat into available parameter
            //length.
            if (_state == ParserState::FindCommand ) {
                if (data[i] == ' ' || data[i] == '?' || data[i] == '\n') {
                    ParserStatus res = _discard ? ParserStatus::SyntaxError : parseNode();

                    if (res != ParserStatus::Success) {
                        if (!_discard) errUndefinedHeader(this);
                        _state = ParserState::InvalidNode;
                        _discard = true;
                    }

                    _bufSize = 0;

                    if (data[i] == '\n') {
                        if (_state != ParserState::InvalidNode) invokeNode();
                        reset();
                    }
                }
            } else if (_state == ParserState::FindEndOfLine || _state == ParserState::InvalidNode) {
                // A definite-length block can contain every byte, including LF.
                if (_blockRemaining > 0) {
                    --_blockRemaining;
                    continue;
                }
                if (_blockDigits > 0) {
                    if (data[i] < '0' || data[i] > '9') {
                        if (!_discard) enqueueError(-161, "Invalid binary block length");
                        _discard = true;
                        _blockDigits = 0;
                    } else {
                        _blockLength = _blockLength * 10 + data[i] - '0';
                        if (--_blockDigits == 0) {
                            _blockRemaining = _blockLength;
                            if (_blockLength > _bufCapacity - _bufSize && !_discard) {
                                enqueueError(-223, "Binary block exceeds input buffer");
                                _discard = true;
                            }
                        }
                        continue;
                    }
                } else if (_afterHash) {
                    _afterHash = false;
                    if (data[i] >= '1' && data[i] <= '9') {
                        _blockDigits = data[i] - '0';
                        _blockLength = 0;
                        continue;
                    }
                    if (data[i] == '0') {
                        if (!_discard) enqueueError(-161, "Indefinite binary blocks are not supported");
                        _discard = true;
                    }
                } else if (data[i] == '#') {
                    _afterHash = true;
                }
                if (data[i] == '\n') {
                    if (!_discard) invokeNode();
                    reset();
                }
            }
        }

        //made it to the end, able to buffer as much as provided
        return n;
    }

    ParseResult ScpiParser::parseChoice(const ScpiChoice* choices, int32_t& value) {
        consumeWhiteSpace();

        int32_t choice = 0;
        uint8_t cur;
        bool match = false;

        while (choices[choice].choiceString != nullptr && !match) {
            cur = 0;
            const char* str = choices[choice].choiceString;

            while (_paramPos + cur < _bufSize && str[cur] != 0) {
                if (!cmpIChar(_buf[_paramPos + cur], str[cur])) {
                    choice++; //mismatch, try next choice
                    break;
                }

                cur++;
            }

            if (str[cur] == 0) {
                //made it to the end without a mismatch, we've got a winner
                match = true;
                _paramPos += cur;
            }
        }

        if (!match || !isEndOfParam()) {
            return ParseResult::Invalid;
        }

        value = choices[choice].value;

        return ParseResult::Success;
    }

    ParseResult ScpiParser::parseBool(bool& value) {
        consumeWhiteSpace();

        if (_buf[_paramPos] == '0') {
            value = false;
            _paramPos++;
        } else if (_buf[_paramPos] == '1') {
            
            value = true;
            _paramPos++;
        } else if (_buf[_paramPos] == 'O' || _buf[_paramPos] == 'o') {
            //on or off, case-insensitive
            _paramPos++;
            if (_paramPos == _bufSize) {
                return ParseResult::EndOfData;
            }

            if (_buf[_paramPos] == 'N' || _buf[_paramPos] == 'n') {
                value = true;
                _paramPos++;
            } else {
                if (_buf[_paramPos] == 'F' || _buf[_paramPos] == 'f') {
                    _paramPos++;
                    if (_paramPos == _bufSize) {
                        return ParseResult::EndOfData;
                    }

                    if (_buf[_paramPos] == 'F' || _buf[_paramPos] == 'f') {
                        value = true;
                        _paramPos++;
                    }
                }
            }
        }

        // Ensure param is separated from next param, not mispelled, or at end of string
        if (!isEndOfParam()) {
            return ParseResult::Invalid;
        }

        return ParseResult::Success;
    }
    
    ParseResult ScpiParser::parseBlock(char** buf, int* len) {
        consumeWhiteSpace();
        *buf = nullptr;
        *len = 0;
        if (_paramPos + 2 > _bufSize || _buf[_paramPos] != '#') {
            return ParseResult::Invalid;
        }
        
        _paramPos++; // consume #

        uint8_t digitsLen = 0;
        if (_buf[_paramPos] <= '9' && _buf[_paramPos] >= '1') {
            digitsLen = _buf[_paramPos] - '0';
            _paramPos++;
        } else {
            return ParseResult::Invalid;
        }

        int dataLen = 0;
        if (digitsLen > _bufSize - _paramPos) return ParseResult::EndOfData;
        for (int i = 0; i < digitsLen; ++i) {
            if (_buf[_paramPos] < '0' || _buf[_paramPos] > '9') return ParseResult::Invalid;
            dataLen *= 10;
            dataLen += _buf[_paramPos] - '0';
            _paramPos++;
        }
        if (dataLen > _bufSize - _paramPos) return ParseResult::EndOfData;

        //point buffer at visa read buffer, data will remain valid while
        //command/query is invoked. Caller just needs to behave themselves
        //but this means no extra copies or memory space needed for buffer.
        char* start = _buf + _paramPos;

        //move parse location past end of block
        _paramPos += dataLen;

        if (!isEndOfParam()) {
            return ParseResult::Invalid;
        }
        *buf = start;
        *len = dataLen;

        return ParseResult::Success;
    }
    

    ParserStatus ScpiParser::parseNode() {
        //determine leaf node, split on ':' or end of string

        ScpiNode* node = _treeRoot;
        int cur = 0;
        int start = 0;
        uint8_t depth = 0;

        if (_buf[0] == ':') { //skip optional leading ':'
            cur = 1;
            start = 1;
        }

        for (; cur <= _bufSize; ++cur) {
            if (cur == _bufSize || _buf[cur] == ':' || _buf[cur] == '?' || _buf[cur] == ' ' || _buf[cur] == '\n') {
                //reached end of tree portion, perform lookup
                if (cur - start > 255) return ParserStatus::SyntaxError;
                ScpiNode* child = node->lookupChild(_buf + start, cur - start);

                if (child == nullptr) {
                    return ParserStatus::UnknownCommand;
                }

                int8_t num = child->nodeNum(_buf + start, cur - start);
                _nodeNums.set(depth, num);
                depth++;

                if (_buf[cur] == ':') {
                    cur++;
                }

                if (cur == _bufSize || _buf[cur] == ' ' || _buf[cur] == '\n') {
                    _bufSize = 0;
                    _curNode = child;
                    _state = ParserState::FindEndOfLine;

                    return ParserStatus::Success;
                }

                if (_buf[cur] == '?') {
                    _isQuery = true;
                    _bufSize = 0;
                    _curNode = child;
                    _state = ParserState::FindEndOfLine;

                    return ParserStatus::Success;
                }
                
                start = cur;
                node = child;
            }
        }

        //Got to the end and didn't find a leaf node match, uh oh!
        return ParserStatus::Incomplete;
    }

    ParserStatus ScpiParser::invokeNode() {
        if (_curNode == nullptr) {
            errUndefinedHeader(this);
            return ParserStatus::Unknown;
        }

        _paramPos = 0;

        if (_isQuery) {
            QueryResult res = _curNode->invokeQuery(this);

            if (res != QueryResult::Success) {
                if (res == QueryResult::NoHandler) {
                    errNoQuery(this);
                } else if (res == QueryResult::MissingParam) {
                    errMissingParam(this);
                } else if (res == QueryResult::UnexpectedParam) {
                    errTooManyParams(this);
                }

                return ParserStatus::Unknown;
            }
        } else {
            CommandResult res = _curNode->invokeCommand(this);

            if (res != CommandResult::Success) {
                if (res == CommandResult::NoHandler) {
                    errNoCommand(this);
                } else if (res == CommandResult::MissingParam) {
                    errMissingParam(this);
                } else if (res == CommandResult::UnexpectedParam) {
                    errTooManyParams(this);
                } else if (res == CommandResult::SyntaxError) {
                    errSyntax(this);
                }

                return ParserStatus::Unknown;
            }
        }

        _bufSize = 0;
        _curNode = nullptr;
        _isQuery = false;
        _state = ParserState::FindCommand;

        return ParserStatus::Success;
    }

    bool ScpiParser::enqueueError(int16_t code, const char* str) {
        return _err.enqueue(code, str);
    }

    void ScpiParser::finalize() {
        _finalized = true;
        _nodeNums.reserve(_maxDepth);

        for (uint8_t i = 0; i < _maxDepth; ++i) {
            _nodeNums.set(i, 0);
        }
    }
    
    RegistrationResult ScpiParser::registerNode(const char* str, ScpiCommand cmdHandler, ScpiQuery queryHandler) {
        if (_finalized) {
            return RegistrationResult::AlreadyFinalized;
        }

        int len = 0;
        while (len < _bufCapacity && str[len] != 0) {
            len++;
        }

        len++; // account for null terminator

        if (len > 255 || len < 2) return RegistrationResult::SyntaxError;
        int cur = 0;
        int start = 0;
        uint8_t depth = 0;
        ScpiNode* curNode = _treeRoot;

        //check if specifier starts with root ':' and skip if so
        if (str[0] == ':') {
            cur = 1;
            start = 1;
        }

        for (;cur < len; ++cur) {
            //Split up string by ':' or null-terminator
            if (str[cur] == ':' || str[cur] == 0) {

                //First check to see if node already has that child
                ScpiNode* node = curNode->lookupChild(str + start, cur - start);
                if (node != nullptr && str[cur] == 0) return RegistrationResult::Ambiguous;
                if (node == nullptr) {

                    bool leaf = str[cur] == 0;
                    node = new ScpiNode(str + start, cur - start, depth,
                        leaf ? cmdHandler : nullptr, leaf ? queryHandler : nullptr);
                    
                    RegistrationResult res = curNode->addChild(node);

                    if (res != RegistrationResult::Success) {
                        return res;
                    }
                }
                
                curNode = node;
                
                start = cur + 1;
                depth++;

                if (str[cur] == 0) {
                    break;
                }
            }
        }

        if (depth > _maxDepth) {
            _maxDepth = depth;
        }

        return RegistrationResult::Success;
    }

    bool cmpIChar(char a, char b) {
        //make sure a is smaller value (upper case if different)
        if (a > b) {
            char t = a;
            a = b;
            b = t;
        }

        bool aUCase = (a <= 'Z' && a >= 'A');
        bool bUCase = (b <= 'Z' && b >= 'A');

        if (aUCase != bUCase) {
            //not the same case, a must be upper case and b must be lower case
            b -= 32;
        }

        return a == b;
    }

} // namespace SCPI
} // namespace CTI
