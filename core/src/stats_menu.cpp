#include "stats_menu.h"

#include <cstdio>
#include <string>

#include "character_class.h"
#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "item.h"
#include "util.h"

namespace {
CharacterClass& Current(Game* g) { return g->m_classes[g->m_currentClass]; }

// The static text buffers of the original (0x000896d0 values, 0x000896d8 title/score, 0x00089718
// race and class)
char s_valueText[8];
char s_text[64];
}  // namespace

StatsMenu::StatsMenu(Game* game)
    : Menu(kQuadCount), m_game(game), m_titleLabel(&game->m_fonts[0]), m_scoreLabel(&game->m_fonts[0]) {
    m_shade.SetSize(GLfloat2{2.0f, 2.0f});
    m_shade.m_position = GLfloat2{0.5f, 0.5f};
    m_shade.SetColour(GLcolour4{0, 0, 0, 0xb4});
    SetMenuHeight(0xcb);
    float bottom = m_height + 58.0f;
    SetQuad(9, {23.0f, bottom}, {18.0f, 383.0f}, {0.5f, 0.5f}, 1.0f, 1.0f);
    m_valueRight = 0.928125f;
    const GLfloat2 uv{1.0f, 382.0f};
    const GLfloat2 size{53.0f, 53.0f};
    SetQuad(10, {23.0f, 63.0f}, uv, size, 1.0f, 1.0f);
    SetQuad(11, {80.0f, 63.0f}, uv, size, 1.0f, 1.0f);
    SetQuad(12, {23.0f, 120.0f}, uv, size, 1.0f, 1.0f);
    SetQuad(13, {80.0f, 120.0f}, uv, size, 1.0f, 1.0f);
    SetQuad(14, {23.0f, 177.0f}, uv, size, 1.0f, 1.0f);
    SetQuad(15, {80.0f, 177.0f}, uv, size, 1.0f, 1.0f);
    GLfloat2 tl, br;
    CharacterClass::GetDummyIconCoords(tl, br);
    m_classIcon.m_textureSize = 0x200;
    m_classIcon.AddQuad(tl, br, 2, -1.0f);
    m_classIcon.SetPosition(GLfloat2{0.25f, 0.553125f});
    // the info lines: the first (a title) larger
    float y = bottom / 320.0f;
    for (int i = 0; i < 15; ++i) {
        y = y + (i == 0 ? 0.06f : 0.05f);
        Label* l = new Label(&game->m_fonts[0]);
        l->m_position = GLfloat2{0.0875f, y};
        float scale = i == 0 ? 0.11f : 0.09f;
        l->m_scale = GLfloat2{scale, scale};
        l->m_spacing = 2.0f;
        m_infoLabels.push_back(l);
    }
    float top = (bottom - 5.0f) / 320.0f;
    m_titleLabel.m_position = GLfloat2{0.071875f, top - 0.038f};
    m_titleLabel.m_scale = GLfloat2{0.087f, 0.087f};
    m_titleLabel.m_spacing = 2.0f;
    m_titleLabel.m_colour.a = 0;
    m_titleLabel.UpdateColour();
    m_scoreLabel.m_position = GLfloat2{0.071875f, top};
    m_scoreLabel.m_scale = GLfloat2{0.087f, 0.087f};
    m_scoreLabel.m_spacing = 2.0f;
    m_scoreLabel.m_colour.a = 0;
    m_scoreLabel.UpdateColour();
}

StatsMenu::~StatsMenu() {
    for (Label* l : m_nameLabels) delete l;
    for (Label* l : m_valueLabels) delete l;
    for (Label* l : m_infoLabels) delete l;
}

GLfloat2 StatsMenu::GetButtonPosition(int quad) const {
    const QuadVertex& v = m_vertices[quad * 4];
    return GLfloat2{v.position.x + 0.00625f, v.position.y + 0.00625f};
}

bool StatsMenu::CursorInQuad(int quad) const {
    const QuadVertex* v = &m_vertices[quad * 4];
    float tx = m_game->m_touchX;
    float ty = m_game->m_touchY;
    return v[0].position.x < tx && tx < v[2].position.x && v[0].position.y < ty && ty < v[2].position.y;
}

bool StatsMenu::CursorOutsideMenu() const {
    float tx = m_game->m_touchX;
    float ty = m_game->m_touchY;
    if (tx < 0.015625f || tx > 0.990625f || ty < 0.140625f) return true;
    return ty > ((m_height + 58.0f) + 13.0f) / 320.0f;
}

