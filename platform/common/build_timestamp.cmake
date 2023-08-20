set(BUILD_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/build/build.h")

string(TIMESTAMP CTI_BUILD_DATE "%Y%m%d" UTC)
string(TIMESTAMP CTI_BUILD_TIME "%H%M%S" UTC)
set(CTI_BUILD_TS "${CTI_BUILD_DATE}T${CTI_BUILD_TIME}Z")

message ("\nSetting build timestamp to ${CTI_BUILD_TS}\n")

file(WRITE ${BUILD_HEADER} "//This file is automatically generated by the build system.\n#ifndef build_h_\n#define build_h_\n\n#define CTI_VER_TS ${CTI_BUILD_TS}\n\n#endif //build_h_")