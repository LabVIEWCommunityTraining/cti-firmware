#include "scpi/scpi_core.h"

#include <cstdlib>

namespace CTI {
namespace SCPI {

    ParserStatus ScpiParser::parseNode() {
        ScpiNode* curNode = _treeRoot;

        int bufPos = _bufHead;

        while (bufPos != _bufTail) {

        }

        return ParserStatus::Unknown;
    }

    ScpiParser::ScpiParser(int bufSize) {
        _buf = (uint8_t*)malloc(bufSize);
        _bufHead = 0;
        _bufTail = 0;
        _bufSize = bufSize;

        _state = ParserState::FindCommand;
    }

    ScpiParser::~ScpiParser() {
        if (_buf) {
            free(_buf);
        }
    }

    int ScpiParser::bufferInput(uint8_t* data, int n) {
        if (_bufTail == _bufHead) {
            return 0; // No data buffered, out of room
        }

        for (int i = 0; i < n; ++i) {
            _buf[_bufTail++] = data[i];

            if (_bufTail == _bufSize) {
                _bufTail = 0;
            }

            if (_bufTail == _bufHead) {
                return i + 1;
            }

            if (data[n] == ' ') {
                parseNode();
            }
        }
    }
    
    RegistrationResult ScpiParser::addScpiNode(std::string_view nodeString, ScpiCommand cmdHandler, ScpiQuery queryHandler) {

    }
}
}