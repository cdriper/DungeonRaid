#include "foldout_menu.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "game.h"
#include "interface.h"
#include "item.h"
#include "item_upgrade.h"
#include "dr_gl.h"
#include "font.h"
#include "label.h"
#include "rand.h"
#include "skill.h"
#include "util.h"

namespace {
// g_buttonPositions (0x00089328, filled by a static initialiser): top left of the four option
// buttons in 320-pixel units.
const GLfloat2 kButtonPositions[4] = {{18.0f, 33.0f}, {18.0f, 90.0f}, {18.0f, 147.0f}, {18.0f, 204.0f}};

GLfloat2 IconPosition(int option, float dx) {
    return GLfloat2{(kButtonPositions[option].x + dx) / 320.0f, (kButtonPositions[option].y + 2.0f) / 320.0f};
}

// The n-th element of a set in order, walked like the original's loops: false (out unchanged)
// when n is past the end.
bool NthOfSet(const std::set<int>& s, int n, int& out) {
    for (int v : s) {
        if (n == 0) {
            out = v;
            return true;
        }
        --n;
    }
    return false;
}

CharacterClass& Current(Game* g) { return g->m_classes[g->m_currentClass]; }

// The shop's scroll offer skips skill type t when the word at Game+0xc358+4t is non-zero: meant as
// m_skills[t], but m_skills has 4 slots, so for t >= 4 the original reads the fields that follow
// (count, chain counts, flags, then the life leech SimpleLabel at 0xc388). Reproduced from the port's
// fields; padding bytes count as 0 (fresh heap memory in the original).
uint32_t OriginalSkillSlotWord(Game* g, int t) {
    static_assert(offsetof(SimpleLabel, m_type) == 4 && offsetof(SimpleLabel, m_vertices) == 8,
                  "SimpleLabel must keep the original layout");
    if (t < 4) return g->m_skills[t] != nullptr ? 1u : 0u;
    if (t == 4) return static_cast<uint32_t>(g->m_skillCount);
    if (t < 10) return static_cast<uint32_t>(g->m_chainResourceCounts[t - 5]);
    if (t == 10) {
        return static_cast<uint32_t>(g->m_fillingNewBoard) | static_cast<uint32_t>(g->m_newBoardAvoidsSwords) << 8 |
               static_cast<uint32_t>(g->m_counterActive) << 16 | static_cast<uint32_t>(g->m_stealActive) << 24;
    }
    if (t == 11) return static_cast<uint32_t>(g->m_stealActiveNextTurn);
    const SimpleLabel& label = g->m_simpleLabelC388;
    if (t == 12) return static_cast<uint32_t>(label.m_active);
    uint32_t word;
    std::memcpy(&word, reinterpret_cast<const uint8_t*>(&label) + (t - 12) * 4, 4);
    return word;
}
}  // namespace

FoldoutMenu::FoldoutMenu() : Menu(kQuadCount) {}

FoldoutMenu::~FoldoutMenu() {
    for (Label* l : m_labels) delete l;
    for (int i = 0; i < 4; ++i) {
        delete m_items[i];
        delete m_skills[i];
        delete m_upgrades[i];
    }
    delete m_scrollSkill;
}

