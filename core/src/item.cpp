#include "item.h"

#include <cstdio>
#include <set>

#include "game.h"
#include "game_state.h"
#include "game_tables.h"
#include "item_upgrade.h"
#include "label.h"
#include "rand.h"

std::vector<const char*> Item::m_prefixes;
std::vector<const char*> Item::m_namesSword1;
std::vector<const char*> Item::m_namesSword2;
std::vector<const char*> Item::m_namesMace;
std::vector<const char*> Item::m_namesHammer;
std::vector<const char*> Item::m_namesAxe;
std::vector<const char*> Item::m_namesHelm;
std::vector<const char*> Item::m_namesShield;
std::vector<const char*> Item::m_namesArmour;
std::vector<const char*> Item::m_namesAmulet;
std::vector<const char*> Item::m_namesRing;
std::vector<const char*> Item::m_namesCharm;
std::vector<const char*> Item::m_namesRune;
std::vector<const char*> Item::m_namesPotion;
std::vector<ItemNameMapping> Item::m_nameMapping;

namespace {
// Icon index range of each item type: g_*Range[0..1] (inclusive; accessory for any other type)
const int* IconRange(ItemType type) {
    switch (type) {
        case WEAPON: return g_weaponsRange;
        case HELMET: return g_helmetsRange;
        case ARMOUR: return g_armourRange;
        case SHIELD: return g_shieldsRange;
        default: return g_accessoriesRange;
    }
}

int ClampAtZero(int v) { return v < 0 ? 0 : v; }

// MakeStatLabels' text buffer: a static 128-byte buffer in the original (0x00089484, up to
// Item::m_prefixes), used by nothing else.
char s_statLabelText[128];
}  // namespace

// The original leaves everything but the Upgrade part, m_game, the strings and m_statTextRange
// uninitialised; the port zeroes the scalars.
Item::Item(Game* game)
    : m_game(game),
      m_type(WEAPON),
      m_iconIndex(0),
      m_tier(0),
      m_prefixIndex(0),
      m_nameIndex(0),
      m_stats{},
      m_statTextRange{},
      m_statTextLine{} {
    InitialiseNames();
}

Item::Item(Game* game, ItemType type) : Item(game) {
    Initialise(type);
    GenerateName(nullptr);
    GenerateStats(nullptr);
    MakeStats();
}

Item::~Item() = default;

void Item::Initialise(ItemType type) {
    const int* range = IconRange(type);
    m_iconIndex = RandI(range[0], range[1], RAND_ITEM);
    SetIcon(m_iconIndex % 20, m_iconIndex / 20, 1024, 0);
    m_type = type;
}

void Item::Initialise(Item* previous) {
    Initialise(previous->m_type);
    GenerateName(previous);
    GenerateStats(previous);
    MakeStats();
}

// A starting item is "Novice". A successor's tier rises by 1..10 until it is within 12 of the end
// of the prefix list; from there the tier stays and the prefix is any from the tier to the end.
void Item::GenerateName(Item* previous) {
    const char* prefix;
    if (previous == nullptr) {
        m_tier = -1;
        m_prefixIndex = -1;
        prefix = "Novice";
    } else {
        int tier = previous->m_tier;
        int prefixCount = static_cast<int>(m_prefixes.size());
        if (tier < prefixCount - 12) {
            m_tier = RandI(tier + 1, tier + 10, RAND_ITEM);
            m_prefixIndex = m_tier;
        } else {
            m_tier = tier;
            m_prefixIndex = RandI(previous->m_tier, prefixCount - 1, RAND_ITEM);
        }
        prefix = m_prefixes[m_prefixIndex];
    }
    const char* name = "";
    for (const ItemNameMapping& mapping : m_nameMapping) {
        if (mapping.first <= m_iconIndex && m_iconIndex < mapping.last) {
            m_nameIndex = RandI(0, static_cast<int>(mapping.names->size()) - 1, RAND_ITEM);
            name = (*mapping.names)[m_nameIndex];
            break;
        }
    }
    std::sprintf(m_name, "%s  %s", prefix, name);
}

