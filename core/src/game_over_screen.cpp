#include "game_over_screen.h"

#include <cstdio>

#include "character_class.h"
#include "dr_gl.h"
#include "game.h"
#include "item.h"
#include "label.h"
#include "rand.h"
#include "skill.h"
#include "tile.h"
#include "util.h"

namespace {
const float kPx = 1.0f / 1024.0f;

// The static text buffer of the original's counting numbers (0x00089348)
char s_number[32];

float CentreX(const Label* l) { return 0.5f + (l->m_width * l->m_scale.x) * -0.5f; }
float RightX(const Label* l) { return 0.85f - l->m_width * l->m_scale.x; }
}  // namespace

// Headings at x 0.15, rows 0.07 apart with extra gaps; the perk and flaw icons right-aligned at
// 0.85 on their rows; the enemy quads with their glows (randomly rotated).
GameOverScreen::GameOverScreen(Game* game) : m_game(game) {
    m_shade.SetSize(GLfloat2{2.0f, 2.0f});
    m_shade.m_position = GLfloat2{0.5f, 0.5f};
    m_loseBanner.SetSize(GLfloat2{0.278125f, 0.878125f});
    m_loseBanner.m_position = GLfloat2{0.501562476f, 0.473437488f};   // 0x3f006666, 0x3ef26666
    m_loseBanner.SetAlpha(0);
    m_loseBanner.SetUV(GLfloat2{833 * kPx, 676 * kPx}, GLfloat2{922 * kPx, 957 * kPx});
    m_loseBanner.m_rotation = -90.0f;
    m_winBanner.SetSize(GLfloat2{0.71875f, 0.228125f});
    m_winBanner.m_position = GLfloat2{0.501562476f, 0.473437488f};
    m_winBanner.SetUV(GLfloat2{73 * kPx, 887 * kPx}, GLfloat2{303 * kPx, 960 * kPx});
    m_winBanner.SetAlpha(0);
    const float icon = 0.0612500012f;   // 0x3d7ae148
    for (int i = 0; i < kLabelCount; ++i) {
        Label* l = new Label(&game->m_fonts[0]);
        l->m_spacing = 2.0f;
        float extra;
        if (i == 0) {
            l->m_scale = GLfloat2{0.2f, 0.2f};
            extra = 0.0f;
        } else {
            l->m_scale = GLfloat2{0.15f, 0.15f};
            if (i < 2) extra = 0.0175f;
            else if (i < 3) extra = 0.035f;
            else if (i < 8) extra = 0.0583333336f;
            else if (i == 12) extra = 0.198333323f;
            else extra = 0.12833333f;
        }
        if (i < 13) {
            l->m_position.x = 0.15f;
            l->m_position.y = (0.2f + static_cast<float>(i) * 0.07f) + extra;
        }
        m_labels.push_back(l);
        if (i == 10 || i == 11) {   // the perk / flaw icons on these rows
            Quad* quads = i == 10 ? m_perks : m_flaws;
            for (int k = 0; k < 4; ++k) {
                quads[k].SetSize(GLfloat2{icon, icon});
                quads[k].m_position.x = 0.85f - (static_cast<float>(k) + 0.5f) * icon;
                quads[k].m_position.y = m_labels[i]->m_position.y + -0.0306250006f;
            }
        }
    }
    GLfloat2 tl, br;
    Tile::GetMortalGlowUV(tl, br);   // (unused)
    for (int i = 0; i < kEnemyQuads; ++i) {
        m_enemies[i].SetSize(GLfloat2{0.0625f, 0.0625f});
        m_glows[i].SetSize(GLfloat2{0.09375f, 0.09375f});
        m_glows[i].m_rotation = Rand(0.0f, 360.0f, RAND_COSMETIC);
    }
}

GameOverScreen::~GameOverScreen() {
    for (Label* l : m_labels) delete l;
}

