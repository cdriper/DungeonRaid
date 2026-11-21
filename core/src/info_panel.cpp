#include "info_panel.h"

#include <cmath>
#include <string>

#include "character_class.h"
#include "dr_gl.h"
#include "game.h"
#include "skill.h"
#include "tile.h"
#include "util.h"

namespace {

constexpr float kH = Game::kPanelHeight;  // the panel's height (screen units; the board is 1 x 1.5)
constexpr float kInset = 0.03f;
constexpr float kFrameWidth = 0.004f;    // the thin frame (tall canvas): its lines
constexpr float kFrameInset = 0.006f;    // from the panel's edges (clear of rounded screen corners)
constexpr float kWidth = 1.0f - 2.0f * kInset;
// The text sizes, largest first: the panel uses the first at which nothing is cut (4 lines at the
// normal size, 5 at the smaller ones). Lines are 0.744 of the size apart (0.058 at 0.078), the
// first baseline one line below the panel's top.
struct Fit {
    float scale;
    size_t lines;
};
const Fit kFits[] = {{0.078f, 4}, {0.071f, 5}, {0.064f, 5}};
constexpr float kLineFactor = 0.058f / 0.078f;

const GLcolour4 kTitle{0xff, 0xd7, 0x6e, 0xff};
const GLcolour4 kText{0xe8, 0xe8, 0xf0, 0xff};
const GLcolour4 kDim{0x9a, 0x9a, 0xb0, 0xff};
const GLcolour4 kGood{0x78, 0xff, 0x78, 0xff};
const GLcolour4 kBad{0xff, 0x5a, 0x5a, 0xff};

std::string N(int v) { return std::to_string(v); }

int MaxHealth(const Game* g) { return g->m_baseMaxHealth + g->m_bonusMaxHealth; }

// The enemies' next attack as InitialiseFlares adds it up (unfrozen skulls not refilled this
// turn); `skipKilled`: without the chain's skulls it would kill.
struct Attack {
    int enemies = 0;
    int attackers = 0;
    int total = 0;
    int poison = 0;
};
Attack CountAttack(Game* g, bool skipKilled) {
    Attack a;
    for (auto& row : g->m_board) {
        for (Tile& t : row) {
            if (t.m_type != TILE_SKULL) continue;
            if (skipKilled && t.m_selected && t.m_crossVisible && !g->m_chainSet.count(&t)) continue;
            a.enemies += 1;
            if (t.m_spawnedThisTurn || t.m_frozen) continue;
            const int attack = t.GetAttack();
            if (attack > 0) a.attackers += 1;
            a.total += attack;
            if (t.m_enemyType == ENEMY_POISONOUS) a.poison += attack;
        }
    }
    return a;
}

bool NoAttackThisTurn(Game* g) {   // ProcessInput: the Unexpected perk spares the first 5 turns
    return g->m_turn <= 4 && g->m_classes[g->m_currentClass].HavePerk(PERK_UNEXPECTED);
}

// "Armour 10 blocks 10 -> 13 damage to health", red when it would kill
InfoPanel::Line HealthDamageLine(Game* g, int attack, const char* lead) {
    InfoPanel::Line l;
    const int blocked = attack < g->m_armour ? attack : g->m_armour;
    const int damage = attack - blocked;
    l.Add(lead, kText);
    l.Add(N(attack), kText);
    l.Add(", armour blocks " + N(blocked) + " -> ", kText);
    if (damage >= g->m_health && damage > 0) {
        l.Add(N(damage) + " damage: LETHAL", kBad);
    } else {
        l.Add(N(damage) + " damage", damage > 0 ? kBad : kGood);
        l.Add(" to health", kText);
    }
    return l;
}

}  // namespace

InfoPanel::Line& InfoPanel::Line::Add(const std::string& part, GLcolour4 colour) {
    const unsigned from = static_cast<unsigned>(text.size());
    text += part;
    spans.push_back(Span{from, static_cast<unsigned>(text.size()), colour});
    return *this;
}

bool InfoPanel::Line::operator==(const Line& o) const {
    if (text != o.text || spans.size() != o.spans.size()) return false;
    for (size_t i = 0; i < spans.size(); ++i) {
        const Span& a = spans[i];
        const Span& b = o.spans[i];
        if (a.from != b.from || a.to != b.to || a.colour.r != b.colour.r || a.colour.g != b.colour.g ||
            a.colour.b != b.colour.b || a.colour.a != b.colour.a) {
            return false;
        }
    }
    return true;
}