// Without a predecessor: the type's base stat. With one: the type's main stat grows and every other
// stat the predecessor had is carried over, each with a chance to be reduced (and, depending on the
// type, converted into main stat or a random accessory stat).
void Item::GenerateStats(Item* previous) {
    ClearStats();
    if (previous == nullptr) {
        switch (m_type) {
            case WEAPON:
                m_stats[UPGRADE_WEAPON_DAMAGE] = 2;
                break;
            case HELMET:
            case SHIELD:
                m_stats[UPGRADE_ARMOUR] = 1;
                break;
            case ARMOUR:
                m_stats[UPGRADE_ARMOUR] = m_game->m_classes[m_game->m_currentClass].HaveFlaw(FLAW_SLIM) ? 1 : 2;
                break;
            case ACCESSORY:
                if (m_game->m_challenge && m_game->m_mode == MODE_PRETZEL) break;
                m_stats[UPGRADE_HEALTH] = 10;
                break;
            default:
                break;
        }
        return;
    }

    std::set<int> carried;
    for (int i = 0; i < 20; ++i) {
        if (previous->m_stats[i] > 0) carried.insert(i);
    }
    switch (m_type) {
        case WEAPON:
            m_stats[UPGRADE_WEAPON_DAMAGE] = previous->m_stats[UPGRADE_WEAPON_DAMAGE] + 1;
            carried.erase(UPGRADE_WEAPON_DAMAGE);
            for (int stat : carried) {
                int value = previous->m_stats[stat];
                if (Rand(0.0f, 1.0f, RAND_ITEM) < 0.15f) {
                    m_stats[stat] = ClampAtZero(value - ItemUpgrade::GetStatDowngrade(static_cast<UpgradeType>(stat), m_game));
                    m_stats[UPGRADE_WEAPON_DAMAGE] += 1;
                } else {
                    m_stats[stat] = value;
                }
            }
            break;
        case HELMET: {
            m_stats[UPGRADE_ARMOUR] = previous->m_stats[UPGRADE_ARMOUR] + 1;
            carried.erase(UPGRADE_ARMOUR);
            bool upgraded = false;   // at most one stat is upgraded instead of reduced
            for (int stat : carried) {
                int value = previous->m_stats[stat];
                UpgradeType type = static_cast<UpgradeType>(stat);
                if (Rand(0.0f, 1.0f, RAND_ITEM) < 0.2f) {
                    if (ItemUpgrade::IsUpgradeAllowed(type, m_game) && !upgraded && Rand(0.0f, 1.0f, RAND_ITEM) < 0.5f) {
                        m_stats[stat] = ItemUpgrade::GetStatUpgrade(type, m_game) + value;
                        upgraded = true;
                    } else {
                        m_stats[stat] = ClampAtZero(value - ItemUpgrade::GetStatDowngrade(type, m_game));
                        m_stats[UPGRADE_ARMOUR] += 1;
                    }
                } else {
                    m_stats[stat] = value;
                }
            }
            break;
        }
        case ARMOUR: {
            // 20%: two more armour and every stat is kept; otherwise one more, stats may be reduced
            bool mayReduce = !(Rand(0.0f, 1.0f, RAND_ITEM) < 0.2f);
            m_stats[UPGRADE_ARMOUR] = previous->m_stats[UPGRADE_ARMOUR] + (mayReduce ? 1 : 2);
            carried.erase(UPGRADE_ARMOUR);
            for (int stat : carried) {
                int value = previous->m_stats[stat];
                if (mayReduce && Rand(0.0f, 1.0f, RAND_ITEM) < 0.1f) {
                    m_stats[stat] = ClampAtZero(value - ItemUpgrade::GetStatDowngrade(static_cast<UpgradeType>(stat), m_game));
                    m_stats[UPGRADE_ARMOUR] += 1;
                } else {
                    m_stats[stat] = value;
                }
            }
            break;
        }
        case SHIELD:
            m_stats[UPGRADE_ARMOUR] = previous->m_stats[UPGRADE_ARMOUR] + 1;
            carried.erase(UPGRADE_ARMOUR);
            for (int stat : carried) {
                int value = previous->m_stats[stat];
                UpgradeType type = static_cast<UpgradeType>(stat);
                if (stat == UPGRADE_WEAPON_DAMAGE) {
                    // weapon damage on a shield is usually converted into armour
                    if (Rand(0.0f, 1.0f, RAND_ITEM) < 0.7f) {
                        m_stats[stat] = ClampAtZero(value - ItemUpgrade::GetStatDowngrade(type, m_game));
                        m_stats[UPGRADE_ARMOUR] += 1;
                    } else {
                        m_stats[stat] = value;
                    }
                } else if (Rand(0.0f, 1.0f, RAND_ITEM) < 0.2f) {
                    m_stats[stat] = ClampAtZero(value - ItemUpgrade::GetStatDowngrade(type, m_game));
                    if (Rand(0.0f, 1.0f, RAND_ITEM) < 0.2f) {
                        m_stats[UPGRADE_WEAPON_DAMAGE] += 1;
                    } else {
                        m_stats[UPGRADE_ARMOUR] += 1;
                    }
                } else {
                    m_stats[stat] = value;
                }
            }
            break;
        case ACCESSORY: {
            m_stats[UPGRADE_HEALTH] = previous->m_stats[UPGRADE_HEALTH] + ItemUpgrade::GetStatUpgrade(UPGRADE_HEALTH, m_game) / 2;
            carried.erase(UPGRADE_HEALTH);
            // A reduced stat moves one upgrade to a random accessory stat. That stat may come later in
            // the loop and then be overwritten with the predecessor's value (as in the original).
            for (int stat : carried) {
                int value = previous->m_stats[stat];
                if (Rand(0.0f, 1.0f, RAND_ITEM) < 0.1f) {
                    m_stats[stat] = ClampAtZero(value - ItemUpgrade::GetStatDowngrade(static_cast<UpgradeType>(stat), m_game));
                    int bonus = ItemUpgrade::GetRandomStat(ACCESSORY, false, m_game);
                    m_stats[bonus] = ItemUpgrade::GetStatUpgrade(static_cast<UpgradeType>(bonus), m_game) + m_stats[bonus];
                } else {
                    m_stats[stat] = value;
                }
            }
            // one extra random stat: always in a Pretzel challenge, else with 50%
            bool pretzel = m_game->m_challenge && m_game->m_mode == MODE_PRETZEL;
            if (pretzel || Rand(0.0f, 1.0f, RAND_ITEM) < 0.5f) {
                bool allowQuicken = m_stats[UPGRADE_QUICKEN] == 0;
                int bonus = ItemUpgrade::GetRandomStat(ACCESSORY, allowQuicken, m_game);
                m_stats[bonus] = m_stats[bonus] + ItemUpgrade::GetStatUpgrade(static_cast<UpgradeType>(bonus), m_game);
            }
            break;
        }
        default:
            break;
    }
}

