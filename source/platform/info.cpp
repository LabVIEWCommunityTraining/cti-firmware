#include "cti/platform.h"

#include <stdio.h>
#include <string.h>

#include "cti/version.h"

CTI::PlatformInfo::PlatformInfo() {
}

const char* CTI::PlatformInfo::Vendor() const { return CTI_VENDOR; }

const char* CTI::PlatformInfo::Version() const { return CTI_VERSION_FULL; }