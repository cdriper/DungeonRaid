// ItemUpgrade (derives from Upgrade): one stat upgrade offered for an item in the upgrade
// fold-out menu, plus the static stat rules shared with Item. Layout: re/structs/ItemUpgrade.json.
#pragma once

#include "dr_types.h"
#include "upgrade.h"

class Game;
class Item;

class ItemUpgrade : public Upgrade {
public:
    explicit ItemUpgrade(Game* game);                                 // 0x00051cc0
    ~ItemUpgrade() override;                                          // 0x00051c80

    void Initialise(Item* item);                                      // 0x000523c8
    void InitStats(Item* item);                                       // 0x00051ae4
    const char* GetDescription();                                     // 0x00051740
    const char* GetStats();                                           // 0x00051780: m_statText

    // "<label>: <amount>" for an item without the stat, else "<label>: <old> >> <new>"
    void WriteStatInt(const char* label, Item* item);                 // 0x00051a94
    void WriteStatPercent(const char* label, Item* item);             // 0x00051994
    void WriteStatFloat(const char* label, Item* item);               // 0x000519e4

    // Float stats (Boost XP/Gold/UP) are stored in quarters.
    static int GetFloatPrecision(int value);                          // 0x00051784
    static float GetFloatValue(int value);                            // 0x0005179c
    static bool IsUpgradeAllowed(UpgradeType stat, Game* game);       // 0x000517b8
    static int GetStatUpgrade(UpgradeType stat, Game* game);          // 0x00051888
    static int GetStatDowngrade(UpgradeType stat, Game* game);        // 0x0005193c
    // Random stat for an item type (ItemType; 4 = accessory). Returns an UpgradeType.
    static int GetRandomStat(int itemType, bool allowQuicken, Game* game);   // 0x00051db8

    Game* m_game;
    UpgradeType m_stat;
    int m_amount;
    char m_statText[32];
};