void GameOverScreen::Initialise(bool survived) {
    Game* g = m_game;
    m_survived = survived;
    m_loseBanner.SetAlpha(0);
    m_winBanner.SetAlpha(0);
    if (!survived) {
        m_shade.SetColour(GLcolour4{0xb6, 0x2a, 0x0f, 0});
    } else {
        m_shade.SetColour(GLcolour4{0x40, 0xe2, 0, 0});
    }
    m_state = 0;
    m_stage = 0;
    m_itemsShown = 0;
    m_time = 0.0f;
    m_skillsShown = 0;
    m_timer = 0.0f;
    m_perksShown = 0;
    m_flawsShown = 0;
    m_enemiesShown = 0;
    m_active = true;
    std::vector<Label*>& l = m_labels;
    l[3]->SetText("Turn", -1);
    l[4]->SetText("Level", -1);
    if (!g->m_challenge || g->m_mode != MODE_PRETZEL) {
        l[5]->SetText("Experience", -1);
        l[6]->SetText("Gold", -1);
        l[7]->SetText("Upgrades", -1);
    } else {
        l[5]->SetText("Average Length", -1);
        l[6]->SetText("Max Length", -1);
        l[7]->SetText("Total Pretzels", -1);
    }
    l[8]->SetText("Items", -1);
    l[9]->SetText("Skills", -1);
    l[10]->SetText("Perks", -1);
    l[11]->SetText("Flaws", -1);
    l[12]->SetText("Score", -1);
    l[0]->SetText(g->m_name, -1);
    l[0]->m_position.x = CentreX(l[0]);
    CharacterClass& c = g->m_classes[g->m_currentClass];
    char text[64];
    std::snprintf(text, sizeof(text), "Level %d %s %s", static_cast<int>(static_cast<uint8_t>(c.m_level)),
                  CharacterClass::GetRaceName(c.m_race), CharacterClass::GetClassName(c.m_class));
    l[1]->SetText(text, -1);
    l[1]->m_position.x = CentreX(l[1]);
    if (!g->m_challenge || g->m_mode == 3) {
        l[2]->SetText(Game::GetDifficultyName(g->m_difficulty), -1);
    } else {
        l[2]->SetText(Game::GetModeName(g->m_mode), -1);
    }
    l[2]->m_position.x = CentreX(l[2]);
    for (Label* x : l) {
        x->m_colour.a = 0xff;
        x->UpdateColour();
    }
    // the value labels on rows 5..12, 13 (Process moves them to their rows as they count)
    for (int i = 13; i < kLabelCount; ++i) {
        int row = i == 20 ? 12 : i - 8;
        l[i]->SetText("0", -1);
        l[i]->m_position.y = l[row]->m_position.y;
        l[i]->m_position.x = RightX(l[i]);
    }
    for (int i = 0; i < 4; ++i) {
        m_perks[i].SetAlpha(0xff);
        m_flaws[i].SetAlpha(0xff);
    }
    for (int i = 0; i < kEnemyQuads; ++i) {
        m_enemies[i].SetAlpha(0xff);
        m_glows[i].SetAlpha(0xff);
    }
    g->QueueSound(survived ? SOUND_LEVELUP : SOUND_LOSE);
}

// Item and skill icons (drawn at scale 0.4) right-aligned at the rows' end, the perk and flaw
// icons, the enemies left on the board sorted by type in rows of up to 11 below the score.
void GameOverScreen::InitIcons() {
    Game* g = m_game;
    for (int t = 0; t < ITEM_TYPE_COUNT; ++t) {
        Item* item = g->GetItem(static_cast<ItemType>(t));
        float y = m_labels[8]->m_position.y;
        item->m_iconPosition.x = 2.125f + static_cast<float>(5 - t) * -0.153125f;
        item->m_iconPosition.y = y / 0.4f - 0.153125f;
    }
    for (int k = 0; k < g->m_skillCount; ++k) {
        float y = m_labels[9]->m_position.y;
        Skill* s = g->m_skills[k];
        s->m_iconPosition.x = 2.125f + static_cast<float>(g->m_skillCount - k) * -0.153125f;
        s->m_iconPosition.y = y / 0.4f - 0.153125f;
    }
    CharacterClass& c = g->m_classes[g->m_currentClass];
    GLfloat2 tl, br;
    m_perkCount = 0;
    for (int k = 0; k < 4; ++k) {
        if (c.m_perks[k] == PERK_NONE) continue;
        CharacterClass::GetIconCoords(c.m_perks[k], tl, br);
        m_perks[m_perkCount].SetUV(GLfloat2{tl.x / 512.0f, tl.y / 512.0f}, GLfloat2{br.x / 512.0f, br.y / 512.0f});
        ++m_perkCount;
    }
    m_flawCount = 0;
    for (int k = 0; k < 4; ++k) {
        if (c.m_flaws[k] == FLAW_NONE) continue;
        CharacterClass::GetIconCoords(c.m_flaws[k], tl, br);
        m_flaws[m_flawCount].SetUV(GLfloat2{tl.x / 512.0f, tl.y / 512.0f}, GLfloat2{br.x / 512.0f, br.y / 512.0f});
        ++m_flawCount;
    }
    int counts[ENEMY_TYPE_COUNT];
    m_enemyCount = 0;
    for (int t = 0; t < ENEMY_TYPE_COUNT; ++t) counts[t] = g->GetEnemyCount(static_cast<EnemyType>(t));
    for (int t = 0; t < ENEMY_TYPE_COUNT; ++t) m_enemyCount += counts[t];
    std::vector<int> types;
    for (int t = 0; t < ENEMY_TYPE_COUNT; ++t) {
        for (int k = 0; k < counts[t]; ++k) types.push_back(t);
    }
    const float size = m_enemies[0].m_size.y;
    int left = m_enemyCount;
    int i = 0;
    for (int row = 1; left > 0; ++row) {
        int n = left > 10 ? 11 : left;
        float rowX = 0.5f + (static_cast<float>(n) * 0.5f) * -0.0625f;
        float rowY = static_cast<float>(row);
        for (int col = 0; col < n; ++col, ++i) {
            EnemyType type = static_cast<EnemyType>(types[i]);
            Tile::GetEnemyUV(type, tl, br);
            m_enemies[i].SetUV(tl, br);
            m_enemies[i].m_position.y = m_labels[12]->m_position.y + rowY * size;
            m_enemies[i].m_position.x = rowX + (static_cast<float>(col) + 0.5f) * size;
            m_glows[i].m_position = m_enemies[i].m_position;
            if (type != ENEMY_NORMAL && type == CharacterClass::GetMortalEnemyType(c.m_race)) {
                Tile::GetMortalGlowUV(tl, br);
                m_glows[i].SetUV(tl, br);
            } else {
                m_glows[i].SetUV(GLfloat2{0.0f, 0.0f}, GLfloat2{0.0f, 0.0f});
            }
        }
        left -= n;
    }
}