void FoldoutMenu::Create(Game* game, Interface* ui) {
    m_game = game;
    m_active = false;
    m_interface = ui;
    m_confirmEnabled = false;
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0xff);
    SetQuad(0, {0.0f, 0.0f}, {498.0f, 498.0f}, {13.0f, 13.0f}, 1.0f, 1.0f);
    SetQuad(1, {13.0f, 0.0f}, {479.0f, 472.0f}, {1.0f, 13.0f}, 284.0f, 1.0f);
    SetQuad(2, {0.0f, 13.0f}, {478.0f, 487.0f}, {13.0f, 1.0f}, 1.0f, 249.0f);
    SetQuad(3, {0.0f, 262.0f}, {498.0f, 482.0f}, {13.0f, 15.0f}, 1.0f, 1.0f);
    SetQuad(4, {13.0f, 262.0f}, {483.0f, 470.0f}, {1.0f, 15.0f}, 284.0f, 1.0f);
    SetQuad(5, {13.0f, 13.0f}, {145.0f, 399.0f}, {1.0f, 1.0f}, 284.0f, 249.0f);
    // port: the right side of the frame, the left side's quads mirrored (the original has none: the
    // menu ends at the screen's edge, which the horizontal layout shows inside the window)
    SetQuad(kRightFrame, {297.0f, 0.0f}, {511.0f, 498.0f}, {-13.0f, 13.0f}, -1.0f, 1.0f);
    SetQuad(kRightFrame + 1, {297.0f, 13.0f}, {491.0f, 487.0f}, {-13.0f, 1.0f}, -1.0f, 249.0f);
    SetQuad(kRightFrame + 2, {297.0f, 262.0f}, {511.0f, 482.0f}, {-13.0f, 15.0f}, -1.0f, 1.0f);
    SetQuad(6, {0.0f, 262.0f}, {109.0f, 383.0f}, {91.0f, 52.0f}, 1.0f, 1.0f);
    SetQuad(kConfirmQuad, {18.0f, 261.0f}, {143.0f, 288.0f}, {53.0f, 35.0f}, 1.0f, 1.0f);
    SetQuad(8, {309.0f, 380.0f}, {309.0f, 380.0f}, {3.0f, 55.0f}, 1.0f, 1.0f);
    SetQuad(9, {313.0f, 380.0f}, {313.0f, 380.0f}, {1.0f, 55.0f}, 271.0f, 1.0f);
    SetQuad(10, {315.0f, 380.0f}, {315.0f, 380.0f}, {3.0f, 55.0f}, 1.0f, 1.0f);
    SetQuad(11, {309.0f, 380.0f}, {309.0f, 380.0f}, {3.0f, 55.0f}, 1.0f, 1.0f);
    SetQuad(12, {313.0f, 380.0f}, {313.0f, 380.0f}, {1.0f, 55.0f}, 271.0f, 1.0f);
    SetQuad(13, {315.0f, 380.0f}, {315.0f, 380.0f}, {3.0f, 55.0f}, 1.0f, 1.0f);
    SetQuad(0x14, {18.0f, 204.0f}, {319.0f, 380.0f}, {3.0f, 53.0f}, 1.0f, 1.0f);
    SetQuad(0x15, {21.0f, 204.0f}, {323.0f, 380.0f}, {1.0f, 53.0f}, 269.0f, 1.0f);
    SetQuad(0x16, {290.0f, 204.0f}, {325.0f, 380.0f}, {3.0f, 53.0f}, 1.0f, 1.0f);
    for (int i = 0; i < 4; ++i) {
        SetQuad(0x1b + i, {kButtonPositions[i].x + 53.0f, kButtonPositions[i].y + 16.0f}, {491.0f, 93.0f},
                {20.0f, 21.0f}, 1.0f, 1.0f);
    }
    m_offset.y = 0.28125f;
    for (int i = 0; i < 4; ++i) {
        m_items[i] = new Item(m_game);
        m_skills[i] = new Skill(m_game);
        m_upgrades[i] = new ItemUpgrade(m_game);
    }
    m_scrollSkill = new Skill(m_game);
    m_shade.SetSize(GLfloat2{2.0f, 2.0f});
    m_shade.m_position = GLfloat2{0.5f, 0.5f};
    m_shade.SetColour(GLcolour4{0, 0, 0, 0});
}

