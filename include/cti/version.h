#ifndef version_h_
#define version_h_

//The version number can be editted in ../version.cmake

#include "build/build.h"
#include "common.h"

#define CTI_VER_SEP .

#define CTI_VERSION_TS CTI_STR(CTI_VER_TS)

#define CTI_VER CTI_CONCAT(CTI_VER_MAJOR, CTI_CONCAT(CTI_VER_SEP, CTI_CONCAT(CTI_VER_MINOR, CTI_CONCAT(CTI_VER_SEP, CTI_VER_PATCH))))
#define CTI_VERSION CTI_STR(CTI_VER)
#define CTI_VERSION_FULL CTI_STR(CTI_CONCAT(CTI_VER, CTI_CONCAT(CTI_VER_SEP, CTI_VER_TS)))

#endif // version_h_