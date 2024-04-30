#include "scpi/scpi_core.h"

#include <cstdlib>
#include <cstring>

namespace CTI {
namespace SCPI {

    // ScpiChoice array terminator has a null string
    ScpiChoice EndScpiChoice { nullptr, 0 };

    ScpiParser::ScpiParser(int bufCapacity) {
        _buf = (char*)malloc(bufCapacity);
        _bufCapacity = bufCapacity;
        _bufSize = 0;

        _maxDepth = 0;

        _treeRoot = new ScpiNode();
        _curNode = nullptr;

        _state = ParserState::FindCommand;
        _paramPos = 0;
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
    }

    int ScpiParser::bufferInput(const char* data, int n) {
        for (int i = 0; i < n; ++i) {
            if (_bufSize >= _bufCapacity) {
                return i; //buffer full, return the count of data actually buffered.
            }

            _buf[_bufSize] = data[i];

            _bufSize++;

            if (_state == ParserState::FindCommand ) {
                if (data[i] == ' ' || data[i] == '\n') {
                    ParserStatus res = parseNode();

                    if (res != ParserStatus::Success) {
                        _state = ParserState::InvalidNode;
                    }
                    _bufSize = 0;
                }
            } else if (_state == ParserState::FindEndOfLine) {
                if (data[i] == '\n') {
                    ParserStatus res = invokeNode();
                    reset();
                }
            } else if (_state == ParserState::InvalidNode) {
                if (data[i] == '\n') {
                    _state = ParserState::FindCommand;
                    reset();
                }
            }
        }

        //made it to the end, able to buffer as much as provided
        return n;
    }

    ParseResult ScpiParser::parseChoice(const ScpiChoice* choices, uint8_t& value) {
        consumeWhiteSpace();

        uint8_t choice = 0;
        uint8_t cur;
        bool match = false;

        while (choices[choice].choiceString != nullptr && !match) {
            cur = 0;
            const char* str = choices[choice].choiceString;

            while (_paramPos + cur < _bufSize && str[cur] != 0) {
                if (_buf[_paramPos + cur] != str[cur]) {
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
    
    ParseResult ScpiParser::parseBlock(char* buf, int len) {
        consumeWhiteSpace();

        if (_buf[_paramPos] != '#') {
            return ParseResult::Invalid;
        }

        _paramPos++; // consume #
        uint8_t digitsLen = 0;
        if (_buf[_paramPos] <= '9' && _buf[_paramPos] >= '0') {
            digitsLen = _buf[_paramPos] - '0';
            _paramPos++;
        } else {
            return ParseResult::Invalid;
        }

        int dataLen = 0;
        for (int i = 0; i < digitsLen; ++i) {
            dataLen *= 10;
            dataLen += _buf[_paramPos] - '0';
            _paramPos++;
        }

        if (dataLen > len) {
            return ParseResult::InsufficientBuffer;
        }

        std::memcpy(buf, _buf, dataLen);

        if (!isEndOfParam()) {
            return ParseResult::Invalid;
        }

        return ParseResult::Success;
    }
    

    ParserStatus ScpiParser::parseNode() {
        //determine leaf node, split on ':' or end of string

        ScpiNode* node = _treeRoot;
        uint8_t cur = 0;
        uint8_t start = 0;
        uint8_t depth = 0;

        if (_buf[0] == ':') { //skip optional leading ':'
            cur = 1;
            start = 1;
        }

        for (; cur <= _bufSize; ++cur) {
            if (cur == _bufSize || _buf[cur] == ':' || _buf[cur] == '?' || _buf[cur] == ' ' || _buf[cur] == '\n') {
                //reached end of tree portion, perform lookup
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
            gPlatform.IO.Print("\ncurNode not set, aborting!\n\n");
            return ParserStatus::Unknown;
        }

        _paramPos = 0;

        if (_isQuery) {
            QueryResult res = _curNode->invokeQuery(this);

            if (res != QueryResult::Success) {
                if (res == QueryResult::NoHandler) {
                    return ParserStatus::UnknownCommand;
                }

                return ParserStatus::Unknown;
            }
        } else {
            CommandResult res = _curNode->invokeCommand(this);

            if (res != CommandResult::Success) {
                if (res == CommandResult::NoHandler) {
                    return ParserStatus::UnknownCommand;
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
        return false;
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

        uint8_t cur = 0;
        uint8_t start = 0;
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
                if (node == nullptr) {

                    node = new ScpiNode(str + start, cur - start, depth,
                        cmdHandler, queryHandler);
                    
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
} // namespace SCPI
} // namespace CTI