void FoldoutMenu::Initialise(int type) {
    Game* g = m_game;
    m_active = true;
    m_type = type;
    m_animTime = 0.0f;
    m_opening = true;
    m_offset.x = 1.0f;
    m_confirmEnabled = false;
    m_selected[0] = -1;
    m_selected[1] = -1;
    m_confirmPressed = false;
    m_lastTouchState = 0;
    m_interface->HideTooltip();
    m_labelCount = 0;
    for (int& c : m_optionLabelCounts) c = 0;
    for (int i = 0; i < 4; ++i) SetQuad(kFirstOption + i, kButtonPositions[i], {1.0f, 382.0f}, {53.0f, 53.0f}, 1.0f, 1.0f);
    // challenge mode Sprint (computed as 1 - mode, 0 when mode > 1 unsigned)
    bool sprint = g->m_challenge && g->m_mode == MODE_SPRINT;

    if (type == 1) {   // shop: three items, maybe with a skill scroll
        g->QueueSound(SOUND_SHOP);
        SetQuad(0x12, {15.0f, 3.0f}, {1.0f, 296.0f}, {141.0f, 29.0f}, 1.0f, 1.0f);
        SetQuad(0x13, {161.0f, 15.0f}, {123.0f, 266.0f}, {127.0f, 21.0f}, 1.0f, 1.0f);
        std::set<int> types;
        for (int t = 0; t < 5; ++t) types.insert(t);
        for (int i = 0; i < 3; ++i) {
            int n = RandI(0, static_cast<int>(types.size()) - 1, RAND_ITEM);
            int t;
            if (!NthOfSet(types, n, t)) continue;
            Item* item = m_items[i];
            item->Initialise(g->GetItem(static_cast<ItemType>(t)));
            item->m_iconPosition = IconPosition(i, 2.0f);
            types.erase(t);
            AddLabel(i, item->m_name);
            AddLabel(i, item->m_statsText1.c_str());
            AddLabel(i, item->m_statsText2.c_str());
        }
        float chance = Current(g).m_race == RACE_SAUREN ? 0.5f : 0.25f;
        if (Rand(0.0f, 1.0f, RAND_ITEM) < chance) {
            int index = RandI(0, 2, RAND_ITEM);
            m_scrollIndex = index;
            SetQuad(0x17, {kButtonPositions[index].x + 54.0f, kButtonPositions[index].y}, {1.0f, 382.0f},
                    {53.0f, 53.0f}, 1.0f, 1.0f);
            bool allowOwned = Rand(0.0f, 1.0f, RAND_ITEM) < 0.1f;
            std::vector<int> candidates;
            for (int t = 0; t < 0x2a; ++t) {
                if (t == SKILL_WISH) continue;
                if (t == 0x18 && g->m_challenge && g->m_mode == MODE_PRETZEL) continue;
                if (!allowOwned && OriginalSkillSlotWord(g, t) != 0) continue;
                if (g->IsSkillUnlocked(static_cast<SkillType>(t)) && m_scrollSkill->CanActivate(t)) {
                    candidates.push_back(t);
                }
            }
            int pick = RandI(0, static_cast<int>(candidates.size()) - 1, RAND_ITEM);
            // (no candidates: the original indexes a null vector and crashes; the port skips)
            if (!candidates.empty()) m_scrollSkill->Initialise(static_cast<SkillType>(candidates[pick]), nullptr);
            GLfloat2 p{kButtonPositions[index].x + 54.0f + 2.0f, kButtonPositions[index].y + 2.0f};
            m_scrollSkill->m_iconPosition = GLfloat2{p.x / 320.0f, p.y / 320.0f};
        } else {
            m_scrollIndex = -1;
        }
        m_items[3]->m_iconPosition = IconPosition(3, 2.0f);
        AddLabel(3, "");
        AddLabel(3, "");
        AddLabel(3, "");
    } else if (type == 2) {   // upgrade: one upgrade for each of four different items
        g->QueueSound(SOUND_UPGRADE);
        SetQuad(0x12, {15.0f, 3.0f}, {1.0f, 326.0f}, {123.0f, 29.0f}, 1.0f, 1.0f);
        SetQuad(0x13, {139.0f, 15.0f}, {1.0f, 360.0f}, {159.0f, 21.0f}, 1.0f, 1.0f);
        std::set<int> types;
        for (int t = 0; t < 5; ++t) types.insert(t);
        for (int i = 0; i < 4; ++i) {
            SetQuad(0x17 + i, {kButtonPositions[i].x + 73.0f, kButtonPositions[i].y}, {1.0f, 382.0f}, {53.0f, 53.0f},
                    1.0f, 1.0f);
            int n = RandI(0, static_cast<int>(types.size()) - 1, RAND_UPGRADE);
            int t;
            if (!NthOfSet(types, n, t)) continue;
            Item* item = g->GetItem(static_cast<ItemType>(t));
            item->m_iconPosition = IconPosition(i, 2.0f);
            types.erase(t);
            m_upgradeItems[i] = item;
            ItemUpgrade* up = m_upgrades[i];
            up->Initialise(item);
            up->m_iconPosition = IconPosition(i, 75.0f);
            AddLabel(i, up->m_name);
            AddLabel(i, up->GetDescription());
            AddLabel(i, up->GetStats());
        }
    } else if (type == 0) {   // level-up: up to two skill offers, the rest stat bonuses
        g->QueueSound(SOUND_LEVELUP);
        SetQuad(0x12, {15.0f, 3.0f}, {1.0f, 266.0f}, {121.0f, 29.0f}, 1.0f, 1.0f);
        SetQuad(0x13, {137.0f, 15.0f}, {251.0f, 266.0f}, {156.0f, 21.0f}, 1.0f, 1.0f);
        std::set<int> pool;   // skill types that can be offered
        if (g->m_skillCount == 4) {
            for (int i = 0; i < g->m_skillCount; ++i) pool.insert(g->m_skills[i]->m_type);
        } else {
            pool = Current(g).GetSkillPool();
        }
        bool allDone = true;   // every skill owned is maxed (level 10) or Wish
        for (int i = 0; i < g->m_skillCount; ++i) {
            Skill* s = g->m_skills[i];
            if (sprint || s->m_level > 9) {
                pool.erase(s->m_type);
            } else if (s->m_type == SKILL_WISH) {
                pool.erase(SKILL_WISH);
            } else {
                allDone = false;
            }
        }
        if (sprint) pool.erase(0x16);
        std::set<int> stats;
        for (int st = 0; st < 8; ++st) {
            // stat 7 only while one of the four token chances is below 100
            if (st != 7 || g->m_baseSkullTokenChance + g->m_bonusSkullTokenChance < 100 ||
                g->m_baseCoinTokenChance + g->m_bonusCoinTokenChance < 100 ||
                g->m_basePotionTokenChance + g->m_bonusPotionTokenChance < 100 ||
                g->m_baseShieldTokenChance + g->m_bonusShieldTokenChance < 100) {
                stats.insert(st);
            }
        }
        if (g->m_challenge && g->m_mode == MODE_PRETZEL) {
            pool.erase(0x18);
            pool.erase(7);
            stats.erase(6);
        }
        int poolSize = static_cast<int>(pool.size());
        int skillOffers;
        if (g->m_skillCount == 3 && allDone) {
            skillOffers = poolSize > 0 ? 1 : poolSize;
        } else {
            skillOffers = poolSize > 1 ? 2 : poolSize;
        }
        m_skillOfferCount = skillOffers;
        int offer = 0;   // keeps its value when a draw finds nothing (uninitialised at first in the original)
        for (int i = 0; i < skillOffers; ++i) {
            int n = RandI(0, static_cast<int>(pool.size()) - 1, RAND_SKILL);
            NthOfSet(pool, n, offer);
            if (i == 0 && g->m_level == 2 && Current(g).m_class != 0) {
                // the second level always offers the class's first skill (in Sprint not when it is Wish)
                SkillType first = Current(g).m_baseSkills[0];
                if (!(sprint && first == SKILL_WISH)) offer = first;
            }
            Skill* previous = nullptr;   // the owned skill of that type (the last one), to level up
            for (int k = 0; k < g->m_skillCount; ++k) {
                if (g->m_skills[k]->m_type == offer) previous = g->m_skills[k];
            }
            if (i == 0 && g->m_skillCount == 3 && previous == nullptr) {
                // a new fourth skill is offered: the second offer can only level up an owned one
                pool.clear();
                if (!sprint) {
                    for (int k = 0; k < g->m_skillCount; ++k) {
                        Skill* s = g->m_skills[k];
                        if (s->m_level < 10 && s->m_type != SKILL_WISH) pool.insert(s->m_type);
                    }
                }
            } else {
                pool.erase(offer);
            }
            Skill* skill = m_skills[i];
            skill->Initialise(static_cast<SkillType>(offer), previous);
            skill->m_iconPosition = IconPosition(i, 2.0f);
            AddLabel(i, skill->m_name);
            std::vector<std::string> lines;
            float width = ((kButtonPositions[0].x + 53.0f + 5.0f) / -320.0f + 0.984375f) - 0.071875f;
            g->m_fonts[0].SplitLines(skill->GetDescription(), 0.09f, width, lines);
            for (const std::string& line : lines) AddLabel(i, line.c_str());
            if (lines.size() < 2) AddLabel(i, "");
            AddLabel(i, skill->GetStats(0));
        }
        int stat = 0;   // likewise
        for (int i = skillOffers; i < 4; ++i) {
            int n = RandI(0, static_cast<int>(stats.size()) - 1, RAND_SKILL);
            NthOfSet(stats, n, stat);
            stats.erase(stat);
            Skill* skill = m_skills[i];
            skill->Initialise(static_cast<StatType>(stat));
            skill->m_iconPosition = IconPosition(i, 2.0f);
            AddLabel(i, skill->m_name);
            for (int k = 0; k < skill->m_statsTextCount; ++k) AddLabel(i, skill->GetStats(k));
        }
    }
    UpdateLabels();
}

