// Small helpers, ported from the disassembly. The operation order matters for bit-exactness:
// every vmla is a rounded multiply followed by a rounded add.
#include "util.h"

#include <chrono>
#include <cmath>
#include <ctime>

// vcmpe value,max; bge -> max. Then value > min (or unordered) -> value, else min.
// So a NaN value comes back unchanged.
float Clamp(float value, float min, float max) {
    if (value >= max) {
        return max;
    }
    if (value <= min) {
        return min;
    }
    return value;
}

int ClampI(int value, int min, int max) {
    if (value >= max) {
        return max;
    }
    return min >= value ? min : value;
}

// s12 = 1 - t; s15 = s12 * b; s15 += a * t
float Interpolate(float a, float b, float t) {
    float result = (1.0f - t) * b;
    float at = a * t;
    return result + at;
}

// x = t * b.x; x += (1 - t) * a.x   (same for y)
GLfloat2 Interpolate(const GLfloat2& a, const GLfloat2& b, float t) {
    float s = 1.0f - t;
    float x = t * b.x;
    float y = t * b.y;
    float ax = s * a.x;
    float ay = s * a.y;
    return GLfloat2{x + ax, y + ay};
}

float Sqr(float x) {
    return x * x;
}

namespace {
int32_t DefaultClockMicros() {
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return static_cast<int32_t>(duration_cast<microseconds>(steady_clock::now() - start).count());
}

ClockSourceMicros g_clockSource = DefaultClockMicros;
}  // namespace

void SetClockSource(ClockSourceMicros source) {
    g_clockSource = source ? source : DefaultClockMicros;
}

// (double)clock() / 1000000.0
double GetTime() {
    return static_cast<double>(g_clockSource()) / 1000000.0;
}

// (float)(GetTime() - start)
float GetTimeSince(double start) {
    return static_cast<float>(GetTime() - start);
}

int FloatToIntArm(float value) {
    if (value != value) return 0;
    if (value >= 2147483648.0f) return 2147483647;
    if (value <= -2147483648.0f) return -2147483647 - 1;
    return static_cast<int>(value);
}

uint32_t FloatToUintArm(float value) {
    if (!(value > 0.0f)) return 0;   // NaN, negative, zero
    if (value >= 4294967296.0f) return 0xffffffffu;
    return static_cast<uint32_t>(value);
}

float CosF(float x) { return static_cast<float>(std::cos(static_cast<double>(x))); }
float SinF(float x) { return static_cast<float>(std::sin(static_cast<double>(x))); }

float PowF(float x, float y) { return static_cast<float>(std::pow(static_cast<double>(x), static_cast<double>(y))); }

namespace {
uint32_t DefaultTimeSource() { return static_cast<uint32_t>(std::time(nullptr)); }
TimeSourceSeconds g_timeSource = DefaultTimeSource;
}  // namespace

void SetTimeSource(TimeSourceSeconds source) { g_timeSource = source ? source : DefaultTimeSource; }
uint32_t GetUnixTime() { return g_timeSource(); }
