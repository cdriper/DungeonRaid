#include "item_upgrade.h"

#include <cstdio>
#include <cstring>
#include <set>

#include "game.h"
#include "game_tables.h"
#include "item.h"
#include "rand.h"

namespace {
bool IsPretzelChallenge(const Game* game) {
    return game->m_challenge && game->m_mode == MODE_PRETZEL;
}
}  // namespace

// The original leaves m_stat, m_amount and m_statText uninitialised; the port zeroes them.
ItemUpgrade::ItemUpgrade(Game* game) : m_game(game), m_stat(UPGRADE_WEAPON_DAMAGE), m_amount(0), m_statText{} {}

ItemUpgrade::~ItemUpgrade() = default;

// Quicken is only offered while the item has none (the original computes `1 - stats[5]` as a
// char and passes it only when stats[5] <= 1 unsigned, i.e. allowed iff stats[5] == 0).
void ItemUpgrade::Initialise(Item* item) {
    bool allowQuicken = item->m_stats[UPGRADE_QUICKEN] == 0;
    m_stat = static_cast<UpgradeType>(GetRandomStat(item->m_type, allowQuicken, m_game));
    SetIcon(m_stat % 10, m_stat / 10 + 5, 1024, 512);
    std::strcpy(m_name, g_upgradeNames[m_stat]);
    InitStats(item);
}

void ItemUpgrade::InitStats(Item* item) {
    m_amount = GetStatUpgrade(m_stat, m_game);
    switch (m_stat) {
        case UPGRADE_WEAPON_DAMAGE: WriteStatInt("Dmg", item); break;
        case UPGRADE_BASE_DAMAGE: WriteStatInt("Dmg", item); break;
        case UPGRADE_LIFE_LEECH: WriteStatPercent("Leech", item); break;
        case UPGRADE_POISON: WriteStatInt("Poison dmg", item); break;
        case UPGRADE_ARMOUR_PIERCING: WriteStatPercent("+ Pierce", item); break;
        case UPGRADE_QUICKEN: WriteStatInt("- Cooldown", item); break;
        case UPGRADE_BOOST_XP: WriteStatFloat("+ XP", item); break;
        case UPGRADE_BOOST_GOLD: WriteStatFloat("+ GP", item); break;
        case UPGRADE_BOOST_STRENGTH: WriteStatInt("+ STR", item); break;
        case UPGRADE_BOOST_LUCK: WriteStatInt("+ LUK", item); break;
        case UPGRADE_HEALTH: WriteStatInt("+ HP", item); break;
        case UPGRADE_ARMOUR: WriteStatInt("Defence", item); break;
        case UPGRADE_SPIKES: WriteStatInt("Spike dmg", item); break;
        case UPGRADE_BOOST_UP: WriteStatFloat("+ UP", item); break;
        case UPGRADE_DURABILITY: WriteStatPercent("- Armour dmg", item); break;
        case UPGRADE_BLUNTING: WriteStatInt("Attack dmg", item); break;
        case UPGRADE_REPAIR: WriteStatInt("+ Repair", item); break;
        case UPGRADE_REGENERATE: WriteStatInt("HP regenerate", item); break;
        case UPGRADE_BOOST_DEXTERITY: WriteStatInt("+ DEX", item); break;
        case UPGRADE_BOOST_VITALITY: WriteStatInt("+ VIT", item); break;
        default: break;
    }
}

const char* ItemUpgrade::GetDescription() {
    if (IsPretzelChallenge(m_game) && m_stat == UPGRADE_BOOST_VITALITY) {
        return "(+5% bonus potion)";
    }
    return g_upgradeDescriptions[m_stat];
}

const char* ItemUpgrade::GetStats() {
    return m_statText;
}

void ItemUpgrade::WriteStatInt(const char* label, Item* item) {
    int current = item->m_stats[m_stat];
    if (current < 1) {
        std::sprintf(m_statText, "%s: %d", label, m_amount);
    } else {
        std::sprintf(m_statText, "%s: %d >> %d", label, current, current + m_amount);
    }
}

void ItemUpgrade::WriteStatPercent(const char* label, Item* item) {
    int current = item->m_stats[m_stat];
    if (current < 1) {
        std::sprintf(m_statText, "%s: %d%%", label, m_amount);
    } else {
        std::sprintf(m_statText, "%s: %d%% >> %d%%", label, current, current + m_amount);
    }
}

void ItemUpgrade::WriteStatFloat(const char* label, Item* item) {
    int current = item->m_stats[m_stat];
    if (current < 1) {
        std::sprintf(m_statText, "%s: %.*f", label, GetFloatPrecision(m_amount),
                     static_cast<double>(GetFloatValue(m_amount)));
    } else {
        int next = current + m_amount;
        std::sprintf(m_statText, "%s: %.*f >> %.*f", label, GetFloatPrecision(current),
                     static_cast<double>(GetFloatValue(current)), GetFloatPrecision(next),
                     static_cast<double>(GetFloatValue(next)));
    }
}

// Quarters: whole numbers without decimals, halves with one, quarters with two.
int ItemUpgrade::GetFloatPrecision(int value) {
    if ((value & 3) == 0) return 0;
    if ((value & 1) == 0) return 1;
    return 2;
}

float ItemUpgrade::GetFloatValue(int value) {
    return static_cast<float>(value) * 0.25f;
}