bool FoldoutMenu::CursorInButton(int quad) const {
    const QuadVertex* v = &m_vertices[quad * 4];
    float tx = m_game->m_touchX;
    float ty = m_game->m_touchY;
    return v[0].position.x + m_offset.x < tx && v[0].position.y + m_offset.y < ty && ty < m_offset.y + v[2].position.y;
}

bool FoldoutMenu::CursorInQuad(int quad) const {
    const QuadVertex* v = &m_vertices[quad * 4];
    float tx = m_game->m_touchX;
    float ty = m_game->m_touchY;
    return m_offset.x + v[0].position.x < tx && tx < m_offset.x + v[2].position.x && v[0].position.y + m_offset.y < ty &&
           ty < m_offset.y + v[2].position.y;
}

void FoldoutMenu::HighlightButton(int quad, int row, bool on) {
    const GLfloat2 size{53.0f, 53.0f};
    const GLfloat2 uv = on ? GLfloat2{55.0f, 382.0f} : GLfloat2{1.0f, 382.0f};
    SetQuadUV(quad, uv, size);
    if (m_type == 2) {
        SetQuadUV(quad + 9, uv, size);   // the upgrade's icon frame
    } else if (m_type == 1 && quad - kFirstOption == m_scrollIndex) {
        SetQuadUV(0x17, uv, size);       // the scroll's frame
    }
    if (!on) return;
    float y = m_vertices[quad * 4].position.y * 320.0f - 1.0f;
    SetQuadPos(row * 3 + 8, 17.0f, y);
    SetQuadPos(row * 3 + 9, 20.0f, y);
    SetQuadPos(row * 3 + 10, 291.0f, y);
    if (m_type == 1) {
        // the last three labels (option 3's) show the item the offer would replace
        Item* current = m_game->GetItem(m_items[quad - kFirstOption]->m_type);
        std::string replaces = std::string("Replaces: ") + current->m_name;   // static buffer 0x000892fc
        m_labels[m_labelCount - 3]->SetText(replaces.c_str(), -1);
        m_labels[m_labelCount - 2]->SetText(current->m_statsText1.c_str(), -1);
        m_labels[m_labelCount - 1]->SetText(current->m_statsText2.c_str(), -1);
        UpdateLabels();
    }
}

