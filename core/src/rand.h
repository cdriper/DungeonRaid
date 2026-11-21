// Random numbers: six independent Park-Miller style streams (one per RandType).
// Bit-exact port of the original; see rand.cpp for the details.
#pragma once

#include <cstdint>

#include "dr_types.h"

extern uint32_t g_randSeeds[RAND_TYPE_COUNT];   // 0x00089ae0

void SetRandSeed(uint32_t seed, RandType type);  // 0x00067574; RAND_TYPE_COUNT seeds all streams
uint32_t GetRandSeed(RandType type);             // 0x000675c8; 0 for an invalid type
void FlushRandSeeds();                           // 0x000675a4
float RandUnitF(uint32_t& seed);                 // 0x00067610; [0, 1), advances the seed
float Rand(float min, float max, RandType type); // 0x00067650
int RandI(int min, int max, RandType type);      // 0x00067bf4; inclusive range
