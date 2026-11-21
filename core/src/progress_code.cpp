#include "progress_code.h"

#include "character_class.h"
#include "game.h"
#include "game_state.h"

namespace progress {
namespace {

const char kPrefix[] = "DR2-";
const uint8_t kVersion = 2;
const int kClassCount = 10;
// One class in the cla file (CharacterClass::SerialiseFullState): race, perk/flaw of slots 1..3,
// replacement slots 1..19, level, games played, enemies killed, 3 best scores.
const size_t kRecordSize = 1 + 6 + 19 + 1 + 4 + 4 + 12;
const size_t kClassBytes = 2 + kClassCount * kRecordSize;
const size_t kPayloadSize = 1 + kClassBytes + 1 + Game::kAchievementBytes;   // version, classes, countdown, achievements
const size_t kCodeSize = kPayloadSize + 4;          // and the CRC

const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

uint32_t ReadU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
           static_cast<uint32_t>(p[3]) << 24;
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// The class records: the values must be ones the game can hold (enums in range or NONE).
Error ReadClasses(const uint8_t* p, Data& out) {
    if (p[0] != 0 || p[1] != kClassCount) return Error::BadValue;
    p += 2;
    for (int c = 0; c < kClassCount; ++c, p += kRecordSize) {
        if (p[0] >= RACE_COUNT) return Error::BadValue;
        for (int i = 0; i < 3; ++i) {
            const uint8_t perk = p[1 + 2 * i], flaw = p[2 + 2 * i];
            if (perk >= PERK_COUNT && perk != PERK_NONE) return Error::BadValue;
            if (flaw >= FLAW_COUNT && flaw != FLAW_NONE) return Error::BadValue;
        }
        for (int i = 0; i < 19; ++i) {
            const uint8_t skill = p[7 + i];
            if (skill >= SKILL_COUNT && skill != SKILL_NONE) return Error::BadValue;
        }
        ClassInfo& info = out.classes[c];
        info.level = p[26];
        if (info.level > 10) return Error::BadValue;
        info.gamesPlayed = static_cast<int>(ReadU32(p + 27));
        info.enemiesKilled = static_cast<int>(ReadU32(p + 31));
        if (info.gamesPlayed < 0 || info.enemiesKilled < 0) return Error::BadValue;
        for (int i = 0; i < 3; ++i) info.bestScore[i] = ReadU32(p + 35 + 4 * i);
    }
    return Error::None;
}

int CountUnlocked(const Game::AchievementRecord* records) {
    int n = 0;
    for (int a = 1; a < kAchievementCount; ++a) n += records[a].progress >= 100.0f;
    return n;
}

std::vector<uint8_t> SerialiseClasses(Game& game) {
    GameState state(0x400);
    state.ResetForWriting();
    state.WriteChar(0);
    state.WriteChar(kClassCount);
    for (CharacterClass& c : game.m_classes) c.SerialiseFullState(&state);
    return std::vector<uint8_t>(state.GetBuffer(), state.GetBuffer() + state.GetStateSize());
}

}  // namespace

const char* ErrorText(Error error) {
    switch (error) {
        case Error::None: return "";
        case Error::Empty: return "The clipboard is empty";
        case Error::NotACode: return "The clipboard holds no progress code";
        case Error::BadEncoding: return "The progress code is damaged (bad characters)";
        case Error::BadSize: return "The progress code is incomplete";
        case Error::BadVersion: return "The progress code is from another version";
        case Error::BadChecksum: return "The progress code is damaged (checksum)";
        case Error::BadValue: return "The progress code holds invalid values";
    }
    return "";
}

uint32_t Crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

std::string Base64Encode(const std::vector<uint8_t>& bytes) {
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const size_t n = bytes.size() - i < 3 ? bytes.size() - i : 3;
        uint32_t v = static_cast<uint32_t>(bytes[i]) << 16;
        if (n > 1) v |= static_cast<uint32_t>(bytes[i + 1]) << 8;
        if (n > 2) v |= bytes[i + 2];
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        out += n > 1 ? kAlphabet[(v >> 6) & 63] : '=';
        out += n > 2 ? kAlphabet[v & 63] : '=';
    }
    return out;
}

bool Base64Decode(const std::string& text, std::vector<uint8_t>& bytes) {
    bytes.clear();
    if (text.size() % 4 != 0) return false;
    for (size_t i = 0; i < text.size(); i += 4) {
        uint32_t v = 0;
        int pad = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = text[i + k];
            int d;
            if (c >= 'A' && c <= 'Z') d = c - 'A';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
            else if (c >= '0' && c <= '9') d = c - '0' + 52;
            else if (c == '+') d = 62;
            else if (c == '/') d = 63;
            else if (c == '=' && i + 4 == text.size() && k >= 2) d = -1;
            else return false;
            if (d < 0) {
                ++pad;
                d = 0;
            } else if (pad > 0) {
                return false;   // data after padding
            }
            v = (v << 6) | static_cast<uint32_t>(d);
        }
        bytes.push_back(static_cast<uint8_t>(v >> 16));
        if (pad < 2) bytes.push_back(static_cast<uint8_t>(v >> 8));
        if (pad < 1) bytes.push_back(static_cast<uint8_t>(v));
    }
    return true;
}