InfoPanel::InfoPanel(Game* game) : m_game(game) {
    m_background.SetSize(GLfloat2{1.0f, kH});
    m_background.SetColour(GLcolour4{18, 18, 32, 0xff});
    m_edge.SetSize(GLfloat2{1.0f, 0.004f});
    m_edge.SetColour(GLcolour4{95, 95, 150, 0xff});
    for (Quad& q : m_frame) q.SetColour(GLcolour4{95, 95, 150, 0xff});
}

// In the board's coordinates: below it (y 1.5 ..) with the setting, else above it (y -kH .. 0;
// Game::Render moves the board down by kH). The horizontal layout: its own card under the skills bar
// and HUD (drawn from y 1.5 .., Game::kWidePanelH tall).
float InfoPanel::PanelTop() const { return m_game->IsWide() || m_game->m_panelAtBottom ? 1.5f : -kH; }

float InfoPanel::PanelHeight() const { return m_game->IsWide() ? Game::kWidePanelH : kH; }

bool InfoPanel::InPanel(float y) const { return y >= PanelTop() && y < PanelTop() + PanelHeight(); }

float InfoPanel::TextWidth(const std::string& text) {
    m_measure.m_font = &m_game->m_fonts[0];
    m_measure.m_spacing = 2.0f;
    m_measure.SetText(text.c_str(), -1);
    return m_measure.m_width * m_scale;
}

// Greedy word wrap to the panel's width; the last allowed line ends in "..." when text is left
// (m_cut: a smaller text size is tried).
void InfoPanel::AddWrapped(std::vector<Line>& lines, const char* text, GLcolour4 colour, size_t maxLines) {
    if (maxLines == 0) {
        if (*text) m_cut = true;
        return;
    }
    const float width = kWidth;
    std::vector<std::string> words;
    std::string word;
    for (const char* p = text; *p; ++p) {
        if (*p == ' ' || *p == '\n') {
            if (!word.empty()) words.push_back(word);
            word.clear();
        } else {
            word += *p;
        }
    }
    if (!word.empty()) words.push_back(word);
    std::string line;
    size_t added = 0;
    for (size_t i = 0; i < words.size(); ++i) {
        const std::string next = line.empty() ? words[i] : line + " " + words[i];
        if (line.empty() || TextWidth(next) <= width) {
            line = next;
            continue;
        }
        if (added + 1 == maxLines) {   // no room for the rest
            m_cut = true;
            while (!line.empty() && TextWidth(line + "...") > width) line.pop_back();
            line += "...";
            Line l;
            lines.push_back(l.Add(line, colour));
            return;
        }
        Line l;
        lines.push_back(l.Add(line, colour));
        ++added;
        line = words[i];
    }
    if (!line.empty() && added < maxLines) {
        Line l;
        lines.push_back(l.Add(line, colour));
    }
}

InfoPanel::Pick InfoPanel::PickAt(const GLfloat2& p) {
    Game* g = m_game;
    Pick pick;
    for (int i = 0; i < g->m_skillCount && i < 4; ++i) {
        if (g->m_skills[i] != nullptr && g->m_interface.m_skillButtons[i].ContainsPoint(p)) {
            pick.kind = Pick::SKILL;
            pick.skill = i;
            return pick;
        }
    }
    if (Tile* t = g->GetTileAtPosition(p)) {
        pick.kind = Pick::TILE;
        pick.row = t->m_row;
        pick.col = t->m_column;
    }
    return pick;
}