// The frame: 13-pixel corners, the middle 284 wide and `height` pixels high.
void StatsMenu::SetMenuHeight(int height) {
    m_height = static_cast<float>(height);
    float bottom = m_height + 58.0f;
    SetQuad(0, {5.0f, 45.0f}, {498.0f, 498.0f}, {13.0f, 13.0f}, 1.0f, 1.0f);
    SetQuad(1, {18.0f, 45.0f}, {479.0f, 472.0f}, {1.0f, 13.0f}, 284.0f, 1.0f);
    SetQuad(2, {302.0f, 45.0f}, {380.0f, 415.0f}, {15.0f, 13.0f}, 1.0f, 1.0f);
    SetQuad(3, {5.0f, 58.0f}, {478.0f, 487.0f}, {13.0f, 1.0f}, 1.0f, m_height);
    SetQuad(4, {18.0f, 58.0f}, {145.0f, 399.0f}, {1.0f, 1.0f}, 284.0f, m_height);
    SetQuad(5, {302.0f, 58.0f}, {380.0f, 430.0f}, {15.0f, 1.0f}, 1.0f, m_height);
    SetQuad(6, {5.0f, bottom}, {498.0f, 482.0f}, {13.0f, 15.0f}, 1.0f, 1.0f);
    SetQuad(7, {18.0f, bottom}, {483.0f, 470.0f}, {1.0f, 15.0f}, 284.0f, 1.0f);
    SetQuad(8, {302.0f, bottom}, {380.0f, 433.0f}, {15.0f, 13.0f}, 1.0f, 1.0f);
}

// A value right-aligned: base + bonus (percentages capped at 100), green when the bonus counts.
void StatsMenu::SetLabelValue(int stat, int base, int bonus, bool percent) {
    if (percent) {
        int v = base + bonus;
        if (v > 99) v = 100;
        std::snprintf(s_valueText, sizeof(s_valueText), "%d%%", v);
    } else {
        std::snprintf(s_valueText, sizeof(s_valueText), "%d", base + bonus);
    }
    bool green = bonus > 0 && (!percent || base < 100);
    Label* l = m_valueLabels[stat];
    l->SetText(s_valueText, -1);
    l->m_position.x = m_valueRight - l->m_width * l->m_scale.x;
    l->m_colour = green ? GLcolour4{0, 0xff, 0, 0} : GLcolour4{0xff, 0xff, 0xff, 0};
    l->UpdateColour();
}

// The stat rows right of the item buttons: section heads (level, the four attributes, weapon
// damage, defence) larger, the stats derived from them indented below.
void StatsMenu::CreateLabels() {
    const QuadVertex& corner = m_vertices[45];   // quad 11's top right
    float y = corner.position.y - 0.002f;
    float x = corner.position.x + 0.015625f;
    float indented = x + 0.03125f;
    static const char* const kNames[kStatCount] = {
        "Level", "Strength", "Base damage", "Bonus XP chance", "Dexterity", "Defence per shield",
        "Bonus shield chance", "Vitality", "Max health", "Bonus potion chance", "Luck", "Health per potion",
        "Bonus coin chance", "Weapon damage", "Armour piercing", "Defence", "Armour durability"};
    for (int i = 0; i < kStatCount; ++i) {
        Label* name = new Label(&m_game->m_fonts[0]);
        Label* value = new Label(&m_game->m_fonts[0]);
        bool head = i < 2 || i == 4 || i == 10 || i == 7 || i == 15 || i == 13;
        float scale;
        if (head) {
            y = y + 0.038f;
            name->m_position = GLfloat2{x, y};
            scale = 0.087f;
        } else {
            y = y + 0.034f;
            name->m_position = GLfloat2{indented, y};
            scale = 0.079f;
        }
        value->m_position = name->m_position;
        name->m_scale = GLfloat2{scale, scale};
        value->m_scale = GLfloat2{scale, scale};
        name->m_spacing = 2.0f;
        value->m_spacing = 2.0f;
        value->m_colour.a = 0;
        value->UpdateColour();
        name->m_colour.a = 0;
        name->UpdateColour();
        m_nameLabels.push_back(name);
        m_valueLabels.push_back(value);
    }
    for (int i = 0; i < kStatCount; ++i) m_nameLabels[i]->SetText(kNames[i], -1);
}

void StatsMenu::SetIconAlpha(uint8_t alpha) {
    for (int t = 0; t < ITEM_TYPE_COUNT; ++t) {
        Item* item = m_game->GetItem(static_cast<ItemType>(t));
        for (QuadVertex& v : item->m_iconVertices) v.colour.a = alpha;
    }
}