// Touches while the menu is idle: a press selects/deselects an option or presses the confirm button
// (when enough options are selected); releasing on the confirm button starts folding in.
void FoldoutMenu::ProcessInput() {
    int touch = m_game->m_touchState;
    if (touch == TOUCHING) {
        if (m_lastTouchState != NOT_TOUCHING) {   // only a new press counts
            m_lastTouchState = TOUCHING;
            return;
        }
        int type = m_type;
        int sel = m_selected[0];
        if (m_confirmEnabled && CursorInQuad(kConfirmQuad)) {
            m_game->QueueSound(SOUND_CLICK);
            m_confirmPressed = true;
            SetQuadColour(kConfirmQuad, 200, 200, 200, 0xff);
        } else {
            for (int b = kFirstOption; b < kFirstOption + 4; ++b) {
                if (type == 1 && b == kFirstOption + 3) continue;   // the shop has three offers
                if (!CursorInButton(b)) continue;
                if (type == 1) {
                    m_game->QueueSound(SOUND_COIN_START);
                    if (m_scrollIndex == b - kFirstOption) {
                        GLfloat2 p{(kButtonPositions[m_scrollIndex].x + 54.0f) / 320.0f,
                                   (kButtonPositions[m_scrollIndex].y + 26.0f) / 320.0f};
                        GLfloat2 at{p.x + m_offset.x, p.y + m_offset.y};
                        m_interface->ShowScrollTooltip(m_scrollSkill->m_type, at);
                    }
                } else if (type == 2) {
                    m_game->QueueSound(SOUND_UP);
                } else if (type == 0) {
                    if (m_selected[0] == -1 || m_selected[1] == -1 || b == m_selected[0] || b == m_selected[1]) {
                        m_game->QueueSound(SOUND_XP);
                    }
                }
                if (m_selected[0] == b) {   // deselect
                    HighlightButton(b, 0, false);
                    m_selected[0] = -1;
                    sel = -1;
                } else if (m_selected[1] == b) {
                    HighlightButton(b, 1, false);
                    m_selected[1] = -1;
                    sel = m_selected[0];
                } else if (m_selected[0] != -1 && type == 0) {
                    if (m_selected[1] == -1) {   // the level-up's second choice
                        HighlightButton(b, 1, true);
                        m_selected[1] = b;
                    }
                    // (a third press is ignored)
                    sel = m_selected[0];
                } else {
                    if (m_selected[0] != -1) HighlightButton(m_selected[0], 0, false);
                    m_selected[0] = b;
                    HighlightButton(b, 0, true);
                    sel = b;
                }
                break;
            }
        }
        if (type == 0) {
            m_confirmEnabled = m_selected[0] != -1 && m_selected[1] != -1;
        } else {
            m_confirmEnabled = sel != -1;
        }
        m_lastTouchState = TOUCHING;   // (the touch state cannot change during the call)
        return;
    }
    if (touch == FINISHED_TOUCHING && m_confirmPressed) {
        if (!CursorInQuad(kConfirmQuad)) {
            m_confirmPressed = false;
        } else {
            m_opening = false;
            m_animTime = 0.0f;
            if (m_type == 1) {
                m_game->QueueSound(SOUND_SHOP);
            } else if (m_type == 2) {
                m_game->QueueSound(SOUND_UPGRADE);
            } else if (m_type == 0) {
                m_game->QueueSound(SOUND_LEVELUP);
            }
        }
        SetQuadColour(kConfirmQuad, 0xff, 0xff, 0xff, 0xff);
    }
    m_interface->HideTooltip();
    m_lastTouchState = touch;
}

