// Item (derives from Upgrade): one equipment item (weapon, helmet, armour, shield, accessory) with
// its generated name and 20 stats. Layout: re/structs/Item.json (0x204 bytes in the original).
#pragma once

#include <string>
#include <vector>

#include "dr_types.h"
#include "upgrade.h"

class Game;
class GameState;
class ItemUpgrade;
class Label;

// Item::m_nameMapping entry (12 bytes in the original): icons [first, last) use `names`.
struct ItemNameMapping {
    std::vector<const char*>* names;
    int first;
    int last;
};

class Item : public Upgrade {
public:
    explicit Item(Game* game);                          // 0x00050e0c
    Item(Game* game, ItemType type);                    // 0x00051620: a new starting item
    ~Item() override;                                   // 0x0004ca30

    void Initialise(ItemType type);                     // 0x0004c0ec: random icon of the type
    void Initialise(Item* previous);                    // 0x000515f8: successor of `previous`
    void GenerateName(Item* previous);                  // 0x0004c35c
    void GenerateStats(Item* previous);                 // 0x00050ee4
    void ClearStats();                                  // 0x0004bac4
    void MakeStats();                                   // 0x0004cbe0
    int MakeStatLabels(std::vector<Label*>& labels);    // 0x0004bc74: returns the labels used
    int GetItemIndex();                                 // 0x0004bad8
    void IncreaseStat(UpgradeType stat);                // 0x0004dbb8
    void ReduceStat(UpgradeType stat);                  // 0x0004dbe0
    void AddUpgrade(ItemUpgrade* upgrade);              // 0x0004dc08
    void Serialise(GameState* state);                   // 0x0004bb90
    void Deserialise1(GameState* state);                // 0x0004da08
    void Deserialise2(GameState* state);                // 0x0004d858 (same code as Deserialise1)

    static void InitialiseNames();                      // 0x0004dc24

    Game* m_game;
    ItemType m_type;
    int m_iconIndex;
    int m_tier;                     // -1 for a starting item
    int m_prefixIndex;              // index into m_prefixes, -1 = "Novice"
    int m_nameIndex;                // index into the name list m_iconIndex maps to
    int m_stats[20];                // by UpgradeType
    std::string m_statsText1;       // short texts of the first 4 shown stats
    std::string m_statsText2;       // the rest
    int m_statTextRange[20][2];     // per stat: [start, end) of its text, -1 if not shown
    int m_statTextLine[20];         // per stat: 0 = m_statsText1, 1 = m_statsText2, -1

    static std::vector<const char*> m_prefixes;         // 0x00089504
    static std::vector<const char*> m_namesSword1;      // 0x00089510
    static std::vector<const char*> m_namesSword2;      // 0x0008951c
    static std::vector<const char*> m_namesMace;        // 0x00089528
    static std::vector<const char*> m_namesHammer;      // 0x00089534
    static std::vector<const char*> m_namesAxe;         // 0x00089540
    static std::vector<const char*> m_namesHelm;        // 0x0008954c
    static std::vector<const char*> m_namesShield;      // 0x00089558
    static std::vector<const char*> m_namesArmour;      // 0x00089564
    static std::vector<const char*> m_namesAmulet;      // 0x00089570
    static std::vector<const char*> m_namesRing;        // 0x0008957c
    static std::vector<const char*> m_namesCharm;       // 0x00089588
    static std::vector<const char*> m_namesRune;        // 0x00089594
    static std::vector<const char*> m_namesPotion;      // 0x000895a0
    static std::vector<ItemNameMapping> m_nameMapping;  // 0x000895ac
};
