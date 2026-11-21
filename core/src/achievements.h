// The achievements' titles, descriptions and points, indexed by Achievement (dr_enums.h).
// Generated from the original APK's OpenFeint offline config (res/raw/openfeint_offline_config.xml:
// title, description, points = gamerscore; none secret), matched to the enum by the Java shell's
// enum -> OpenFeint id table (DungeonRaidSurfaceView.mAchievements; the config's own order differs:
// Nemesis and Getting Crowded swap). The original kept these on the Java / OpenFeint side; the
// native code only queues an achievement with an amount (Game::QueueAchievement), which the shell
// added to the stored percentage, unlocking at 100.
#pragma once

#include "dr_enums.h"

struct AchievementInfo {
    const char* title;
    const char* description;
    int points;
};

constexpr int kAchievementCount = ACH_PRETZEL_LEGEND + 1;   // ACH_NONE included (entry 0 is empty)

extern const AchievementInfo g_achievements[kAchievementCount];