void Item::ClearStats() {
    for (int& stat : m_stats) stat = 0;
}

// Short stat texts ("3 dmg, 2 def, ..."), in a fixed stat order: the first four go to
// m_statsText1, the rest to m_statsText2, each text's position is recorded per stat.
void Item::MakeStats() {
    std::vector<std::string> texts;
    std::vector<int> stats;
    char buffer[32];
    auto add = [&](int stat) {
        texts.push_back(buffer);
        stats.push_back(stat);
    };
    auto addFloat = [&](int stat, const char* format) {
        std::sprintf(buffer, format, ItemUpgrade::GetFloatPrecision(m_stats[stat]),
                     static_cast<double>(ItemUpgrade::GetFloatValue(m_stats[stat])));
        add(stat);
    };
    if (m_stats[11] > 0) { std::sprintf(buffer, "%d def", m_stats[11]); add(11); }
    if (m_stats[0] > 0) { std::sprintf(buffer, "%d dmg", m_stats[0]); add(0); }
    if (m_stats[1] > 0) { std::sprintf(buffer, "%d base dmg", m_stats[1]); add(1); }
    if (m_stats[10] > 0) { std::sprintf(buffer, "%d hp", m_stats[10]); add(10); }
    if (m_stats[8] > 0) { std::sprintf(buffer, "%d str", m_stats[8]); add(8); }
    if (m_stats[18] > 0) { std::sprintf(buffer, "%d dex", m_stats[18]); add(18); }
    if (m_stats[19] > 0) { std::sprintf(buffer, "%d vit", m_stats[19]); add(19); }
    if (m_stats[9] > 0) { std::sprintf(buffer, "%d luk", m_stats[9]); add(9); }
    if (m_stats[2] > 0) { std::sprintf(buffer, "%d%% leech", m_stats[2]); add(2); }
    if (m_stats[3] > 0) { std::sprintf(buffer, "%d poison", m_stats[3]); add(3); }
    if (m_stats[4] > 0) { std::sprintf(buffer, "%d%% pierce", m_stats[4]); add(4); }
    if (m_stats[5] > 0) { std::sprintf(buffer, "quick"); add(5); }
    if (m_stats[6] > 0) addFloat(6, "%.*f xp");
    if (m_stats[7] > 0) addFloat(7, "%.*f gp");
    if (m_stats[13] > 0) addFloat(13, "%.*f up");
    if (m_stats[12] > 0) { std::sprintf(buffer, "%d spikes", m_stats[12]); add(12); }
    if (m_stats[14] > 0) { std::sprintf(buffer, "%d%% durable", m_stats[14]); add(14); }
    if (m_stats[15] > 0) { std::sprintf(buffer, "%d blunt", m_stats[15]); add(15); }
    if (m_stats[16] > 0) { std::sprintf(buffer, "%d repair", m_stats[16]); add(16); }
    if (m_stats[17] > 0) { std::sprintf(buffer, "%d regen", m_stats[17]); add(17); }

    for (int i = 0; i < 20; ++i) {
        m_statTextLine[i] = -1;
        m_statTextRange[i][0] = -1;
        m_statTextRange[i][1] = -1;
    }
    m_statsText1.clear();
    m_statsText2.clear();
    for (size_t i = 0; i < texts.size(); ++i) {
        int line = i < 4 ? 0 : 1;
        std::string& text = line == 0 ? m_statsText1 : m_statsText2;
        if (i != 0 && i != 4) text += ", ";
        int stat = stats[i];
        m_statTextLine[stat] = line;
        m_statTextRange[stat][0] = static_cast<int>(text.size());
        text += texts[i];
        m_statTextRange[stat][1] = static_cast<int>(text.size());
    }
}