// A new touch remembers what it began on (and drops the kept info); its release keeps a skill
// that was not cast, or the tile under the finger after a tap (a chain of at most one tile).
// A new turn drops the kept info. (The game consumes a release in the same frame, so a release
// is seen as TOUCHING turning into anything else.)
void InfoPanel::Track() {
    Game* g = m_game;
    if (g->m_turn != m_turn) {
        m_turn = g->m_turn;
        m_kept = Pick();
    }
    const bool touching = g->m_touchState == TOUCHING;
    if (touching) {
        const GLfloat2 p{g->m_touchX, g->m_touchY};
        if (!m_wasTouching) {
            m_press = PickAt(p);
            m_kept = Pick();
            m_maxChain = 0;
            m_pressCharge = m_press.kind == Pick::SKILL ? g->m_skills[m_press.skill]->m_charge : 0;
        }
        if (g->m_chain.size() > m_maxChain) m_maxChain = g->m_chain.size();
        m_lastTouch = p;
    } else if (m_wasTouching) {
        if (m_press.kind == Pick::SKILL) {
            Skill* s = m_press.skill < g->m_skillCount ? g->m_skills[m_press.skill] : nullptr;
            const bool cast = s == nullptr || s->m_charge < m_pressCharge;
            m_kept = cast ? Pick() : m_press;
        } else if (m_press.kind == Pick::TILE && m_maxChain <= 1) {
            const Pick last = PickAt(m_lastTouch);
            m_kept = last.kind == Pick::TILE ? last : Pick();
        } else {
            m_kept = Pick();
        }
        m_press = Pick();
    }
    m_wasTouching = touching;
}

bool InfoPanel::BuildPick(std::vector<Line>& lines, const Pick& pick) {
    Game* g = m_game;
    if (pick.kind == Pick::SKILL) {
        if (pick.skill < 0 || pick.skill >= g->m_skillCount || g->m_skills[pick.skill] == nullptr) return false;
        BuildSkill(lines, pick.skill);
        return true;
    }
    if (pick.kind == Pick::TILE) {
        BuildTile(lines, &g->m_board[pick.row][pick.col]);
        return !lines.empty();
    }
    return false;
}

// Priority: a touch (the pressed skill, a chain of 2+ tiles, else the tile under the finger), the
// mouse over the panel (the status), the mouse hover, the info kept after a tap, the status.
// The content at the largest text size at which nothing is cut (else the smallest, cut).
std::vector<InfoPanel::Line> InfoPanel::BuildLines() {
    Game* g = m_game;
    // a menu over the board (level-up, shop, upgrade, pause, stats): its touches and the mouse are
    // on the menu, not the tiles under it; just the status, and no info is kept from it
    const bool menu = g->HudMenuOpen();
    if (menu) {
        m_press = Pick();
        m_kept = Pick();
        m_wasTouching = g->m_touchState == TOUCHING;   // a touch going on when it closes is not a tap
    } else {
        Track();
    }
    std::vector<Line> lines;
    for (const Fit& fit : kFits) {
        m_scale = fit.scale;
        m_maxLines = fit.lines;
        if (g->IsWide()) {   // the tall panel of the horizontal layout: as many lines as fit
            m_maxLines = static_cast<size_t>((PanelHeight() - 0.03f) / (fit.scale * kLineFactor));
        }
        m_cut = false;
        lines.clear();
        if (menu) {
            BuildStatus(lines);
        } else {
            BuildContent(lines);
        }
        if (!m_cut && lines.size() <= m_maxLines) break;
    }
    if (lines.size() > m_maxLines) lines.resize(m_maxLines);
    return lines;
}

void InfoPanel::BuildContent(std::vector<Line>& lines) {
    Game* g = m_game;
    if (g->m_touchState == TOUCHING) {
        if (m_press.kind == Pick::SKILL && BuildPick(lines, m_press)) {
        } else if (g->m_chain.size() >= 2) {
            BuildSelection(lines);
        } else if (Tile* t = g->GetTileAtPosition(GLfloat2{g->m_touchX, g->m_touchY})) {
            BuildTile(lines, t);
        } else if (!g->m_chain.empty()) {
            BuildSelection(lines);
        }
    } else if (g->m_hoverActive && g->m_touchState == NOT_TOUCHING && InPanel(g->m_hoverY)) {
        // the mouse over the panel itself: the status (not the kept info)
    } else if (g->m_hoverActive && g->m_touchState == NOT_TOUCHING && BuildHover(lines)) {
    } else {
        BuildPick(lines, m_kept);
    }
    if (lines.empty()) BuildStatus(lines);
}

void InfoPanel::BuildStatus(std::vector<Line>& lines) {
    Game* g = m_game;
    const Attack a = CountAttack(g, false);
    Line title;
    lines.push_back(title.Add("Enemies on the board: " + N(a.enemies), kTitle));
    if (a.enemies == 0) {
        AddTurnLines(lines);
        return;
    }
    if (NoAttackThisTurn(g)) {
        Line l;
        lines.push_back(l.Add("No attack this turn (Unexpected)", kGood));
    } else {
        Line l;
        l.Add("Their attack: " + N(a.total), kText);
        if (a.poison > 0) l.Add("  (poison " + N(a.poison) + ")", kText);
        if (a.attackers < a.enemies) l.Add("  " + N(a.enemies - a.attackers) + " not attacking", kDim);
        lines.push_back(l);
        lines.push_back(HealthDamageLine(g, a.total, "Next attack "));
    }
    AddTurnLines(lines);
}

