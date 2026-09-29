#include "scpi/scpi_core.h"
#include "scpi/scpi_errors.h"

#include <ctype.h>

namespace CTI {
namespace SCPI {

#ifdef CTI_MINIMAL_ERROR_STRINGS
ScpiError noError {0, "No error"};
ScpiError overflowError {-350, "Overflow"};
#else
ScpiError noError {0, "No error"};
ScpiError overflowError {-350, "Error Queue Overflow"};
#endif

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

    int i = 0;
    while (str[i] != 0 && i < SCPI_ERROR_STR_SIZE - 1) {
        _err[_head].str[i] = str[i];
        ++i;
    }
    _err[_head].str[i] = 0;

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

bool ScpiErrorQueue::dequeue(int16_t* code, char ** str) {
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

    ScpiError* err = &_err[_tail];

    _tail++;
    if (_tail == _capacity) {
        _tail = 0;
    }

    if (_tail == _head) {
        _empty = true;
    }

    _full = false;

    *code = err->code;
    *str = err->str;

    return true;
}

void ScpiErrorQueue::clear() {
    _head = 0;
    _tail = 0;
    _empty = true;
    _full = false;
    _overflow = false;
}

char err_buffer[SCPI_ERR_BUFFER_SIZE + 1];

bool errCommand(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-100, "Command");
#else
    return scpi->enqueueError(-100, "Unspecified command error");
#endif
}

bool errInvalidChar(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-101, "Invalid character");
#else
    char c = scpi->curChar();

    if (isprint(c)) {
        snprintf(err_buffer, SCPI_ERR_BUFFER_SIZE, "Unexpected character %c at position %d", c, scpi->position());
    } else {
        snprintf(err_buffer, SCPI_ERR_BUFFER_SIZE, "Unexpected character #H%02X at position %d", c, scpi->position());
    }

    return scpi->enqueueError(-101, err_buffer);
#endif
}

bool errSyntax(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-102, "Syntax");
#else
    snprintf(err_buffer, SCPI_ERR_BUFFER_SIZE, "Unspecified syntax error at position %d", scpi->position());
    return scpi->enqueueError(-102, err_buffer);
#endif
}

bool errSeparator(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-103, "Separator");
#else
    snprintf(err_buffer, SCPI_ERR_BUFFER_SIZE, "Invalid separator %c at position %d", scpi->curChar(), scpi->position());
    return scpi->enqueueError(-103, err_buffer);
#endif
}

bool errParamType(ScpiParser* scpi, const char* expectedType) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-104, "Parameter type");
#else
    if (expectedType != nullptr) {
        snprintf(err_buffer, SCPI_ERR_BUFFER_SIZE, "Wrong parameter type at position %d, %s expected", scpi->position(), expectedType);
    } else {
        snprintf(err_buffer, SCPI_ERR_BUFFER_SIZE, "Wrong parameter type at position %d", scpi->position());
    }

    return scpi->enqueueError(-104, err_buffer);
#endif
}

bool errTooManyParams(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-108, "Too many params");
#else
    return scpi->enqueueError(-108, "Too many parameteres");
#endif
}

bool errMissingParam(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-109, "Missing param");
#else
    return scpi->enqueueError(-109, "Too few parameters");
#endif
}

bool errUndefinedHeader(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-113, "Unknown header");
#else
    return scpi->enqueueError(-113, "Undefined header");
#endif
}

bool errNoQuery(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(100, "No query");
#else
    return scpi->enqueueError(100, "Header recognized but can't be queried");
#endif
}

bool errNoCommand(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(101, "No command");
#else
    return scpi->enqueueError(101, "Header recognized but can't be commanded");
#endif
}

bool errSuffixOutOfRange(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-114, "Suffix range");
#else
    return scpi->enqueueError(-114, "Suffix for node is out of range");
#endif
}

bool errParamOutOfRange(ScpiParser* scpi) {
#ifdef CTI_MINIMAL_ERROR_STRINGS
    return scpi->enqueueError(-222, "Param range");
#else
    return scpi->enqueueError(-222, "Parameter value is out of range");
#endif
}

} // namnespace SCPI
} // namespace CTI