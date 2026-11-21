// Small helpers used everywhere (clamping, interpolation, time).
#pragma once

#include <cstdint>

#include "dr_types.h"

float Clamp(float value, float min, float max);                 // 0x000675e0
int ClampI(int value, int min, int max);                        // 0x00067688
// Careful: returns a at t = 1 and b at t = 0 (the GLfloat2 overload is the other way round).
float Interpolate(float a, float b, float t);                   // 0x00067698
GLfloat2 Interpolate(const GLfloat2& a, const GLfloat2& b, float t);  // 0x000676c0; a at t = 0
float Sqr(float x);                                             // 0x000676fc

// Time in seconds. The original divides clock() by 1e6 (bionic's CLOCKS_PER_SEC), i.e. process
// CPU time. The port reads a microsecond counter through a replaceable source instead (the
// harness installs a fake one); by default a monotonic wall clock.
using ClockSourceMicros = int32_t (*)();
void SetClockSource(ClockSourceMicros source);                  // nullptr restores the default
double GetTime();                                               // 0x00067c70
float GetTimeSince(double start);                               // 0x00067c98

// float -> int as ARM's vcvt.s32.f32 does it: truncate toward zero, saturate out-of-range values
// to INT_MIN/INT_MAX, NaN -> 0. A plain C++ cast is undefined for those values (x86 gives
// INT_MIN). Not a function of the original; used wherever it converts values that can overflow.
int FloatToIntArm(float value);
// float -> unsigned as vcvt.u32.f32: truncate, negative and NaN -> 0, >= 2^32 -> 0xffffffff.
uint32_t FloatToUintArm(float value);
// powf as the harness's emulator computes it (in double, rounded to float). Bionic's powf is
// correctly rounded in practically all cases, which this matches; see native/README.md.
float PowF(float x, float y);
// cosf/sinf the same way (in double, rounded to float).
float CosF(float x);
float SinF(float x);

// time(nullptr) of the original (InitNewGame/Start seed the RNG with it), through a replaceable
// source (the harness installs a fixed one); by default std::time.
using TimeSourceSeconds = uint32_t (*)();
void SetTimeSource(TimeSourceSeconds source);                   // nullptr restores the default
uint32_t GetUnixTime();