// "Turn 37   Special in 4 turns   Trophy in 6 kills" (dim; the trophy part on a line of its own
// when it doesn't fit). The special: the promotion timer counts down after each turn's refill and,
// at 0, the next new skull is promoted (IsTimeToPromote), so N turns = timer + 1; the Sprint
// challenge promotes on a fixed turn instead. The trophy: Game::TrophyKillsLeft (nothing on Easy).
void InfoPanel::AddTurnLines(std::vector<Line>& lines) {
    Game* g = m_game;
    std::string turn = "Turn " + N(g->m_turn + 1) + "   ";
    const bool sprint = g->m_challenge && g->m_mode == MODE_SPRINT;
    if (g->m_timeToPromotion == 0) {
        turn += sprint ? "Special: next turn" : "Special: next skull";
    } else if (!sprint) {
        turn += "Special in " + N(g->m_timeToPromotion + 1) + " turns";
    } else {
        const bool zealous = g->m_classes[g->m_currentClass].HaveFlaw(FLAW_ZEALOUS);
        const int first = zealous ? 3 : 4, period = zealous ? 9 : 10;
        int k = 1;
        while (k < period && (g->m_turn + k < first || (g->m_turn + k - first) % period != 0)) ++k;
        turn += k == 1 ? "Special: next turn" : "Special in " + N(k) + " turns";
    }
    std::string trophy;
    const int kills = g->TrophyKillsLeft();
    if (kills == 0) {
        trophy = "No more trophies this game";
    } else if (kills > 0 && sprint) {
        trophy = "Trophy in ~" + N(2 * kills) + " kills";
    } else if (kills > 0) {
        trophy = "Trophy in " + N(kills) + (kills == 1 ? " kill" : " kills");
    }
    Line l;
    if (trophy.empty()) {
        lines.push_back(l.Add(turn, kDim));
    } else if (TextWidth(turn + "   " + trophy) <= kWidth) {
        lines.push_back(l.Add(turn + "   " + trophy, kDim));
    } else {
        lines.push_back(l.Add(turn, kDim));
        Line t;
        lines.push_back(t.Add(trophy, kDim));
    }
}