// Caps: Life Leech 25, Armour Piercing 95%, the three boost rates 2.0, Durability (armour damage
// chance down to 10%); Health is never offered in a Pretzel challenge.
bool ItemUpgrade::IsUpgradeAllowed(UpgradeType stat, Game* game) {
    switch (stat) {
        case UPGRADE_LIFE_LEECH: return game->m_lifeLeech < 25;
        case UPGRADE_ARMOUR_PIERCING: return game->m_armourPiercingChance < 95;
        case UPGRADE_BOOST_XP: return game->m_xpBonusRate < 2.0f;
        case UPGRADE_BOOST_GOLD: return game->m_goldBonusRate < 2.0f;
        case UPGRADE_BOOST_UP: return game->m_upgradeBonusRate < 2.0f;
        case UPGRADE_HEALTH:
            if (game->m_challenge) return game->m_mode != MODE_PRETZEL;
            return true;
        case UPGRADE_DURABILITY: {
            int chance = game->m_armourDamageChance;
            if (chance > 99) chance = 100;
            return 100 - chance < 90;
        }
        default: return true;
    }
}

// Amount one upgrade adds. Life Leech and Durability shrink as the game total grows; the
// thresholds differ from GetStatDowngrade's (as in the original).
int ItemUpgrade::GetStatUpgrade(UpgradeType stat, Game* game) {
    switch (stat) {
        case UPGRADE_WEAPON_DAMAGE:
        case UPGRADE_BASE_DAMAGE:
        case UPGRADE_QUICKEN:
        case UPGRADE_BOOST_XP:
        case UPGRADE_BOOST_GOLD:
        case UPGRADE_BOOST_STRENGTH:
        case UPGRADE_BOOST_LUCK:
        case UPGRADE_ARMOUR:
        case UPGRADE_SPIKES:
        case UPGRADE_BOOST_UP:
        case UPGRADE_BLUNTING:
        case UPGRADE_REPAIR:
        case UPGRADE_BOOST_DEXTERITY:
        case UPGRADE_BOOST_VITALITY:
            return 1;
        case UPGRADE_LIFE_LEECH:
            if (game->m_lifeLeech < 15) return 5;
            if (game->m_lifeLeech < 21) return 3;
            return 2;
        case UPGRADE_POISON:
            return 2;
        case UPGRADE_ARMOUR_PIERCING:
            return 10;
        case UPGRADE_HEALTH:
            return IsPretzelChallenge(game) ? 0 : 10;
        case UPGRADE_DURABILITY:
            if (game->m_armourDamageChance <= 100) {
                int durability = 100 - game->m_armourDamageChance;
                if (durability > 85) return 2;
                if (durability > 79) return 3;
                if (durability > 69) return 5;
            }
            return 10;
        case UPGRADE_REGENERATE:
            return game->m_classes[game->m_currentClass].HavePerk(PERK_FAITHFUL) ? 2 : 1;
        default:
            return 0;
    }
}

int ItemUpgrade::GetStatDowngrade(UpgradeType stat, Game* game) {
    if (stat == UPGRADE_LIFE_LEECH) {
        if (game->m_lifeLeech < 18) return 5;
        if (game->m_lifeLeech < 23) return 3;
        return 2;
    }
    if (stat != UPGRADE_DURABILITY) {
        return GetStatUpgrade(stat, game);
    }
    if (game->m_armourDamageChance <= 100) {
        int durability = 100 - game->m_armourDamageChance;
        if (durability > 87) return 2;
        if (durability > 82) return 3;
        if (durability > 74) return 5;
    }
    return 10;
}

// Candidate stats per item type, minus the ones not allowed now; picks one uniformly (in
// ascending order of the set). Blunting is dropped in a Sprint challenge and otherwise kept only
// when RandI(0, attack reduction) gives 0.
int ItemUpgrade::GetRandomStat(int itemType, bool allowQuicken, Game* game) {
    std::set<int> stats;
    switch (itemType) {
        case WEAPON: stats = {0, 1, 2, 3, 4, 6, 8, 9}; break;
        case HELMET: stats = {11, 10, 12, 17, 6, 7, 13}; break;
        case ARMOUR: stats = {11, 14, 10, 12, 15, 17, 18, 19}; break;
        case SHIELD: stats = {11, 0, 1, 16, 12, 15, 13}; break;
        case ACCESSORY: stats = {8, 9, 18, 19, 5, 17, 10}; break;
        default: break;
    }
    if (!allowQuicken) stats.erase(UPGRADE_QUICKEN);
    if (game->m_challenge && game->m_mode == MODE_SPRINT) {
        stats.erase(UPGRADE_BLUNTING);
    } else if (RandI(0, game->m_attackReduction, RAND_UPGRADE) != 0) {
        stats.erase(UPGRADE_BLUNTING);
    }
    if (!IsUpgradeAllowed(UPGRADE_HEALTH, game)) stats.erase(UPGRADE_HEALTH);
    if (!IsUpgradeAllowed(UPGRADE_LIFE_LEECH, game)) stats.erase(UPGRADE_LIFE_LEECH);
    if (!IsUpgradeAllowed(UPGRADE_BOOST_XP, game)) stats.erase(UPGRADE_BOOST_XP);
    if (!IsUpgradeAllowed(UPGRADE_BOOST_UP, game)) stats.erase(UPGRADE_BOOST_UP);
    if (!IsUpgradeAllowed(UPGRADE_BOOST_GOLD, game)) stats.erase(UPGRADE_BOOST_GOLD);
    if (!IsUpgradeAllowed(UPGRADE_DURABILITY, game)) stats.erase(UPGRADE_DURABILITY);
    if (!IsUpgradeAllowed(UPGRADE_ARMOUR_PIERCING, game)) stats.erase(UPGRADE_ARMOUR_PIERCING);

    // With an empty set (unknown item type) the original returns what is left of the counter.
    int index = RandI(0, static_cast<int>(stats.size()) - 1, RAND_UPGRADE);
    for (int stat : stats) {
        if (index == 0) return stat;
        --index;
    }
    return index;
}