void GameOverScreen::FadeOut() {
    m_time = 0.0f;
    m_state = 3;
    m_timer = 10.0f;
}

void GameOverScreen::Process(float dt) {
    Game* g = m_game;
    float timer = m_timer;
    int touch = g->m_touchState;
    if (m_state >= 2 && touch == TOUCHING && m_lastTouchState == NOT_TOUCHING) {   // a tap skips ahead
        if (m_state != 3) m_time = 1.0f;
        m_timer = 0.0f;
        touch = g->m_touchState;
    }
    m_lastTouchState = touch;
    if (timer > 0.0f) {   // waiting for the next row
        float t = m_timer - dt;
        m_timer = t;
        if (!(t > 0.0f) && m_state == 2) {
            g->QueueSound(SOUND_ATTACK_HIT);
            m_stage = m_stage == 12 ? 21 : m_stage + 1;
        }
        if (m_timer > 0.0f) return;
    }
    bool pretzel = g->m_challenge && g->m_mode == MODE_PRETZEL;
    if (m_state == 3) {   // fading out over two seconds
        float t = m_time + dt * 0.5f;
        m_time = t;
        if (1.0f <= t) {
            m_active = false;
            return;
        }
        float shade = Interpolate(0.0f, 255.0f, t);
        float rest = Interpolate(0.0f, 255.0f, Clamp(m_time * 3.0f, 0.0f, 1.0f));
        uint8_t a = static_cast<uint8_t>(FloatToIntArm(rest));
        for (Label* l : m_labels) {
            l->m_colour.a = a;
            l->UpdateColour();
        }
        for (int i = 0; i < ITEM_TYPE_COUNT; ++i) {
            for (QuadVertex& v : g->GetItem(static_cast<ItemType>(i))->m_iconVertices) v.colour.a = a;
        }
        for (int i = 0; i < g->m_skillCount; ++i) {
            for (QuadVertex& v : g->m_skills[i]->m_iconVertices) v.colour.a = a;
        }
        for (int i = 0; i < 4; ++i) {
            m_perks[i].SetAlpha(a);
            m_flaws[i].SetAlpha(a);
        }
        for (int i = 0; i < kEnemyQuads; ++i) {
            m_enemies[i].SetAlpha(a);
            m_glows[i].SetAlpha(a);
        }
        m_shade.SetAlpha(static_cast<uint8_t>(FloatToIntArm(shade)));
        return;
    }
    if (m_state == 1) {   // the colour fades to black over a second
        float t = dt + m_time;
        m_time = t;
        if (1.0f <= t) {
            m_state = 2;
            m_stage = -1;
            m_value = 1.0f;
            m_time = 0.0f;
            m_speed = 1.0f;
            m_timer = 0.5f;
            m_iconTimer = 0.2f;
            InitIcons();
            m_shade.SetColour(GLcolour4{0, 0, 0, 0xff});
            return;
        }
        float r = Interpolate(0.0f, m_survived ? 64.0f : 182.0f, t);
        float gr = Interpolate(0.0f, m_survived ? 226.0f : 42.0f, m_time);
        float b = Interpolate(0.0f, m_survived ? 0.0f : 15.0f, m_time);
        m_shade.SetColour(GLcolour4{static_cast<uint8_t>(FloatToIntArm(r)), static_cast<uint8_t>(FloatToIntArm(gr)),
                                    static_cast<uint8_t>(FloatToIntArm(b)), 0xff});
        if (!m_survived) return;
        m_winBanner.SetAlpha(static_cast<uint8_t>(FloatToUintArm(255.0f + m_time * -255.0f)));
        return;
    }
    if (m_state != 2) {   // 0: the flash appears over two seconds
        float t = m_time + dt * 0.5f;
        m_time = t;
        if (t < 1.0f) {
            uint8_t a = static_cast<uint8_t>(FloatToIntArm(Interpolate(255.0f, 0.0f, t * t)));
            m_shade.SetAlpha(a);
            (m_survived ? m_winBanner : m_loseBanner).SetAlpha(a);
            return;
        }
        m_state = 1;
        m_time = 0.0f;
        m_timer = 1.0f;
        m_shade.SetAlpha(0xff);
        (m_survived ? m_winBanner : m_loseBanner).SetAlpha(0xff);
        return;
    }
    // the summary, row by row
    auto nextRow = [this]() {
        m_time = 0.0f;
        m_value = 1.0f;
        m_speed = 1.0f;
        m_timer = 0.5f;
    };
    auto finish = [this, g]() {
        g->ShowNagScreen();
        m_time = 0.0f;
        m_state = 3;
        m_timer = 10.0f;
    };
    // one more icon every 0.2 s (all at once after a tap); true when all are shown
    auto icons = [this, dt, g](int& shown, int count, Sound sound) {
        if (m_time == 1.0f) {
            shown = count;
            return true;
        }
        if (shown < count) {
            float t = m_iconTimer - dt;
            m_iconTimer = t;
            if (t <= 0.0f) {
                ++shown;
                m_iconTimer = 0.2f;
                g->QueueSound(sound);
            }
        }
        return shown >= count;
    };
    switch (m_stage) {
        case 21:   // the enemies left
            if (m_time != 1.0f && m_enemiesShown >= m_enemyCount) {
                finish();
                return;
            }
            if (icons(m_enemiesShown, m_enemyCount, SOUND_SKULL)) finish();
            return;
        case 11:
            if (m_time != 1.0f && m_flawCount <= m_flawsShown) { nextRow(); return; }
            if (icons(m_flawsShown, m_flawCount, SOUND_COIN)) nextRow();
            return;
        case 10:
            if (m_time != 1.0f && m_perkCount <= m_perksShown) { nextRow(); return; }
            if (icons(m_perksShown, m_perkCount, SOUND_COIN)) nextRow();
            return;
        case 8:
            if (m_time == 1.0f) {
                m_itemsShown = 5;
                nextRow();
                return;
            }
            if (m_itemsShown >= 5) { nextRow(); return; }
            if (icons(m_itemsShown, 5, SOUND_COIN)) nextRow();
            return;
        case 9:
            if (m_time != 1.0f && g->m_skillCount <= m_skillsShown) { nextRow(); return; }
            if (icons(m_skillsShown, g->m_skillCount, SOUND_COIN)) nextRow();
            return;
        default:
            break;
    }
    if (static_cast<unsigned>(m_stage - 3) > 9) {   // the rows without a number
        nextRow();
        return;
    }
    // a number counting up: turn, level, experience, gold, upgrades (or the Pretzel numbers), score
    Label* label = m_labels[m_stage == 12 ? 20 : m_stage + 10];
    int target = 0;
    auto sprint = [g](int v) { return g->m_challenge && g->m_mode == MODE_SPRINT ? v / 4 : v; };
    switch (m_stage) {
        case 3: target = g->m_turn; break;
        case 4: target = g->m_level; break;
        case 5: target = pretzel ? g->m_averageChainLength10 : sprint(g->m_xp + g->m_xpBanked); break;
        case 6: target = pretzel ? g->m_longestChain : sprint(g->m_gold + g->m_goldBanked); break;
        case 7: target = pretzel ? g->m_chainCount : sprint(g->m_upgradePoints + g->m_upgradePointsBanked); break;
        case 12: target = g->GetScore(); break;
        default: break;   // (unreachable: the icon rows)
    }
    float speed = m_speed + dt * 0.1f;
    float old = m_value;
    m_speed = speed;
    float value = speed * old;
    float goal = static_cast<float>(target);
    m_value = value;
    int shown;
    if (!(value < goal) || m_time == 1.0f) {
        m_value = goal;
        g->QueueSound(SOUND_ABILITY_READY);
        shown = FloatToIntArm(m_value);
    } else {
        shown = FloatToIntArm(old);
        if (FloatToIntArm(value) != shown) {
            g->QueueSound(SOUND_COIN);
            shown = FloatToIntArm(m_value);
        }
    }
    if (pretzel && m_stage == 5) {
        std::snprintf(s_number, sizeof(s_number), "%.1f", static_cast<double>(static_cast<float>(shown) / 10.0f));
    } else {
        std::snprintf(s_number, sizeof(s_number), "%d", shown);
    }
    label->SetText(s_number, -1);
    label->m_position.y = m_labels[m_stage]->m_position.y;
    label->m_position.x = RightX(label);
    if (goal <= m_value) nextRow();
}

