#include "scpi/scpi_core.h"

namespace CTI {
namespace SCPI {

ScpiError noError {0, "No error"};
ScpiError overflowError {-350, "Error Queue Overflow"};

ScpiErrorQueue::ScpiErrorQueue() {
    _capacity = SCPI_ERROR_QUEUE_SIZE;

    _head = 0;
    _tail = 0;

    _empty = true;
    _full = false;
    _overflow = false;
}

bool ScpiErrorQueue::enqueue(int16_t code, const char* str) {
    if (_full) {
        _overflow = true;
        return false;
    }

    _err[_head].code = code;
    _err[_head].str = str;

    _head++;
    if (_head == _capacity) {
        _head = 0;
    }

    if (_head == _tail) {
        _full = true;
    }

    _empty = false;

    return true;
}

bool ScpiErrorQueue::dequeue(int16_t* code, const char ** str) {
    if (_empty) {
        if (_overflow) {
            _overflow = false;
            *code = overflowError.code;
            *str = overflowError.str;
            return true;
        }

        *code = noError.code;
        *str = noError.str;
        return false;
    }

    ScpiError err = _err[_tail];

    _tail++;
    if (_tail == _capacity) {
        _tail = 0;
    }

    if (_tail == _head) {
        _empty = true;
    }

    _full = false;

    *code = err.code;
    *str = err.str;

    return true;
}

void ScpiErrorQueue::clear() {
    _head = 0;
    _tail = 0;
    _empty = true;
    _full = false;
    _overflow = false;
}

} // namnespace SCPI
} // namespace CTI