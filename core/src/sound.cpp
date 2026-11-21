#include "sound.h"

#include <vector>

#include "rand.h"

namespace {
constexpr int kSoundCount = 38;   // 0x26

const char* const kSoundNames[kSoundCount] = {
    "click", "coinstart", "coin", "swordstart", "potion", "potionstart", "shield", "shieldstart",
    "up", "xp", "abilityready", "shop", "levelup", "upgrade", "skull", "armour", "armourbreak",
    "damage", "start", "ability", "freeze", "teleport", "shatter", "boost", "explosive", "slash",
    "attackmiss", "attackhit", "explosion", "poison", "blunt", "help", "heartbeat", "lose",
    "doorcreak", "jump", "bless", "quake",
};

// 0x0006e390 (read from the binary)
const int kSoundVariations[kSoundCount] = {
    1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 4, 1, 1, 1, 1, 1,
};

// 0x0006e428 (read from the binary; all equal to these float literals)
const float kSoundVolumes[kSoundCount] = {
    0.2f, 0.5f, 0.5f, 0.3f, 0.5f, 0.8f, 0.4f, 0.3f, 0.3f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.6f, 0.5f, 0.5f, 0.4f, 0.7f,
    0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.8f, 0.5f, 0.5f, 0.5f, 0.7f, 0.8f, 0.5f, 0.7f, 1.0f, 0.5f, 0.7f, 0.8f, 1.0f, 0.8f,
};

std::vector<std::vector<int>> g_soundMapping;   // g_soundMapping
}  // namespace

// resize(38): grows with empty lists, or drops the lists beyond 38 (none in practice).
void InitialiseSoundMappings() { g_soundMapping.resize(kSoundCount); }

// No bounds check in the original (the mappings must be initialised first).
void AddSoundMapping(int sound, int id) { g_soundMapping[sound].push_back(id); }

int GetSoundMapping(int sound) {
    const std::vector<int>& ids = g_soundMapping[sound];
    int n = static_cast<int>(ids.size());
    if (n == 0) return -1;
    return ids[RandI(0, n - 1, RAND_COSMETIC)];
}

const char* GetSoundName(int sound) {
    if (sound < 0 || sound >= kSoundCount) return "";
    return kSoundNames[sound];
}

int GetSoundVariations(int sound) {
    if (static_cast<unsigned>(sound) >= kSoundCount) return 0;
    return kSoundVariations[sound];
}

float GetSoundVolume(int sound) {
    if (static_cast<unsigned>(sound) >= kSoundCount) return 0.5f;
    return kSoundVolumes[sound];
}

int GetTotalSoundCount() {
    int total = 0;
    for (int i = 0; i < kSoundCount; ++i) total += GetSoundVariations(i);
    return total;
}