void GameOverScreen::Render() {
    Game* g = m_game;
    sdlDisable(GL_TEXTURE_2D);
    m_shade.Render();
    sdlEnable(GL_TEXTURE_2D);
    if (m_state < 2) {
        sdlLoadIdentity();
        sdlBindTexture(GL_TEXTURE_2D, 1);
        (m_survived ? m_winBanner : m_loseBanner).Render();
        if (m_state < 2) return;
    }
    if (m_state != 2) m_enemiesShown = m_enemyCount;
    std::vector<Label*>& l = m_labels;
    switch (m_stage) {   // each row draws the rows above it too (falling through)
        default:
            return;
        case 21:
            sdlBindTexture(GL_TEXTURE_2D, 1);
            for (int i = 0; i < m_enemiesShown; ++i) {
                m_glows[i].Render();
                m_enemies[i].Render();
            }
            [[fallthrough]];
        case 12:
            l[12]->Render();
            l[20]->Render();
            m_flawsShown = m_flawCount;
            [[fallthrough]];
        case 11:
            l[11]->Render();
            for (int i = 0; i < m_flawsShown; ++i) {
                sdlBindTexture(GL_TEXTURE_2D, 4);
                m_flaws[i].Render();
            }
            m_perksShown = m_perkCount;
            [[fallthrough]];
        case 10:
            l[10]->Render();
            for (int i = 0; i < m_perksShown; ++i) {
                sdlBindTexture(GL_TEXTURE_2D, 4);
                m_perks[i].Render();
            }
            m_skillsShown = g->m_skillCount;
            [[fallthrough]];
        case 9:
            l[9]->Render();
            if (g->m_skillCount > 0) {
                sdlPushMatrix();
                sdlScalef(0.4f, 0.4f, 0.4f);
                for (int i = 0; i < m_skillsShown; ++i) {
                    sdlBindTexture(GL_TEXTURE_2D, 1);
                    g->m_skills[g->m_skillCount - i - 1]->RenderIcon(false);
                }
                sdlPopMatrix();
            }
            m_itemsShown = 5;
            [[fallthrough]];
        case 8:
            l[8]->Render();
            sdlBindTexture(GL_TEXTURE_2D, 3);
            sdlPushMatrix();
            sdlScalef(0.4f, 0.4f, 0.4f);
            for (int i = 0; i < m_itemsShown; ++i) g->GetItem(static_cast<ItemType>(ACCESSORY - i))->RenderIcon(true);
            sdlPopMatrix();
            [[fallthrough]];
        case 7:
            l[7]->Render();
            l[17]->Render();
            [[fallthrough]];
        case 6:
            l[6]->Render();
            l[16]->Render();
            [[fallthrough]];
        case 5:
            l[5]->Render();
            l[15]->Render();
            [[fallthrough]];
        case 4:
            l[4]->Render();
            l[14]->Render();
            [[fallthrough]];
        case 3:
            l[3]->Render();
            l[13]->Render();
            [[fallthrough]];
        case 2:
            l[2]->Render();
            [[fallthrough]];
        case 1:
            l[1]->Render();
            [[fallthrough]];
        case 0:
            l[0]->Render();
            return;
    }
}
