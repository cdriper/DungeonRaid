// Game, port-only: the achievements' progress, kept by the port (the original's Java shell kept it
// on OpenFeint). The core queues an achievement with an amount (QueueAchievement); the host drains
// the queue after each frame (ProcessAchievementQueue), as the Java shell did: the amount is added to
// the stored percentage, and at 100 the achievement is unlocked (capped, dated, a toast); an
// unlocked one takes no more. The host saves the records (file "ach") when they change.
#include <cstdio>
#include <string>

#include "game.h"
#include "util.h"

namespace {
const uint8_t kFileMagic[3] = {'A', 'C', 'H'};
const uint8_t kFileVersion = 1;

void PutU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

uint32_t GetU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
           static_cast<uint32_t>(p[3]) << 24;
}
}  // namespace

bool Game::IsAchievementUnlocked(int achievement) const {
    return achievement > ACH_NONE && achievement < kAchievementCount && m_achievementRecords[achievement].progress >= 100.0f;
}

int Game::UnlockedAchievementCount() const {
    int n = 0;
    for (int a = 1; a < kAchievementCount; ++a) n += IsAchievementUnlocked(a);
    return n;
}

void Game::AddAchievementProgress(int achievement, float amount) {
    if (achievement <= ACH_NONE || achievement >= kAchievementCount || IsAchievementUnlocked(achievement)) return;
    AchievementRecord& r = m_achievementRecords[achievement];
    r.progress += amount;
    if (r.progress >= 100.0f) {
        r.progress = 100.0f;
        r.time = GetUnixTime();
        m_toast.Queue(std::string("Achievement unlocked: ") + g_achievements[achievement].title);
    } else if (r.progress < 0.0f) {
        r.progress = 0.0f;
    }
    m_achievementsChanged = true;
}

void Game::ProcessAchievementQueue() {
    float amount = 0.0f;
    for (Achievement a = GetNextAchievement(amount); a != ACH_NONE; a = GetNextAchievement(amount)) {
        AddAchievementProgress(a, amount);
    }
}

// Debug mode: the first locked achievement (in the list's order) unlocks, as if earned.
void Game::DebugAddAchievement() {
    if (!m_debugMode) return;
    for (int a = 1; a < kAchievementCount; ++a) {
        if (IsAchievementUnlocked(a)) continue;
        AddAchievementProgress(a, 100.0f);
        return;
    }
    ShowToast("All achievements are unlocked");
}

// The records as bytes (the "ach" file and the progress code share it): the count, then per
// achievement its whole percent (0..100) and the unlock time (Unix seconds; 0 while locked).
std::vector<uint8_t> Game::SerialiseAchievementRecords() const {
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(kAchievementCount - 1));
    for (int a = 1; a < kAchievementCount; ++a) {
        const AchievementRecord& r = m_achievementRecords[a];
        out.push_back(static_cast<uint8_t>(r.progress >= 100.0f ? 100 : static_cast<int>(r.progress)));
        PutU32(out, r.progress >= 100.0f ? r.time : 0u);
    }
    return out;
}

bool Game::ParseAchievementRecords(const uint8_t* p, size_t size, AchievementRecord* out) {
    if (size != kAchievementBytes || p[0] != kAchievementCount - 1) return false;
    out[0] = AchievementRecord();
    for (int a = 1; a < kAchievementCount; ++a) {
        const uint8_t* e = p + 1 + static_cast<size_t>(a - 1) * 5;
        const uint32_t time = GetU32(e + 1);
        if (e[0] > 100 || (e[0] == 100) != (time != 0)) return false;   // dated exactly when unlocked
        out[a].progress = static_cast<float>(e[0]);
        out[a].time = time;
    }
    return true;
}

std::vector<uint8_t> Game::SaveAchievements() const {
    std::vector<uint8_t> out(kFileMagic, kFileMagic + 3);
    out.push_back(kFileVersion);
    const std::vector<uint8_t> records = SerialiseAchievementRecords();
    out.insert(out.end(), records.begin(), records.end());
    return out;
}

bool Game::LoadAchievements(const std::vector<uint8_t>& bytes) {
    AchievementRecord records[kAchievementCount];
    if (bytes.size() < 4 || bytes[0] != kFileMagic[0] || bytes[1] != kFileMagic[1] || bytes[2] != kFileMagic[2] ||
        bytes[3] != kFileVersion || !ParseAchievementRecords(bytes.data() + 4, bytes.size() - 4, records)) {
        return false;
    }
    for (int a = 0; a < kAchievementCount; ++a) m_achievementRecords[a] = records[a];
    return true;
}
