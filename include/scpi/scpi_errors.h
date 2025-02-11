#ifndef scpi_errors_h_
#define scpi_errors_h_

#include "scpi_core.h"

namespace CTI {
namespace SCPI {

#define SCPI_ERR_BUFFER_SIZE 255

bool errCommand(ScpiParser* scpi);

bool errInvalidChar(ScpiParser* scpi);

bool errSyntax(ScpiParser* scpi);

bool errSeparator(ScpiParser* scpi);

bool errParamType(ScpiParser* scpi, const char* expectedType = nullptr);

bool errTooManyParams(ScpiParser* scpi);

bool errMissingParam(ScpiParser* scpi);

bool errUndefinedHeader(ScpiParser* scpi);

bool errNoQuery(ScpiParser* scpi);

bool errNoCommand(ScpiParser* scpi);

bool errSuffixOutOfRange(ScpiParser* scpi);

bool errParamOutOfRange(ScpiParser* scpi);

}
}

#endif // scpi_errors_h_