// Progress codes (port-only, not in the original): the meta-progression as text, to move it between
// devices through the clipboard (the settings screen's Export / Import rows). It holds the 10
// classes' records exactly as the cla file stores them (level, race, perks and flaws, replaced
// skills, games played, enemies killed, best scores), the trophy countdown and the achievements
// (Game::SerialiseAchievementRecords: 77, then per achievement its percent and unlock time):
//
//   "DR2-" + base64( [version 2] [0] [10] [10 records of 47 bytes] [countdown 0..30]
//                    [77] [77 x (percent 0..100, Unix time u32 LE)] [CRC-32, LE] )
//
// Version 1 codes ("DR1-", without the achievements) are not accepted (another version).
// The CRC-32 (zlib's) covers every byte before it. Parse accepts whitespace anywhere (codes
// pasted through chats get wrapped) and checks the prefix, the base64, the size, the version, the
// CRC and that every value is in range before anything is applied.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Game;

namespace progress {

enum class Error { None, Empty, NotACode, BadEncoding, BadSize, BadVersion, BadChecksum, BadValue };

// A short message for a toast.
const char* ErrorText(Error error);

struct ClassInfo {
    int level = 0;
    int gamesPlayed = 0;
    int enemiesKilled = 0;
    unsigned bestScore[3] = {0, 0, 0};   // Normal, Hard, Harder
};

struct Data {
    std::vector<uint8_t> classBytes;   // what the cla file holds: 0, 10, the 10 records
    int timeToTrophy = 0;
    ClassInfo classes[10];
    std::vector<uint8_t> achievementBytes;   // Game::SerialiseAchievementRecords
    int achievementsUnlocked = 0;
};

std::string Export(Game& game);                    // the game's current progress as a code
Data Current(Game& game);                          // the game's current progress (for a comparison)
Error Parse(const std::string& text, Data& out);   // out is filled only when the result is None
void Apply(Game& game, const Data& data);          // replaces the classes, the countdown, the achievements; the host saves them
// Wipes the progress as on a first start: the classes (levels, builds, games, kills, best scores),
// the trophy countdown, the achievements, the score tables and the saved game. The settings and the
// player's name stay. The host writes (or removes) the files.
void Reset(Game& game);

// The building blocks (also used by the tests).
uint32_t Crc32(const uint8_t* data, size_t size);
std::string Base64Encode(const std::vector<uint8_t>& bytes);
bool Base64Decode(const std::string& text, std::vector<uint8_t>& bytes);   // strict: no whitespace

}  // namespace progress