// What the chain gives, with ProcessResourceShadows' and UpdateCrossVisibility's rules (passed-over
// tiles count for nothing; the Cocky flaw voids resources on chains of up to 3).
void InfoPanel::BuildSelection(std::vector<Line>& lines) {
    Game* g = m_game;
    const int size = static_cast<int>(g->m_chain.size());
    Tile* first = g->m_chain.front();
    if (size == 1 && first->m_type == TILE_SPECIAL && first->m_specialType == SPECIAL_TILE_TROPHY) {
        Line t, l;
        lines.push_back(t.Add("Trophy", kTitle));
        lines.push_back(l.Add("Release to unlock a class or level up this one", kGood));
        return;
    }
    const bool cocky = size <= 3 && g->m_classes[g->m_currentClass].HaveFlaw(FLAW_COCKY);
    int plainPotions = 0, shields = 0, coins = 0, skulls = 0, kills = 0, survivors = 0;
    for (Tile* t : g->m_chain) {
        if (g->m_chainSet.count(t)) continue;
        switch (t->m_type) {
            case TILE_POTION:
                if (!cocky && !t->m_explosivePotion && !t->m_manaPotion && !t->m_acidPotion) ++plainPotions;
                break;
            case TILE_SHIELD:
                if (!cocky && !t->m_broken) ++shields;
                break;
            case TILE_COIN:
                if (!cocky) ++coins;
                break;
            case TILE_SKULL:
                ++skulls;
                if (t->m_crossVisible) ++kills; else ++survivors;
                break;
            default:
                break;
        }
    }
    const int sprint = (g->m_challenge && g->m_mode == MODE_SPRINT) ? 2 : 1;
    const bool attack = first->m_type == TILE_SKULL || first->m_type == TILE_SWORD;
    Line title;
    const char* kind = attack ? "Attack" : first->m_type == TILE_POTION ? "Potions"
                                         : first->m_type == TILE_SHIELD ? "Shields"
                                         : first->m_type == TILE_COIN   ? "Coins"
                                                                        : "Selection";
    title.Add(std::string(kind) + ": " + N(size) + (size == 1 ? " tile" : " tiles"), kTitle);
    if (attack) title.Add(",  " + N(g->m_chainDamage) + " damage", kTitle);
    lines.push_back(title);
    if (size < 3) {
        Line l;
        lines.push_back(l.Add("Select 3 or more to match", kDim));
        return;
    }
    if (cocky) {
        Line l;
        lines.push_back(l.Add("Cocky: chains of 3 give no resources", kDim));
    }
    if (attack) {
        Line k;
        if (skulls == 0) {
            k.Add("No enemies in the chain", kDim);
        } else {
            k.Add("Kills: " + N(kills), kills > 0 ? kGood : kText);
            if (survivors > 0) k.Add("    Survive: " + N(survivors), kBad);
        }
        lines.push_back(k);
        if (kills > 0) {
            const int xp = FloatToIntArm(static_cast<float>(kills * sprint) * (g->m_xpBonusRate + 2.0f));
            Line x;
            lines.push_back(x.Add("XP +" + N(xp) + "  (" + N(g->m_xp) + " -> " + N(g->m_xp + xp) + " / 100)", kText));
        }
        if (NoAttackThisTurn(g)) {
            Line l;
            lines.push_back(l.Add("No enemy attack this turn (Unexpected)", kGood));
        } else {
            lines.push_back(HealthDamageLine(g, CountAttack(g, true).total, "Then their attack "));
        }
        return;
    }
    if (first->m_type == TILE_POTION) {
        const int heal = plainPotions * (g->m_bonusHealAmount + g->m_baseHealAmount) * (g->m_doubleHealth ? 2 : 1);
        const int maxHealth = MaxHealth(g);
        const int to = g->m_health + heal < maxHealth ? g->m_health + heal : maxHealth;
        Line l;
        l.Add("Health +" + N(heal) + "  (" + N(g->m_health) + " -> " + N(to) + " / " + N(maxHealth) + ")",
              heal > 0 ? kGood : kText);
        if (g->m_health + heal > maxHealth) l.Add("  " + N(g->m_health + heal - maxHealth) + " over max", kDim);
        lines.push_back(l);
    } else if (first->m_type == TILE_SHIELD) {
        const int perShield = (g->m_bonusArmourAmount + g->m_baseArmourAmount) * (g->m_doubleArmour ? 2 : 1);
        const int gain = shields * perShield;
        const int to = g->m_armour + gain < g->m_maxArmour ? g->m_armour + gain : g->m_maxArmour;
        Line l;
        lines.push_back(l.Add("Armour +" + N(to - g->m_armour) + "  (" + N(g->m_armour) + " -> " + N(to) + " / " +
                                  N(g->m_maxArmour) + ")",
                              to > g->m_armour ? kGood : kText));
        const float missing = static_cast<float>(g->m_maxArmour - g->m_armour);
        const float needed = perShield > 0 ? std::ceil(missing / static_cast<float>(perShield)) : 0.0f;
        int extra = FloatToIntArm(static_cast<float>(shields) - needed);
        if (extra < 0) extra = 0;
        const int up = FloatToIntArm(static_cast<float>(extra * sprint) * (g->m_upgradeBonusRate + 2.0f) *
                                     (g->m_doubleArmour ? 2.0f : 1.0f));
        Line u;
        lines.push_back(u.Add("Upgrade +" + N(up) + "  (" + N(g->m_upgradePoints) + " -> " + N(g->m_upgradePoints + up) +
                                  " / 100)",
                              up > 0 ? kGood : kDim));
    } else if (first->m_type == TILE_COIN) {
        const int gold = FloatToIntArm(static_cast<float>(coins * sprint) * (g->m_goldBonusRate + 2.0f) *
                                       (g->m_doubleGold ? 2.0f : 1.0f));
        Line l;
        lines.push_back(l.Add("Gold +" + N(gold) + "  (" + N(g->m_gold) + " -> " + N(g->m_gold + gold) + ")",
                              gold > 0 ? kGood : kText));
    }
}

