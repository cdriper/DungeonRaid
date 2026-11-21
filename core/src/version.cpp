#include "version.h"

#ifndef DR_VERSION
#error "DR_VERSION comes from the repo's VERSION file (core/CMakeLists.txt)"
#endif

const char* GetVersion() { return DR_VERSION; }

const char* GetBuildInfo() {
    return "v" DR_VERSION "  " __DATE__ " " __TIME__;
}
