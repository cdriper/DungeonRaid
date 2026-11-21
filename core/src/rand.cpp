// Random numbers, ported from the disassembly (the decompiler output hides the bit casts).
#include "rand.h"

#include <cmath>
#include <cstring>

#include "util.h"

uint32_t g_randSeeds[RAND_TYPE_COUNT];

namespace {
const uint32_t kMultiplier = 16807;   // 0x41a7 (Park-Miller "minimal standard", here mod 2^32)
}

// cmp r1,#6: above 6 does nothing, exactly 6 (RAND_TYPE_COUNT) sets all six streams.
void SetRandSeed(uint32_t seed, RandType type) {
    unsigned t = static_cast<unsigned>(type);
    if (t > RAND_TYPE_COUNT) {
        return;
    }
    if (t == RAND_TYPE_COUNT) {
        for (uint32_t& s : g_randSeeds) {
            s = seed;
        }
        return;
    }
    g_randSeeds[t] = seed;
}

uint32_t GetRandSeed(RandType type) {
    unsigned t = static_cast<unsigned>(type);
    return t < RAND_TYPE_COUNT ? g_randSeeds[t] : 0;
}

void FlushRandSeeds() {
    for (uint32_t& s : g_randSeeds) {
        s *= kMultiplier;
    }
}

// seed *= 16807 (mod 2^32); the top 22 bits of the new seed become the mantissa of a float in
// [2, 3) (bit pattern 0x40000000 | seed >> 10), minus 2.0, clamped to [0, 1]. The original
// stores the pattern to a global (0x00089ad0) and reloads it as a float; memcpy does the same.
float RandUnitF(uint32_t& seed) {
    seed *= kMultiplier;
    uint32_t bits = (seed >> 10) | 0x40000000u;
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return Clamp(f - 2.0f, 0.0f, 1.0f);
}

// min + (max - min) * u   (vsub, then vmla: rounded product, rounded sum)
float Rand(float min, float max, RandType type) {
    float range = max - min;
    float u = RandUnitF(g_randSeeds[type]);
    float product = range * u;
    return min + product;
}

// Inclusive integer range; the bounds may come in either order.
// floorf(base + count * u) (vmla), then truncation (vcvt.s32.f32).
int RandI(int min, int max, RandType type) {
    int base, count;
    if (max < min) {
        count = (min + 1) - max;
        base = max;
    } else {
        count = (max - min) + 1;
        base = min;
    }
    float u = RandUnitF(g_randSeeds[type]);
    float product = static_cast<float>(count) * u;
    float value = static_cast<float>(base) + product;
    // ARM's float->int conversion saturates (e.g. RandI(INT_MAX, INT_MAX): value = 2^31 -> INT_MAX)
    return FloatToIntArm(std::floor(value));
}