// Fills labels[0] with the name and one label per non-zero stat (fixed order); returns the count.
int Item::MakeStatLabels(std::vector<Label*>& labels) {
    labels[0]->SetText(m_name, -1);
    int n = 1;
    char* text = s_statLabelText;
    auto put = [&]() { labels[n++]->SetText(text, -1); };
    auto putFloat = [&](int stat, const char* format) {
        std::sprintf(text, format, ItemUpgrade::GetFloatPrecision(m_stats[stat]),
                     static_cast<double>(ItemUpgrade::GetFloatValue(m_stats[stat])));
        put();
    };
    if (m_stats[11] > 0) { std::sprintf(text, "%d Defence", m_stats[11]); put(); }
    if (m_stats[0] > 0) { std::sprintf(text, "%d Damage", m_stats[0]); put(); }
    if (m_stats[1] > 0) { std::sprintf(text, "+%d Base damage", m_stats[1]); put(); }
    if (m_stats[10] > 0) { std::sprintf(text, "+%d Health", m_stats[10]); put(); }
    if (m_stats[8] > 0) { std::sprintf(text, "+%d Strength", m_stats[8]); put(); }
    if (m_stats[18] > 0) { std::sprintf(text, "+%d Dexterity", m_stats[18]); put(); }
    if (m_stats[19] > 0) { std::sprintf(text, "+%d Vitality", m_stats[19]); put(); }
    if (m_stats[9] > 0) { std::sprintf(text, "+%d Luck", m_stats[9]); put(); }
    if (m_stats[2] > 0) { std::sprintf(text, "%d%% Life leech", m_stats[2]); put(); }
    if (m_stats[3] > 0) { std::sprintf(text, "%d Poison damage per turn for 3 turns", m_stats[3]); put(); }
    if (m_stats[4] > 0) { std::sprintf(text, "+%d%% Armour piercing", m_stats[4]); put(); }
    if (m_stats[5] > 0) { std::sprintf(text, "Quicken: reduce skills' cooldown by 1"); put(); }
    if (m_stats[6] > 0) putFloat(6, "+%.*f Experience points per skull");
    if (m_stats[7] > 0) putFloat(7, "+%.*f Gold per coin");
    if (m_stats[13] > 0) putFloat(13, "+%.*f Upgrade points per shield");
    if (m_stats[12] > 0) { std::sprintf(text, "%d Spike damage to attacking enemies", m_stats[12]); put(); }
    if (m_stats[14] > 0) { std::sprintf(text, "+%d%% Armour durability", m_stats[14]); put(); }
    if (m_stats[15] > 0) { std::sprintf(text, "%d Blunting damage reduces enemies' attack", m_stats[15]); put(); }
    if (m_stats[16] > 0) { std::sprintf(text, "+%d Defence per shield", m_stats[16]); put(); }
    if (m_stats[17] > 0) { std::sprintf(text, "+%d Health at the end of each turn", m_stats[17]); put(); }
    return n;
}