void StatsMenu::Show() {
    Game* g = m_game;
    m_fadeTime = 0.2f;
    m_lastTouchState = 0;
    m_active = true;
    m_closing = false;
    m_selected = -1;
    m_infoCount = 0;
    SetMenuHeight(0xcb);
    m_shade.SetAlpha(0);
    m_classIcon.SetAlpha(0);
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, 0);
    SetIconAlpha(0);
    for (Label* l : m_infoLabels) {
        l->m_colour.a = 0;
        l->UpdateColour();
    }
    if (m_nameLabels.empty()) CreateLabels();
    SetLabelValue(0, g->m_level, 0, false);
    SetLabelValue(1, g->m_strength, g->m_itemStrength, false);
    SetLabelValue(2, g->m_baseDamage, g->m_bonusDamage, false);
    SetLabelValue(3, g->m_baseSkullTokenChance, g->m_bonusSkullTokenChance, true);
    SetLabelValue(4, g->m_dexterity, g->m_itemDexterity, false);
    SetLabelValue(5, g->m_baseArmourAmount, g->m_bonusArmourAmount, false);
    SetLabelValue(6, g->m_baseShieldTokenChance, g->m_bonusShieldTokenChance, true);
    SetLabelValue(7, g->m_vitality, g->m_itemVitality, false);
    SetLabelValue(8, g->m_baseMaxHealth, g->m_bonusMaxHealth, false);
    SetLabelValue(9, g->m_basePotionTokenChance, g->m_bonusPotionTokenChance, true);
    SetLabelValue(10, g->m_luck, g->m_itemLuck, false);
    SetLabelValue(11, g->m_baseHealAmount, g->m_bonusHealAmount, false);
    SetLabelValue(12, g->m_baseCoinTokenChance, g->m_bonusCoinTokenChance, true);
    SetLabelValue(13, g->m_weaponDamage, 0, false);
    SetLabelValue(14, 50, g->m_armourPiercingChance - 50, true);
    SetLabelValue(15, g->m_maxArmour, 0, false);
    int durability = g->m_armourDamageChance;
    if (durability > 99) durability = 100;
    SetLabelValue(16, 50, 50 - durability, true);
    if (!g->m_challenge || g->m_mode == 3) {
        std::snprintf(s_text, sizeof(s_text), "%s difficulty", Game::GetDifficultyName(g->m_difficulty));
    } else {
        std::snprintf(s_text, sizeof(s_text), "%s", Game::GetModeName(g->m_mode));
    }
    m_titleLabel.SetText(s_text, -1);
    std::snprintf(s_text, sizeof(s_text), "Score: %d", g->GetScore());
    m_scoreLabel.SetText(s_text, -1);
    g->GetItem(HELMET)->m_iconPosition = GetButtonPosition(10);
    g->GetItem(ARMOUR)->m_iconPosition = GetButtonPosition(11);
    g->GetItem(WEAPON)->m_iconPosition = GetButtonPosition(12);
    g->GetItem(SHIELD)->m_iconPosition = GetButtonPosition(13);
    g->GetItem(ACCESSORY)->m_iconPosition = GetButtonPosition(14);
    for (int q = 10; q < 16; ++q) SetQuadUV(q, GLfloat2{1.0f, 382.0f}, GLfloat2{53.0f, 53.0f});
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(Current(g).m_class, tl, br);
    m_classIcon.SetUV(tl, br);
}

