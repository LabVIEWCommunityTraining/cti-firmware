#ifndef cti_mode_set
#error "cti.h should not be included directly. Instead \
include at least one of cti/visa.h, cti/udaq.h, or cti/native.h \
to create firmware providing a specific mode."
#endif

#ifndef cti_h_
#define cti_h_

#define CTI_PLATFORM CTI_STR(_CTI_PLATFORM)
#define CTI_BOARD CTI_STR(_CTI_BOARD)

#include "cti/version.h"
#include "cti/platform.h"

#endif //cti_h_