// The fold animation (0.3 s) with the board shade; when folded in, the choice takes effect.
float FoldoutMenu::RestX() const { return m_game->IsWide() ? 0.071875f - 13.0f / 320.0f : 0.071875f; }

void FoldoutMenu::Process(float dt) {
    if (m_animTime < 0.0f) {
        m_offset.x = RestX();   // port: follows a layout change while open
        ProcessInput();
        return;
    }
    float f = m_animTime / 0.3f;
    if (!m_opening) f = 1.0f - f;
    m_shade.SetColour(GLcolour4{0, 0, 0, static_cast<uint8_t>(FloatToUintArm(f * 180.0f))});
    float t = dt + m_animTime;
    m_animTime = t;
    if (t < 0.3f) {
        m_offset.x = m_opening ? Interpolate(RestX(), 1.0f, t / 0.3f) : Interpolate(1.0f, RestX(), t / 0.3f);
        return;
    }
    if (m_opening) {
        m_offset.x = RestX();
        m_animTime = -1.0f;
        return;
    }
    m_active = false;
    m_animTime = -1.0f;
    int s0 = m_selected[0] - kFirstOption;
    if (m_type == 1) {
        m_items[s0] = m_game->SetItem(m_items[s0]);   // the replaced item becomes the menu's object
        m_game->UpdateStats();
        if (m_scrollIndex >= 0 && m_scrollIndex == s0) {
            m_scrollSkill->Activate();
            if (Current(m_game).m_race == RACE_SAUREN) m_game->QueueAchievement(ACH_STAY_A_WHILE, 2.0f);
        }
    } else if (m_type == 2) {
        m_upgradeItems[s0]->AddUpgrade(m_upgrades[s0]);
        m_game->UpdateStats();
    } else if (m_type == 0) {
        m_skills[s0] = m_game->SetSkill(m_skills[s0]);
        int s1 = m_selected[1] - kFirstOption;
        m_skills[s1] = m_game->SetSkill(m_skills[s1]);
    }
}

