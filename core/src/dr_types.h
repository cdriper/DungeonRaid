// Basic types shared by the whole game core.
// Ported from libDungeonRaidJni.so (armeabi-v7a); addresses in comments are Ghidra addresses
// (image base 0x10000). The enums (original names and values) are generated into dr_enums.h.
#pragma once

#include <cstdint>

#include "dr_enums.h"

struct GLfloat2 {
    float x;
    float y;
};

struct GLcolour4 {
    uint8_t r, g, b, a;
};