std::string Export(Game& game) {
    std::vector<uint8_t> payload;
    payload.push_back(kVersion);
    const std::vector<uint8_t> classes = SerialiseClasses(game);
    payload.insert(payload.end(), classes.begin(), classes.end());
    const int countdown = game.m_timeToTrophy < 0 ? 0 : (game.m_timeToTrophy > 30 ? 30 : game.m_timeToTrophy);
    payload.push_back(static_cast<uint8_t>(countdown));
    const std::vector<uint8_t> achievements = game.SerialiseAchievementRecords();
    payload.insert(payload.end(), achievements.begin(), achievements.end());
    const uint32_t crc = Crc32(payload.data(), payload.size());
    for (int i = 0; i < 4; ++i) payload.push_back(static_cast<uint8_t>(crc >> (8 * i)));
    return kPrefix + Base64Encode(payload);
}

Data Current(Game& game) {
    Data d;
    d.classBytes = SerialiseClasses(game);
    d.timeToTrophy = game.m_timeToTrophy;
    if (d.classBytes.size() == kClassBytes) ReadClasses(d.classBytes.data(), d);
    d.achievementBytes = game.SerialiseAchievementRecords();
    d.achievementsUnlocked = game.UnlockedAchievementCount();
    return d;
}

Error Parse(const std::string& text, Data& out) {
    std::string compact;
    for (char c : text) {
        if (!IsSpace(c)) compact += c;
    }
    if (compact.empty()) return Error::Empty;
    const size_t prefixLength = sizeof kPrefix - 1;
    if (compact.compare(0, prefixLength, kPrefix) != 0) {
        // another version's prefix ("DR2-"...) gets a clearer message than "no code"
        if (compact.size() > 4 && compact[0] == 'D' && compact[1] == 'R' && compact[3] == '-') return Error::BadVersion;
        return Error::NotACode;
    }
    std::vector<uint8_t> bytes;
    if (!Base64Decode(compact.substr(prefixLength), bytes)) return Error::BadEncoding;
    if (bytes.size() != kCodeSize) return Error::BadSize;
    if (ReadU32(bytes.data() + kPayloadSize) != Crc32(bytes.data(), kPayloadSize)) return Error::BadChecksum;
    if (bytes[0] != kVersion) return Error::BadVersion;
    Data d;
    const Error e = ReadClasses(bytes.data() + 1, d);
    if (e != Error::None) return e;
    d.timeToTrophy = bytes[1 + kClassBytes];
    if (d.timeToTrophy > 30) return Error::BadValue;
    const uint8_t* achievements = bytes.data() + 2 + kClassBytes;
    Game::AchievementRecord records[kAchievementCount];
    if (!Game::ParseAchievementRecords(achievements, Game::kAchievementBytes, records)) return Error::BadValue;
    d.achievementBytes.assign(achievements, achievements + Game::kAchievementBytes);
    d.achievementsUnlocked = CountUnlocked(records);
    d.classBytes.assign(bytes.begin() + 1, bytes.begin() + 1 + static_cast<std::ptrdiff_t>(kClassBytes));
    out = d;
    return Error::None;
}

void Apply(Game& game, const Data& data) {
    GameState& s = game.m_classState;
    s.ResetStates();
    for (uint8_t b : data.classBytes) s.WriteChar(b);
    game.DeserialiseClasses();   // Validate normalises the builds
    game.SerialiseClasses();     // marks the state changed: the host writes cla
    game.SetTimeToTrophy(data.timeToTrophy);
    game.m_timeToTrophyChanged = true;
    if (game.m_classes[game.m_currentClass].m_level == 0) game.m_currentClass = 0;   // as Game::Start
    game.m_nameScreen.SelectClass(game.m_classes[game.m_currentClass]);
    game.m_currentClassChanged = true;
    if (Game::ParseAchievementRecords(data.achievementBytes.data(), data.achievementBytes.size(), game.m_achievementRecords)) {
        game.m_achievementsChanged = true;   // the host writes ach
    }
}

void Reset(Game& game) {
    for (int c = 0; c < 10; ++c) {   // as Game::Start without a cla file
        game.m_classes[c] = CharacterClass();
        game.m_classes[c].Initialise(static_cast<ClassType>(c), &game);
    }
    game.SerialiseClasses();     // marks the state changed: the host writes cla
    game.m_currentClass = 0;
    game.m_nameScreen.SelectClass(game.m_classes[0]);
    game.m_currentClassChanged = true;
    game.SetTimeToTrophy(0);     // (as a first start: no saved countdown)
    game.m_timeToTrophyChanged = true;
    for (Game::AchievementRecord& r : game.m_achievementRecords) r = Game::AchievementRecord();
    game.m_achievementsChanged = true;
    ScoreScreen& scores = game.m_scoreScreen;
    for (int t = 0; t < ScoreScreen::kTypes; ++t) {
        scores.m_counts[t] = 0;
        for (ScoreScreen::Entry& e : scores.m_entries[t]) e = ScoreScreen::Entry();
        scores.m_changed[t] = true;   // the host writes the empty tables
    }
    scores.m_lastType = -1;
    scores.m_lastIndex = -1;
    game.m_saveState.ResetForWriting();   // empty and changed: the host removes the save
}

}  // namespace progress