// The next label (a new one when all are in use) for `option`: its first line at scale 0.11, the
// others at 0.09.
void FoldoutMenu::AddLabel(int option, const char* text) {
    ++m_optionLabelCounts[option];
    if (m_labelCount == static_cast<int>(m_labels.size())) {
        Label* l = new Label(&m_game->m_fonts[0]);
        m_labels.push_back(l);
        l->m_spacing = 2.0f;
    }
    Label* l = m_labels[m_labelCount];
    l->SetText(text, -1);
    float scale = m_optionLabelCounts[option] > 1 ? 0.09f : 0.11f;
    l->m_scale = GLfloat2{scale, scale};
    ++m_labelCount;
}

// Lays the labels out: each option's first line (white, the name) next to its button, the others
// below it (grey). The shop's scroll offer is shifted right past the scroll icon, its lines shrunk
// to fit; the level-up's skill offers show their 4th line (the stats) small at the top right; a
// stat offer's further lines go right of its widest line. In the shop the stat texts are coloured
// green/red where the offer's stat differs from the current item's (and the "Replaces" lines the
// other way round). Option 3's lines in the shop (the replaced item) are darker.
void FoldoutMenu::UpdateLabels() {
    const Font& font = m_game->m_fonts[0];
    int lineHeight = font.GetLineHeight();
    float line = static_cast<float>(lineHeight - font.GetBase()) / static_cast<float>(lineHeight);
    // (option, line in it) of label i: subtracts the per-option counts
    auto locate = [this](int i, int& option, int& index) {
        for (option = 0; option < 4; ++option) {
            if (i < m_optionLabelCounts[option]) {
                index = i;
                return true;
            }
            i -= m_optionLabelCounts[option];
        }
        return false;   // (never: the counts add up to m_labelCount)
    };
    float scrollScale = 0.09f;
    if (m_type == 1 && m_scrollIndex >= 0) {
        // the scroll offer's lines (but the first) shrink so the widest fits
        for (int i = 0; i < m_labelCount; ++i) {
            int option, index;
            if (!locate(i, option, index) || option > m_scrollIndex) break;
            if (option != m_scrollIndex || index == 0) continue;
            float x = kButtonPositions[option].x + 53.0f;
            Label* l = m_labels[i];
            l->m_scale = GLfloat2{0.09f, 0.09f};
            float width = l->m_width * l->m_scale.x;
            float room = (0.984375f - ((x + 5.0f) / 320.0f + 0.16875f)) - 0.071875f;
            if (room < width) {
                float s = (room * 0.09f) / width;
                if (scrollScale > s) scrollScale = s;
            }
        }
    }
    float top = line * 0.11f;
    float widest = 0.0f;   // of the option's lines 1 and 2
    for (int i = 0; i < m_labelCount; ++i) {
        int option = 3, index = 0;
        locate(i, option, index);
        Label* l = m_labels[i];
        if (index == 0) {
            widest = 0.0f;
        } else if (index <= 2) {
            float w = l->m_width * l->m_scale.x;
            if (w > widest) widest = w;
        }
        float x = ((kButtonPositions[option].x + 53.0f) + 5.0f) / 320.0f;
        float scale = m_scrollIndex == option ? scrollScale : 0.09f;
        float y0 = kButtonPositions[option].y / 320.0f + top;
        float step = scale * line;
        int row = index;
        bool scaled = false;
        if (index > 2) {
            if (m_type == 0 && option < m_skillOfferCount) {   // a skill's stats: small, top right
                l->m_scale = GLfloat2{0.0720000044f, 0.0720000044f};   // 0x3d9374bd, one bit above 0.072f
                row = 0;
                x = 0.9125f - l->m_width * l->m_scale.x;
            } else {                                          // right of the widest line
                row = index - 2;
                x = x + (widest + step * 0.5f);
            }
        }
        if (m_type == 2) {
            x = x + 0.228125f;   // right of the upgrade's icon
        } else if (m_type == 1 && m_scrollIndex == option) {
            x = x + 0.16875f;    // right of the scroll's icon
            scaled = row > 0;
        }
        if (scaled) l->m_scale = GLfloat2{scale, scale};
        l->m_position.x = x;
        l->m_position.y = y0 + step * static_cast<float>(row);
        bool replaced = m_type == 1 && option == 3;
        if (row == 0) {
            uint8_t c = replaced ? 0xaa : 0xff;
            l->m_colour = GLcolour4{c, c, c, 0xff};
            l->UpdateColour();
            continue;
        }
        uint8_t c = replaced ? 0x78 : 0xc8;
        l->m_colour = GLcolour4{c, c, c, 0xff};
        l->UpdateColour();
        if (m_type != 1) continue;
        // the shop's stat lines: row 1 is m_statsText1, row 2 m_statsText2
        Item* a;   // whose stat texts these are
        Item* b;   // compared with
        if (option < 3) {
            a = m_items[option];
            b = m_game->GetItem(a->m_type);
        } else {
            if (m_selected[0] < 0) continue;
            Item* offer = m_items[m_selected[0] - kFirstOption];
            a = m_game->GetItem(offer->m_type);
            b = offer;
        }
        for (int k = 0; k < 20; ++k) {
            if (a->m_stats[k] == b->m_stats[k] || a->m_statTextLine[k] + 1 != row) continue;
            GLcolour4 colour = a->m_stats[k] > b->m_stats[k] ? GLcolour4{0, 0xff, 0, 0xff} : GLcolour4{0xff, 0, 0, 0xff};
            l->SetSubColour(static_cast<unsigned>(a->m_statTextRange[k][0]),
                            static_cast<unsigned>(a->m_statTextRange[k][1]), colour);
        }
    }
}