int Item::GetItemIndex() {
    return m_iconIndex - IconRange(m_type)[0];
}

void Item::IncreaseStat(UpgradeType stat) {
    m_stats[stat] = ClampAtZero(m_stats[stat] + ItemUpgrade::GetStatUpgrade(stat, m_game));
    MakeStats();
}

void Item::ReduceStat(UpgradeType stat) {
    m_stats[stat] = ClampAtZero(m_stats[stat] - ItemUpgrade::GetStatDowngrade(stat, m_game));
    MakeStats();
}

void Item::AddUpgrade(ItemUpgrade* upgrade) {
    m_stats[upgrade->m_stat] += upgrade->m_amount;
    MakeStats();
}

// Bytes: icon index relative to the type's range, tier, prefix, name (one byte each, -1 as 0xff),
// a bitmask of the non-zero stats, then those stats as integers. More than 7 stats unlock
// "Armed and Dangerous".
void Item::Serialise(GameState* state) {
    state->WriteChar(static_cast<uint8_t>(m_iconIndex - IconRange(m_type)[0]));
    state->WriteChar(static_cast<uint8_t>(m_tier));
    state->WriteChar(static_cast<uint8_t>(m_prefixIndex));
    state->WriteChar(static_cast<uint8_t>(m_nameIndex));
    unsigned mask = 0;
    for (unsigned i = 0; i < 20; ++i) {
        if (m_stats[i] != 0) mask |= 1u << i;
    }
    state->WriteInteger(static_cast<int>(mask));
    int written = 0;
    for (int i = 0; i < 20; ++i) {
        if (m_stats[i] != 0) {
            state->WriteInteger(m_stats[i]);
            ++written;
        }
    }
    if (written > 7) {
        m_game->QueueAchievement(ACH_ARMED_DANGEROUS, 100.0f);
    }
}