bool InfoPanel::BuildHover(std::vector<Line>& lines) {
    Game* g = m_game;
    if (g->m_hoverX < 0.0f) return false;   // GetTileAtPosition rounds toward zero: the left frame is column 0
    return BuildPick(lines, PickAt(GLfloat2{g->m_hoverX, g->m_hoverY}));
}

void InfoPanel::BuildSkill(std::vector<Line>& lines, int index) {
    Skill* s = m_game->m_skills[index];
    Line t, c;
    lines.push_back(t.Add(std::string(Skill::GetSkillName(s->m_type)) + "  (level " + N(s->m_level) + ")", kTitle));
    if (s->m_cooldown < 0) {
        c.Add("No cooldown", kDim);
    } else if (s->m_charge >= s->m_cooldown) {
        c.Add("Ready: release on the button to cast", kGood);
    } else {
        c.Add("Charging " + N(s->m_charge) + " / " + N(s->m_cooldown), kText);
    }
    lines.push_back(c);
    AddWrapped(lines, s->GetDescription(), kText, m_maxLines - lines.size());
}

void InfoPanel::BuildTile(std::vector<Line>& lines, Tile* t) {
    Game* g = m_game;
    Line title, info;
    switch (t->m_type) {
        case TILE_SKULL: {
            // the name, the stats and the state tags on one line when it fits (the description gets
            // the rest); else the tags, then the stats, go to lines of their own
            // (the original's name table calls a normal enemy "None"; it never showed that one)
            const std::string name = t->m_enemyType == ENEMY_NORMAL ? "Enemy" : Tile::GetEnemyName(t->m_enemyType);
            std::string stats = "   Attack " + N(t->GetAttack()) + "  Armour " + N(t->m_armour) + "  Health " +
                                N(t->m_health) + "/" + N(t->m_maxHealth);
            if (t->m_countdown > 0) stats += "  Countdown " + N(t->m_countdown);
            std::string tags;
            if (t->m_frozen) tags += "  frozen";
            if (t->m_spawnedThisTurn) tags += "  new: no attack yet";
            if (t->m_burning) tags += "  burning " + N(t->m_burnDamage);
            title.Add(name, kTitle);
            if (TextWidth(name + stats + tags) <= kWidth) {
                title.Add(stats, kText);
                if (!tags.empty()) title.Add(tags, kDim);
                lines.push_back(title);
            } else if (TextWidth(name + stats) <= kWidth) {
                title.Add(stats, kText);
                lines.push_back(title);
                Line flags;
                lines.push_back(flags.Add(tags.substr(2), kDim));
            } else {   // too wide even without the tags: try a smaller text size
                m_cut = true;
                lines.push_back(title);
                info.Add(stats.substr(3) + tags, kText);
                lines.push_back(info);
            }
            if (t->m_enemyType != ENEMY_NORMAL) {
                AddWrapped(lines, Tile::GetEnemyDescription(t->m_enemyType), kText, m_maxLines - lines.size());
            } else {
                Line k;
                lines.push_back(k.Add("Takes " + N(t->m_armour + t->m_health) + " damage to kill", kDim));
            }
            return;
        }
        case TILE_POTION:
            if (t->m_manaPotion) title.Add("Mana potion", kTitle);
            else if (t->m_acidPotion) title.Add("Acid potion", kTitle);
            else if (t->m_explosivePotion) title.Add("Explosive potion", kTitle);
            else {
                title.Add("Potion", kTitle);
                info.Add("+" + N((g->m_bonusHealAmount + g->m_baseHealAmount) * (g->m_doubleHealth ? 2 : 1)) +
                             " health each",
                         kText);
            }
            break;
        case TILE_SHIELD:
            if (t->m_broken) {
                title.Add("Broken shield", kTitle);
                info.Add("Gives no armour", kDim);
            } else {
                title.Add("Shield", kTitle);
                info.Add("+" + N((g->m_bonusArmourAmount + g->m_baseArmourAmount) * (g->m_doubleArmour ? 2 : 1)) +
                             " armour each; beyond full armour: upgrade points",
                         kText);
            }
            break;
        case TILE_COIN:
            title.Add("Coin", kTitle);
            info.Add("Gold for item upgrades", kText);
            break;
        case TILE_SWORD:
            if (t->m_broken) {
                title.Add("Broken sword", kTitle);
                info.Add("Adds no damage", kDim);
            } else if (t->m_magicSword) {
                title.Add("Magic sword", kTitle);
                info.Add("+" + N(g->m_weaponDamage * 5 * (g->m_doubleDamage ? 2 : 1)) + " damage (5x weapon)", kText);
            } else if (t->m_arrow) {
                title.Add("Arrow", kTitle);
                info.Add("Shoots past other tiles in a chain", kText);
            } else {
                title.Add("Sword", kTitle);
                info.Add("+" + N(g->m_weaponDamage * (g->m_doubleDamage ? 2 : 1)) + " damage each", kText);
            }
            break;
        case TILE_SPECIAL:
            if (t->m_specialType == SPECIAL_TILE_TROPHY) {
                title.Add("Trophy", kTitle);
                info.Add("Select it alone: unlock a class or level up", kGood);
            } else {
                title.Add("Special tile", kTitle);
            }
            break;
        default:
            return;
    }
    lines.push_back(title);
    if (!info.text.empty()) lines.push_back(info);
    Line flags;
    if (t->m_frozen) flags.Add("Frozen: cannot be selected  ", kDim);
    if (t->m_burning) flags.Add("Burning: +" + N(t->m_burnDamage) + " damage in a chain", kDim);
    if (!flags.text.empty()) lines.push_back(flags);
}

