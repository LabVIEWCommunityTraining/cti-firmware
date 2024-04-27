#include "scpi/scpi_core.h"

#include <cstdlib>
#include <cstring>

namespace CTI {
namespace SCPI {

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

    int ScpiParser::bufferInput(const char* data, int n) {
        for (int i = 0; i < n; ++i) {
            uint8_t j = i + _bufSize;
            if (j >= _bufCapacity) {
                return i; //buffer full, return the count of data actually buffered.
            }

            _buf[j] = data[i];

            if (_state == ParserState::FindCommand ) {
                if (data[n] == ' ' || '\n') {
                    ParserStatus res = parseNode();
                }
            } else if (_state == ParserState::FindEndOfLine) {
                if (data[n] == '\n') {
                    ParserStatus res = invokeNode();
                }
            }
        }
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

        if (_buf[0] == ':') {
            cur = 1;
            start = 1;
        }
        for (; cur <= _bufSize; ++cur) {
            if (cur == _bufSize || _buf[cur] == ':' || _buf[cur] == '?' || _buf[cur] == ' ') {
                ScpiNode* child = node->lookupChild(_buf + cur, cur - start);

                if (child == nullptr) {
                    return ParserStatus::UnknownCommand;
                }

                _nodeNums[depth] = child->nodeNum(_buf + cur, cur - start);
                depth++;

                start = cur;

                if (cur == _bufSize) {
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
            }
        }

        //Got to the end and didn't find a leaf node match, uh oh!
        return ParserStatus::Incomplete;
    }

    ParserStatus ScpiParser::invokeNode() {
        if (_curNode == nullptr) {
            return ParserStatus::Unknown;
        }

        if (_isQuery) {
            QueryResult res = _curNode->invokeQuery(this, _nodeNums);

            if (res != QueryResult::Success) {
                if (res == QueryResult::NoHandler) {
                    return ParserStatus::UnknownCommand;
                }

                return ParserStatus::Unknown;
            }
        } else {
            CommandResult res = _curNode->invokeCommand(this, _nodeNums);

            if (res != CommandResult::Success) {
                if (res == CommandResult::NoHandler) {
                    return ParserStatus::UnknownCommand;
                }

                return ParserStatus::Unknown;
            }
        }

        return ParserStatus::Success;
    }

    void ScpiParser::finalize() {
        _finalized = true;
        _nodeNums.reserve(_maxDepth);

        for (uint8_t i = 0; i < _maxDepth; ++i) {
            _nodeNums[i] = 0;
        }
    }
    
    RegistrationResult ScpiParser::registerNode(const char* str, ScpiCommand cmdHandler, ScpiQuery queryHandler) {
        int len = 0;
        while (len < _bufCapacity && str[len] != 0) {
            len++;
        }

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
                ScpiNode* node = new ScpiNode(str + cur, cur - start, depth,
                    cmdHandler, queryHandler);
                
                RegistrationResult res = curNode->addChild(node);

                if (res != RegistrationResult::Success) {
                    return res;
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
            if (_finalized) {
                finalize();
            }
        }
    }
} // namespace SCPI
} // namespace CTI