// The type must be set before. Out-of-range prefix/name indices read past the lists (undefined
// behaviour in the original as well).
void Item::Deserialise1(GameState* state) {
    m_iconIndex = IconRange(m_type)[0] + state->ReadChar();
    m_tier = state->ReadChar();
    m_prefixIndex = state->ReadChar();
    int nameIndex = state->ReadChar();
    if (m_tier == 0xff) m_tier = -1;
    if (m_prefixIndex == 0xff) m_prefixIndex = -1;
    m_nameIndex = nameIndex;
    SetIcon(m_iconIndex % 20, m_iconIndex / 20, 1024, 0);
    const char* prefix = m_prefixIndex < 0 ? "Novice" : m_prefixes[m_prefixIndex];
    const char* name = "";
    for (const ItemNameMapping& mapping : m_nameMapping) {
        if (mapping.first <= m_iconIndex && m_iconIndex < mapping.last) {
            name = (*mapping.names)[m_nameIndex];
            break;
        }
    }
    std::sprintf(m_name, "%s  %s", prefix, name);
    ClearStats();
    int mask = state->ReadInteger();
    for (int i = 0; i < 20; ++i) {
        if ((mask >> i) & 1) m_stats[i] = 1;
    }
    for (int i = 0; i < 20; ++i) {
        if (m_stats[i] != 0) m_stats[i] = state->ReadInteger();
    }
    MakeStats();
}

void Item::Deserialise2(GameState* state) {
    Deserialise1(state);
}

