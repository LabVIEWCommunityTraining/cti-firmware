#include "scpi/scpi_core.h"

#include <cstring>

namespace CTI {
namespace SCPI {

    ScpiNode::ScpiNode() {
        _nodeStr = nullptr;
        _strLen = 0;
        _reqLen = 0;
        _optional = false;
        _hasNum = false;
        _depth = 0;
        _parent = nullptr;
    }

    ScpiNode::ScpiNode(const char* nodeStr, uint8_t strLen, uint8_t depth,
            ScpiCommand cmdHandler, ScpiQuery queryHandler) {
        _nodeStr = nodeStr;
        _strLen = strLen;
        _depth = depth;
        _cmdHandler = cmdHandler;
        _queryHandler = queryHandler;
        _parent = nullptr;

        _hasNum = false;
        _optional = false;

        if (_nodeStr[0] == '[' && _nodeStr[_strLen - 1] == ']') {
            _optional = true;
            _nodeStr++;
            _strLen -= 2;
        }

        if (_nodeStr[_strLen - 1] == '#') {
            _hasNum = true;
            _strLen--;
        }

        // Count the number of capitalized characters in the string signifying the required portion
        _reqLen = 0;
        while (_reqLen < _strLen && _nodeStr[_reqLen] < 'a') {
            ++_reqLen;
        }
    }

    bool ScpiNode::matches(const char* candidate, uint8_t len) {
        uint8_t startNum = len; //default to end of string for terminal loop conditions later

        if (_hasNum) {
            //check for numeric at end of candidate string
            //also checks for '#' so can match against specifier syntax
            for (uint8_t i = len - 1; i >= 0; --i) {
                if ((candidate[i] > '9' || candidate[i] < '0') && candidate[i] != '#') {
                    break;
                }

                startNum = i; // track earliest found digit from end of candidate
            }
        }

        //string needs to either be same length as required portion or full string
        if (startNum != _reqLen && startNum != _strLen) {
            return false;
        }

        //iterate over required portion to check match, case-insensitively
        for (uint8_t i = 0; i < _reqLen; ++i) {
            //Test if lower-case (required nodeStr should be upper-case)
            if (candidate[i] >= 'a' && candidate[i] <= 'z') {
                if (candidate[i] - 32 != _nodeStr[i]) {
                    return false;
                }
            } else if (candidate[i] != _nodeStr[i]) {
                return false;
            }
        }

        if (len > _reqLen) {
            for (uint8_t i = _reqLen; i < startNum; ++i) {

                if (candidate[i] <= 'Z' && candidate[i] >= 'A') {
                    if (candidate[i] + 32 != _nodeStr[i]) {
                        return false;
                    }
                } else if (candidate[i] != _nodeStr[i]) {
                    return false;
                }
            }
        }

        //at this point we're the same non-number portion length and don't have a mismatch
        return true;
    }

    void ScpiNode::printNode(bool recurse) {
        if (recurse) {
            if (_parent != nullptr) {
                //print parent segments before current segment
                _parent->printNode(recurse);
            } else {
                //root node base case, nothing to print
                return;
            }

            gPlatform.IO.Print(':');
        }
 
        if (_optional) gPlatform.IO.Print('[');

        gPlatform.IO.Print(_strLen, _nodeStr);

        if (_hasNum)   gPlatform.IO.Print('#');
        if (_optional) gPlatform.IO.Print(']');
    }

    ScpiNode* ScpiNode::lookupChild(const char* str, uint8_t len) {
        for (ScpiNode* node : _children) {

            if (node->matches(str, len)) {
                return node;
            }
        }

        return nullptr;
    }

    int8_t ScpiNode::nodeNum(const char* str, uint8_t len) {
        if (!_hasNum) {
            return -1; // Node doesn't specify a num param
        }

        uint8_t numStart = len;

        for (uint8_t i = len - 1; i >= 0; --i) {
            if (str[i] > '9' || str[i] < '0') {
                break;
            }

            numStart = i;
        }

        if (numStart < len) {
            int8_t num = 0;
            for (uint8_t i = numStart; i < len; ++i) {
                int8_t digit = str[i] - '0';
                num *= 10;
                num += (digit);

                if (num < 0) {
                    return -3; //number was too big and rolled over
                }
            }

            return num;
        }

        return -2; //Num supported but not found
    }

    RegistrationResult ScpiNode::addChild(ScpiNode* child) {
        // When this level is optional, also register the child with the parent
        // level so this level can be "skipped". Parent level is registered
        // first as it has a higher chance of having a name collision that
        // can be checked for.
        if (_optional) {
            if (_parent == nullptr) {
                //first level node can't be optional
                return RegistrationResult::SyntaxError;
            }

            RegistrationResult res = _parent->addChild(child);

            if (res != RegistrationResult::Success) {
                return res;
            }
        }

        //Check to see if there's a name collision at this level.
        // this would be more likely when there's an optional level and the 
        // child node is being passed up to an ancestor for registration.
        for(ScpiNode* c : _children) {
            if (c->_strLen == _strLen) {
                if (std::strncmp(c->_nodeStr, _nodeStr, _strLen) == 0) {
                    return RegistrationResult::Ambiguous;
                }
            }
        }

        _children.push_back(child);
        child->_parent = this;

        // There are other result types but for now I'm assuming the node string
        // parsing code will handle those and only call addChild with a valid
        // 
        return RegistrationResult::Success;
    }
}
}