// While fading the shade, quads, labels, item icons and class icon fade together; otherwise a
// press outside closes the menu, a press on a button selects it and shows its lines.
void StatsMenu::Process(float dt) {
    Game* g = m_game;
    float t = 0.0f;
    if (m_fadeTime <= 0.0f) {
        int touch = g->m_touchState;
        if (touch == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
            if (CursorOutsideMenu()) {
                g->QueueSound(SOUND_CLICK);
                m_closing = true;
                m_fadeTime = 0.2f;
                return;   // (m_lastTouchState stays)
            }
            for (int q = 10; q < 16; ++q) {
                if (!CursorInQuad(q) || m_selected == q) continue;
                if (m_selected >= 0) SetQuadUV(m_selected, GLfloat2{1.0f, 382.0f}, GLfloat2{53.0f, 53.0f});
                m_selected = q;
                SetQuadUV(q, GLfloat2{55.0f, 382.0f}, GLfloat2{53.0f, 53.0f});
                if (q == 15) {   // the class: race and class, racial bonus, perks, flaws
                    CharacterClass& c = Current(g);
                    char* text = s_text;   // (0x00089718 in the original)
                    std::snprintf(text, sizeof(s_text), "%s %s", CharacterClass::GetRaceName(c.m_race),
                                  CharacterClass::GetClassName(c.m_class));
                    m_infoLabels[0]->SetText(text, -1);
                    std::vector<std::string> lines;
                    g->m_fonts[0].SplitLines(CharacterClass::GetRacialBonusDescription(c.m_race), 0.09f, 0.8f, lines);
                    for (size_t k = 0; k < lines.size(); ++k) m_infoLabels[k + 1]->SetText(lines[k].c_str(), -1);
                    int n = static_cast<int>(lines.size()) + 1;
                    for (int k = 0; k < 4; ++k) {
                        PerkType perk = Current(g).m_perks[k];
                        if (perk != PERK_NONE) m_infoLabels[n++]->SetText(CharacterClass::GetPerkDescription(perk), -1);
                    }
                    for (int k = 0; k < 4; ++k) {
                        FlawType flaw = Current(g).m_flaws[k];
                        if (flaw != FLAW_NONE) m_infoLabels[n++]->SetText(CharacterClass::GetFlawDescription(flaw), -1);
                    }
                    m_infoCount = n;
                } else {
                    ItemType type;
                    switch (q) {
                        case 11: type = ARMOUR; break;
                        case 12: type = WEAPON; break;
                        case 13: type = SHIELD; break;
                        case 14: type = ACCESSORY; break;
                        default: type = HELMET; break;
                    }
                    m_infoCount = g->GetItem(type)->MakeStatLabels(m_infoLabels);
                }
                float h = 19.1999989f + (static_cast<float>(m_infoCount - 1) * 0.05f) * 320.0f;   // 0x41999999
                h = h + 5.0f;
                SetMenuHeight(FloatToIntArm((h + 203.0f) + 5.0f));
                SetQuadSize(9, 274.0f, h);
                g->QueueSound(SOUND_CLICK);
            }
            touch = g->m_touchState;
        }
        m_lastTouchState = touch;
        return;
    }
    float left = m_fadeTime - dt;
    m_fadeTime = left;
    if (left > 0.0f) {
        t = left / 0.2f;
    } else {
        m_fadeTime = 0.0f;
        if (m_closing) {
            SetIconAlpha(0xff);
            m_active = false;
            return;
        }
    }
    float from;
    float to;
    if (m_closing) {
        m_shade.SetAlpha(static_cast<uint8_t>(FloatToUintArm(Interpolate(180.0f, 0.0f, t))));
        from = 255.0f;
        to = 0.0f;
    } else {
        m_shade.SetAlpha(static_cast<uint8_t>(FloatToUintArm(Interpolate(0.0f, 180.0f, t))));
        from = 0.0f;
        to = 255.0f;
    }
    uint8_t alpha = static_cast<uint8_t>(FloatToIntArm(Interpolate(from, to, t)));
    for (int q = 0; q < kQuadCount; ++q) SetQuadColour(q, 0xff, 0xff, 0xff, alpha);
    for (int i = 0; i < kStatCount; ++i) {
        m_nameLabels[i]->m_colour.a = alpha;
        m_nameLabels[i]->UpdateColour();
        m_valueLabels[i]->m_colour.a = alpha;
        m_valueLabels[i]->UpdateColour();
    }
    m_titleLabel.m_colour.a = alpha;
    m_titleLabel.UpdateColour();
    m_scoreLabel.m_colour.a = alpha;
    m_scoreLabel.UpdateColour();
    SetIconAlpha(alpha);
    for (Label* l : m_infoLabels) {
        l->m_colour.a = alpha;
        l->UpdateColour();
    }
    m_classIcon.SetAlpha(alpha);
}

void StatsMenu::Render() {
    sdlPushMatrix();
    sdlDisable(GL_TEXTURE_2D);
    m_shade.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    sdlVertexPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].position);
    sdlColorPointer(4, GL_UNSIGNED_BYTE, sizeof(QuadVertex), &m_vertices[0].colour);
    sdlTexCoordPointer(2, GL_FLOAT, sizeof(QuadVertex), &m_vertices[0].uv);
    for (int q = 0; q < kQuadCount; ++q) {
        if (q == 9 && m_infoCount < 1) continue;   // the info panel only with lines
        sdlDrawArrays(GL_TRIANGLE_FAN, q * 4, 4);
    }
    sdlPopMatrix();
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_classIcon.Render();
    sdlBindTexture(GL_TEXTURE_2D, 3);
    for (int t = 0; t < ITEM_TYPE_COUNT; ++t) m_game->GetItem(static_cast<ItemType>(t))->RenderIcon(true);
    for (int i = 0; i < kStatCount; ++i) {
        m_nameLabels[i]->Render();
        m_valueLabels[i]->Render();
    }
    m_titleLabel.Render();
    m_scoreLabel.Render();
    for (int i = 0; i < m_infoCount; ++i) m_infoLabels[i]->Render();
}