// Fills the name lists once (guarded by m_namesSword1 being empty, as in the original). Icons
// 0..399 map to the lists; the prefixes are ordered from worst to best (index = tier).
void Item::InitialiseNames() {
    if (!m_namesSword1.empty()) return;
    m_namesSword1 = {
        "Sword", "Blade", "Cutter", "Stabber", "Claymore", "Longsword", "Broadsword", "Flamberge",
        "Gladius", "Greatsword", "Zweihander", "Shortsword", "Broadsword", "Dagger", "Dirk",
    };
    m_namesSword2 = {
        "Sword", "Blade", "Cutter", "Slasher", "Falchion", "Sabre", "Scimitar", "Katana", "Backsword",
        "Cutlass", "Talwar", "Shamshir",
    };
    m_namesMace = {
        "Mace", "Battlemace", "Morningstar", "Club", "Pernach", "Bulawa", "Goedendag", "Cudgel",
        "Warclub", "Truncheon", "Beater", "Pounder", "Scepter",
    };
    m_namesHammer = {
        "Hammer", "Warhammer", "Battlehammer", "Maul", "Greatmaul", "Sledgehammer", "Warmaul",
        "Bludgeon", "Buster", "Slammer", "Basher", "Smiter",
    };
    m_namesAxe = {
        "Axe", "Battleaxe", "Chopper", "Hewer", "Waraxe", "Cleaver", "Handaxe", "Halberd", "Poleaxe",
        "Voulge", "Bardiche", "Hatchet", "Broadaxe", "Tomahawk", "Francisca",
    };
    m_namesHelm = {
        "Helm", "Helmet", "Cowl", "Warhelm", "Battlehelm", "Greathelm", "Mask", "Bascinet", "Sallet",
        "Crown", "Hood", "Casque", "Skullcap", "Armet", "Barbute", "Spangenhelm", "Headgear",
    };
    m_namesShield = {
        "Shield", "Buckler", "Crest", "Aegis", "Barrier", "Guard", "Deflector", "Escutcheon", "Heater",
        "Pavise", "Targe", "Scutum", "Aspis", "Fender", "Defender", "Rondel",
    };
    m_namesArmour = {
        "Armour", "Platemail", "Breastplate", "Carapace", "Shell", "Cuirass", "Splintmail",
        "Battleplate", "Halfplate", "Vest", "Mail", "Brigandine", "Coat", "Plate", "Guardplate",
        "Mailcoat", "Shroud",
    };
    m_namesAmulet = {
        "Amulet", "Necklace", "Talisman", "Medal", "Sacrament", "Scapular", "Necklace", "Choker",
        "Collar", "Torc", "Pendant", "Carcanet", "Locket", "Medallion",
    };
    m_namesRing = {
        "Ring", "Band", "Loop", "Ringlet", "Signet", "Keepsake", "Annulus", "Hoop", "Bracelet", "Halo",
        "Circlet",
    };
    m_namesCharm = {
        "Charm", "Bauble", "Ornament", "Phylactery", "Trinket", "Idol", "Artifact", "Relic", "Heirloom",
        "Antique", "Trophy", "Tribute", "Curio", "Remnant",
    };
    m_namesRune = {
        "Rune", "Image", "Stone", "Tablet", "Symbol", "Sign", "Emblem", "Token", "Vestige", "Hex",
        "Mark", "Brand", "Insignia", "Seal", "Fragment", "Stamp",
    };
    m_namesPotion = {
        "Potion", "Brew", "Poultice", "Salve", "Unguent", "Draught", "Elixir", "Mixture", "Tonic",
        "Philter", "Ointment", "Concoction", "Drink", "Medicine", "Fluid", "Solution",
    };
    m_nameMapping = {
        {&m_namesSword1, 0, 20},    {&m_namesSword2, 20, 40},   {&m_namesMace, 40, 50},
        {&m_namesHammer, 50, 60},   {&m_namesAxe, 60, 80},      {&m_namesHelm, 80, 160},
        {&m_namesShield, 160, 240}, {&m_namesArmour, 240, 320}, {&m_namesAmulet, 320, 340},
        {&m_namesRing, 340, 360},   {&m_namesCharm, 360, 370},  {&m_namesRune, 370, 380},
        {&m_namesPotion, 380, 400},
    };
    m_prefixes = {
        "Broken", "Rotten", "Damaged", "Fragile", "Faded", "Rusty", "Dull", "Torpid", "Simple", "Awful",
        "Poor", "Languid", "Plain", "Dusty", "Frail", "Slow", "Tarnished", "Basic", "Pitiful", "Tame",
        "Old", "Docile", "Dim", "Tragic", "Drab", "Raw", "Sullen", "Bitter", "Painful", "Wild",
        "Anxious", "Greedy", "Heavy", "Cold", "Humble", "Clear", "Earnest", "Light", "Honest",
        "Troubled", "Fit", "Rebel", "Ready", "Polished", "Wicked", "Bright", "Malicious", "Hardened",
        "Fair", "Apt", "Cruel", "Dire", "Serious", "Angry", "Quick", "Bloodied", "Tainted", "Main",
        "Sinful", "Proud", "Enticing", "Confident", "Woeful", "Forged", "Radical", "Possessed", "Spiky",
        "Solid", "Adept", "Beautiful", "Keen", "Flashy", "Smashing", "Large", "High", "Just", "Vivid",
        "Vile", "Frantic", "Dreadful", "Shiny", "Risky", "Gifted", "Intense", "Strong", "Imposing",
        "Beaming", "Tireless", "Silver", "Fearsome", "Surreal", "Gleaming", "Silent", "Rapid",
        "Questing", "Stunning", "Brutal", "Ghostly", "Azure", "Famous", "Lofty", "Awesome", "Chaotic",
        "Lustrous", "Vicious", "Infamous", "Sage", "Killer", "Lawful", "Noble", "Monstrous", "Radiant",
        "Golden", "Fateful", "Flaring", "True", "Arcane", "Powerful", "Winning", "Destined", "Evil",
        "Superior", "Rare", "Magnificent", "Regal", "Demonic", "Grand", "Fantastic", "Superb", "Heroic",
        "Astonishing", "Extreme", "Supreme", "Runic", "Glorious", "Royal", "Imperial", "Sublime",
        "Tremendous", "Saintly", "Exalted", "Ancient", "Final", "Legendary", "Epic", "Godly", "Vorpal",
        "Ultimate",
    };
}