void InfoPanel::Layout(const std::vector<Line>& lines) {
    m_shown = lines;
    m_shownScale = m_scale;
    m_shownTop = PanelTop();
    m_laidOut = true;
    m_labels.resize(lines.size());
    const float step = m_scale * kLineFactor;
    for (size_t i = 0; i < lines.size(); ++i) {
        Label& l = m_labels[i];
        l.m_font = &m_game->m_fonts[0];
        l.m_spacing = 2.0f;
        l.m_scale = GLfloat2{m_scale, m_scale};
        l.SetText(lines[i].text.c_str(), -1);
        l.m_colour = kText;
        l.UpdateColour();
        for (const Span& s : lines[i].spans) l.SetSubColour(s.from, s.to, s.colour);
        l.m_position = GLfloat2{kInset, m_shownTop + step * static_cast<float>(i + 1)};
    }
}

void InfoPanel::Render() {
    Game* g = m_game;
    if (!g->m_tallCanvas || !(g->IsBoardScreen() || g->BoardUnderFade())) return;   // (also under a fade: no jump)
    // a screen over the board (settings, help, unlock, game over): just the empty panel
    const bool covered = g->ScreenOverBoard();
    const std::vector<Line> lines = covered ? std::vector<Line>() : BuildLines();
    if (!m_laidOut || !(lines == m_shown) || m_scale != m_shownScale || PanelTop() != m_shownTop) Layout(lines);
    const float top = PanelTop(), h = PanelHeight();
    m_background.SetSize(GLfloat2{1.0f, h});
    m_background.m_position = GLfloat2{0.5f, top + h * 0.5f};
    sdlLoadIdentity();
    sdlDisable(GL_TEXTURE_2D);
    m_background.Render();
    if (g->IsWide()) {   // in its framed card: a line on the HUD's side
        m_edge.m_position = GLfloat2{0.5f, top + 0.002f};
        m_edge.Render();
    } else {             // a thin frame all round, a little inside the panel
        const float x0 = kFrameInset, x1 = 1.0f - kFrameInset;
        const float y0 = top + kFrameInset, y1 = top + h - kFrameInset;
        const float w = x1 - x0, fh = y1 - y0;
        m_frame[0].SetSize(GLfloat2{w, kFrameWidth});
        m_frame[0].m_position = GLfloat2{0.5f, y0 + kFrameWidth * 0.5f};
        m_frame[1].SetSize(GLfloat2{w, kFrameWidth});
        m_frame[1].m_position = GLfloat2{0.5f, y1 - kFrameWidth * 0.5f};
        m_frame[2].SetSize(GLfloat2{kFrameWidth, fh});
        m_frame[2].m_position = GLfloat2{x0 + kFrameWidth * 0.5f, top + h * 0.5f};
        m_frame[3].SetSize(GLfloat2{kFrameWidth, fh});
        m_frame[3].m_position = GLfloat2{x1 - kFrameWidth * 0.5f, top + h * 0.5f};
        for (Quad& q : m_frame) q.Render();
    }
    sdlEnable(GL_TEXTURE_2D);
    for (Label& l : m_labels) l.Render();
}