// The board shade, then (shifted by m_offset) the frame and buttons, the offer icons and the labels.
void FoldoutMenu::Render() {
    sdlBindTexture(GL_TEXTURE_2D, 0);
    m_shade.Render();
    sdlPushMatrix();
    sdlTranslatef(m_offset.x, m_offset.y, 0.0f);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int q = 0; q < kQuadCount; ++q) {
        if (!m_confirmEnabled && (q == 6 || q == 7)) continue;   // the confirm button
        if (q >= 8 && q <= 10 && m_selected[0] < 0) continue;    // the selection marks
        if (q >= 11 && q <= 13 && m_selected[1] < 0) continue;
        bool draw;
        if (m_type == 1 && q == kFirstOption + 3) {
            draw = false;                                        // the shop has three offers
        } else if (q >= 0x14 && q <= 0x16) {
            draw = m_type == 1;                                  // the separator
        } else if (q < 0x17) {
            draw = true;
        } else if (q >= kRightFrame) {
            draw = m_game->IsWide();                             // port: the frame's right side
        } else {   // 23..30: the scroll's frame (shop) or the upgrades' frames and arrows
            draw = m_type == 2 || (m_type == 1 && m_scrollIndex >= 0 && q == 0x17);
        }
        if (draw) sdlDrawArrays(GL_TRIANGLE_FAN, q * 4, 4);
    }
    if (m_type == 1) {
        sdlBindTexture(GL_TEXTURE_2D, 3);
        for (int i = 0; i < 3; ++i) m_items[i]->RenderIcon(true);
        if (m_selected[0] >= 0) {
            // the item the selection would replace, in option 3's place (this moves that item's icon)
            Item* current = m_game->GetItem(m_items[m_selected[0] - kFirstOption]->m_type);
            current->m_iconPosition = m_items[3]->m_iconPosition;
            current->RenderIcon(true);
        }
        if (m_scrollIndex >= 0) {
            sdlBindTexture(GL_TEXTURE_2D, 1);
            m_scrollSkill->RenderIcon(false);
        }
    } else if (m_type == 2) {
        sdlBindTexture(GL_TEXTURE_2D, 3);
        for (int i = 0; i < 4; ++i) m_upgradeItems[i]->RenderIcon(true);
        sdlBindTexture(GL_TEXTURE_2D, 1);
        for (int i = 0; i < 4; ++i) m_upgrades[i]->RenderIcon(true);
    } else if (m_type == 0) {
        for (int i = 0; i < 4; ++i) {
            sdlBindTexture(GL_TEXTURE_2D, 1);
            m_skills[i]->RenderIcon(true);
        }
    }
    for (int i = 0; i < m_labelCount; ++i) {
        // the shop's "Replaces" lines only while an offer is selected
        if (m_type == 1 && m_selected[0] < 0 && i >= m_labelCount - 3) continue;
        m_labels[i]->Render();
    }
    sdlPopMatrix();
}
