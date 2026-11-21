#include "interface.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

#include "dr_gl.h"
#include "game.h"
#include "rand.h"
#include "skill.h"
#include "token.h"
#include "util.h"

namespace {
// GetTarget's fallback for other tile types (a function-static (0, 0) in the original, 0x000893dc)
GLfloat2 s_noTarget = {0.0f, 0.0f};

const float kPx = 1.0f / 1024.0f;   // texture pixels
const GLcolour4 kWhite{0xff, 0xff, 0xff, 0xff};

// x of a label centred on `centre` (the original computes centre + width * scale * -0.5)
float Centred(const Label& l, float centre) { return centre + (l.m_width * l.m_scale.x) * -0.5f; }

// The static text buffers of the original (their contents persist between calls: the score label
// is set from its buffer even in a challenge mode that writes nothing into it).
char s_scoreText[64];   // 0x000893e4
}  // namespace

Interface::Interface(Game* game)
    : m_swordLabel(&game->m_fonts[0]),
      m_shieldLabel(&game->m_fonts[0]),
      m_skullLabel(&game->m_fonts[0]),
      m_healthLabel(&game->m_fonts[0]),
      m_poisonLabel(&game->m_fonts[0]),
      m_clockLabel(&game->m_fonts[0]),
      m_gameMenu(game),
      m_statsMenu(game),
      m_game(game),
      m_tooltip(game),
      m_scoreLabel(&game->m_fonts[2]),
      m_turnLabel(&game->m_fonts[2]) {
    m_foldoutMenu.Create(game, this);
    m_bottomPanel.SetSize(GLfloat2{1.0f, 0.2375f});
    m_bottomPanel.SetUV(GLfloat2{0.0f, 436 * kPx}, GLfloat2{320 * kPx, 512 * kPx});
    m_bottomPanel.m_position.x = 0.5f;
    m_bottomPanel.m_position.y = m_bottomPanel.m_size.y * -0.5f + 1.5f;
    m_topPanel.SetSize(GLfloat2{1.0f, 0.228125f});
    m_topPanel.SetUV(GLfloat2{322 * kPx, 438 * kPx}, GLfloat2{323 * kPx, 511 * kPx});
    m_topPanel.m_position.x = 0.5f;
    m_topPanel.m_position.y = m_topPanel.m_size.y * 0.5f;
    for (int i = 0; i < 4; ++i) {
        Quad& button = m_skillButtons[i];
        button.SetSize(GLfloat2{0.165625f, 0.165625f});
        button.SetUV(GLfloat2{201 * kPx, 382 * kPx}, GLfloat2{254 * kPx, 435 * kPx});
        button.m_position = GetSpecialTarget(i);
        button.SetColour(kWhite);
        Quad& glow = m_skillReadyGlows[i];
        glow.SetSize(GLfloat2{0.190625f, 0.190625f});
        glow.SetUV(GLfloat2{450 * kPx, 185 * kPx}, GLfloat2{511 * kPx, 246 * kPx});
        glow.m_position = GetSpecialTarget(i);
        glow.SetAlpha(0);
    }
    m_shieldLabel.m_colour = GLcolour4{0xad, 0xb1, 0xff, 0xff};
    m_shieldLabel.UpdateColour();
    m_healthLabel.m_colour = GLcolour4{0xf9, 0xac, 0x62, 0xff};
    m_healthLabel.UpdateColour();
    m_poisonLabel.m_colour = GLcolour4{0x71, 0x34, 0xb0, 0xff};
    m_poisonLabel.UpdateColour();
    m_clockLabel.m_colour.g = 0x78;
    m_clockLabel.m_colour.a = 0xff;
    for (Label* l : {&m_swordLabel, &m_shieldLabel, &m_skullLabel, &m_healthLabel, &m_poisonLabel}) {
        l->m_scale = GLfloat2{0.09f, 0.09f};
    }
    m_clockLabel.m_scale = GLfloat2{0.1f, 0.1f};
    m_clockLabel.m_position = GLfloat2{0.865625f, 0.075f};
    m_clockLabel.m_spacing = 2.0f;
    m_clockLabel.m_colour.r = 0x82;
    m_clockLabel.m_colour.b = 0xdc;
    m_clockLabel.UpdateColour();
    m_menuButton.SetSize(GLfloat2{0.225f, 0.08125f});
    m_menuButton.SetUV(GLfloat2{342 * kPx, 337 * kPx}, GLfloat2{414 * kPx, 363 * kPx});
    m_menuButton.m_position = GLfloat2{0.86875f, 0.05625f};
    m_statsButton.SetSize(GLfloat2{0.225f, 0.08125f});
    m_statsButton.SetUV(GLfloat2{269 * kPx, 337 * kPx}, GLfloat2{341 * kPx, 363 * kPx});
    m_statsButton.m_position = GLfloat2{0.86875f, 0.14375f};
    m_healthShadowBar.SetAlpha(0x78);
    m_xpShadowBar.SetAlpha(0x50);
    m_upgradeShadowBar.SetAlpha(0x78);
    m_goldShadowBar[0].SetAlpha(0x50);
    m_goldShadowBar[1].SetAlpha(0x50);
    for (Label* l : {&m_scoreLabel, &m_turnLabel}) {
        l->m_scale = GLfloat2{0.13125f, 0.13125f};
        l->m_spacing = 2.0f;
        l->m_colour = GLcolour4{0x40, 0xe2, 0x00, 200};
        l->UpdateColour();
        l->m_position.y = 0.259375f;
    }
    m_scoreLabel.m_position.x = 0.015625f;
}

// ================================================================ bars and values

void Interface::UpdateHealthBar(Quad& bar, int value) {
    float maxHealth = static_cast<float>(m_maxHealth);
    if (m_maxHealth <= value) value = m_maxHealth;
    float f = static_cast<float>(value) / maxHealth;
    bar.SetSize(GLfloat2{0.221875f, f * 0.2f});
    GLfloat2 tl{0.3173828125f, 0.4365234375f + (1.0f - f) * 0.0625f};
    GLfloat2 br{0.38671875f, tl.y + f * 0.0625f};
    bar.SetUV(tl, br);
    bar.m_position.y = 1.46875f + bar.m_size.y * -0.5f;
    bar.m_position.x = 0.746875f + bar.m_size.x * 0.5f;
    char text[32];   // static buffer 0x00089444 in the original
    std::snprintf(text, sizeof(text), "%d/%d", m_health, m_maxHealth);
    m_healthLabel.SetText(text, -1);
    m_healthLabel.m_position.x = Centred(m_healthLabel, 0.853125f);
    m_healthLabel.m_position.y = 1.446875f;
    m_healthTarget.y = 1.46875f - m_healthBar.m_size.y;
}

void Interface::UpdateXPBar(Quad& bar, int value) {
    float f = static_cast<float>(value) / 100.0f;
    bar.SetSize(GLfloat2{f * 0.3125f, 0.03125f});
    bar.SetUV(GLfloat2{0.3876953125f, 0.4892578125f}, GLfloat2{0.3876953125f + f * 0.09765625f, 0.4990234375f});
    bar.m_position.y = 1.446875f + bar.m_size.y * 0.5f;
    bar.m_position.x = 0.34375f + bar.m_size.x * 0.5f;
    m_xpTarget.x = m_xpBar.m_size.x + 0.34375f;
}

void Interface::UpdateUPBar(Quad& bar, int value) {
    float f = static_cast<float>(value) / 100.0f;
    bar.SetSize(GLfloat2{f * 0.3125f, 0.03125f});
    bar.SetUV(GLfloat2{0.3876953125f, 0.478515625f}, GLfloat2{0.3876953125f + f * 0.09765625f, 0.48828125f});
    bar.m_position.y = 1.390625f + bar.m_size.y * 0.5f;
    bar.m_position.x = 0.34375f + bar.m_size.x * 0.5f;
    UpdateShieldTarget();
}

void Interface::UpdateGoldBar(Quad (&bars)[2], int value) {
    float stacks = 0.0f;   // full stacks of 20 coins
    int full = value / 20;
    if (full == 0) {
        bars[0].m_position = GLfloat2{200.0f, 200.0f};   // off screen
    } else {
        stacks = static_cast<float>(full);
        bars[0].SetSize(GLfloat2{stacks * 0.05f, 0.190625f});
        bars[0].SetUV(GLfloat2{0.3876953125f, 0.41796875f}, GLfloat2{0.3876953125f + stacks * 0.015625f, 0.4775390625f});
        bars[0].m_position.y = 1.4875f + bars[0].m_size.y * -0.5f;
        bars[0].m_position.x = 0.015625f + bars[0].m_size.x * 0.5f;
    }
    float rest = 0.0f;     // coins in the partial stack
    int part = value % 20;
    if (part == 0) {
        bars[1].m_position = GLfloat2{200.0f, 200.0f};
    } else {
        rest = static_cast<float>(part);
        bars[1].SetSize(GLfloat2{0.05f, rest * 0.009375f});
        GLfloat2 tl{0.3876953125f + stacks * 0.015625f, 0.4775390625f + rest * -0.0029296875f};
        GLfloat2 br{tl.x + 0.015625f, tl.y + rest * 0.0029296875f};
        float x = 0.015625f + stacks * 0.05f;
        bars[1].SetUV(tl, br);
        bars[1].m_position.y = 1.4875f + bars[1].m_size.y * -0.5f;
        bars[1].m_position.x = x + bars[1].m_size.x * 0.5f;
    }
    m_goldTarget.x = (0.015625f + stacks * 0.05f) + 0.025f;
    m_goldTarget.y = 1.4875f + rest * -0.009375f;
}

// Shields fly to the armour value while armour is missing (and no Corrosive enemy is on the board),
// else to the end of the upgrade bar (they become upgrade points).
void Interface::UpdateShieldTarget() {
    if (m_armour < m_maxArmour && m_game->GetEnemyCount(ENEMY_CORROSIVE) == 0) {
        m_shieldTarget = GLfloat2{0.496875f, 1.396875f};
    } else {
        m_shieldTarget = GLfloat2{m_upgradeBar.m_size.x + 0.34375f, 1.40625f};
    }
}

void Interface::UpdateShieldValue() {
    char text[32];   // static buffer 0x000893b8
    std::snprintf(text, sizeof(text), "%d/%d", m_armour, m_maxArmour);
    m_shieldLabel.SetText(text, -1);
    m_shieldLabel.m_position.x = Centred(m_shieldLabel, 0.496875f);
    m_shieldLabel.m_position.y = 1.365625f;
    UpdateShieldTarget();
}

void Interface::UpdateSwordValue() {
    char text[32];   // static buffer 0x00089378
    std::snprintf(text, sizeof(text), "+%d", m_swordValue);
    m_swordLabel.SetText(text, -1);
    m_swordLabel.m_position.x = Centred(m_swordLabel, 0.640625f);
    m_swordLabel.m_position.y = 1.365625f;
}

void Interface::UpdateSkullValue() {
    char text[32];   // static buffer 0x00089358
    std::snprintf(text, sizeof(text), "+%d", m_skullValue);
    m_skullLabel.SetText(text, -1);
    m_skullLabel.m_position.x = Centred(m_skullLabel, 0.33125f);
    m_skullLabel.m_position.y = 1.365625f;
}

// The health bar's colour: white, flashing red with the heartbeat (purple-ish when poisoned).
void Interface::UpdateHealthColour() {
    float c = 1.0f - m_heartbeatPulse / m_heartbeatPulseLength;
    m_heartbeatColour = c;
    if (c < 0.2f) {
        c = (c / 0.2f) * (c / 0.2f);
    } else {
        float x = (c - 0.2f) / 0.8f;
        c = (CosF(static_cast<float>(static_cast<double>(x) * 3.141592653589793)) + 1.0f) * 0.5f;   // the angle in double
    }
    m_heartbeatColour = c;
    float rTarget = m_poison > 0 ? 200.0f : 255.0f;
    float r = Interpolate(100.0f, rTarget, c);
    float gTarget = m_poison < 1 ? 255.0f : 200.0f;
    float g = Interpolate(100.0f, gTarget, m_heartbeatColour);
    uint8_t g8 = static_cast<uint8_t>(FloatToUintArm(g));
    float b = Interpolate(100.0f, 255.0f, m_heartbeatColour);
    m_healthBar.SetColour(GLcolour4{static_cast<uint8_t>(FloatToUintArm(r)), g8, static_cast<uint8_t>(FloatToUintArm(b)), 0xff});
}

// The skill buttons: charge bars (grey until full) and, for ready or just used skills, the glow
// and its help page.
void Interface::UpdateSkills() {
    for (int i = 0; i < m_game->m_skillCount; ++i) {
        Skill* s = m_game->m_skills[i];
        float f = static_cast<float>(s->m_charge) / static_cast<float>(s->m_cooldown);
        float uvH = f * 0.0517578125f;
        float h = f * 0.165625f;
        Quad& bar = m_skillChargeBars[i];
        bar.SetSize(GLfloat2{0.165625f, h});
        GLfloat2 tl{0.2490234375f, 0.4248046875f - uvH};
        bar.SetUV(tl, GLfloat2{0.30078125f, uvH + tl.y});
        GLfloat2 b = GetSpecialTarget(i);
        bar.m_position.x = b.x;
        bar.m_position.y = (b.y + 0.0828125f) + h * -0.5f;
        // port: while charging, the button (the empty part) near black and the fill at full colour
        // (the original greys the fill to 200 over the button as is: hard to tell from ready, and the
        // progress faint); ready: the button and the fill as they are
        const bool ready = f == 1.0f;
        bar.SetColour(kWhite);
        const uint8_t back = ready ? 0xff : 50;
        m_skillButtons[i].SetColour(GLcolour4{back, back, back, 0xff});
        if (s->m_charge == s->m_cooldown || s->m_charge == 0) {
            Quad& glow = m_skillReadyGlows[i];
            glow.SetColour(GLcolour4{static_cast<uint8_t>(s->m_active ? 0xfe : 0xff), 0xff, 0xff, glow.m_vertices[0].colour.a});
            m_game->ShowHelp(glow.m_position.x, glow.m_position.y, 0.0953125f, 0.0953125f, s->m_active ? 0x14 : 0x13);
        }
    }
    for (int i = m_game->m_skillCount; i < 4; ++i) m_skillButtons[i].SetColour(kWhite);   // port: empty slots as is
}

void Interface::PositionSkill(int index) {
    Skill* s = m_game->m_skills[index];
    GLfloat2 b = GetSpecialTarget(index);
    s->m_iconPosition.y = b.y - 0.0765625f;
    s->m_iconPosition.x = b.x - 0.0765625f;
}

void Interface::SetPoison(int poison) {
    m_poison = poison;
    UpdateHealthColour();
    if (m_poison != 0) {
        char text[32];   // static buffer 0x00089398
        std::snprintf(text, sizeof(text), "%d poison", m_poison);
        m_poisonLabel.SetText(text, -1);
        m_poisonLabel.m_position.x = Centred(m_poisonLabel, 0.853125f);
        m_poisonLabel.m_position.y = 1.384375f;
    }
}

// The menu button shows a clock when the clock is on.
void Interface::SetClockState(bool visible) {
    if (!visible) {
        m_menuButton.SetUV(GLfloat2{342 * kPx, 337 * kPx}, GLfloat2{414 * kPx, 363 * kPx});
    } else {
        m_menuButton.SetUV(GLfloat2{425 * kPx, 698 * kPx}, GLfloat2{497 * kPx, 724 * kPx});
    }
    m_clockVisible = visible;
    m_gameMenu.SetClockState(visible);
}

void Interface::SetClockString(const char* text) {
    if (!m_clockVisible) return;
    m_clockLabel.SetText(text, -1);
    m_clockLabel.m_position.x = Centred(m_clockLabel, 0.865625f);
}

void Interface::SetXPshadow(int value) {
    if (value < 1) {
        m_xpShadow = -1;
        return;
    }
    int v = ClampI(value + m_xp, 0, 100);
    if (v != m_xpShadow) {
        m_xpShadow = v;
        UpdateXPBar(m_xpShadowBar, v);
    }
}

void Interface::SetGoldShadow(int value) {
    if (value < 1) {
        m_goldShadow = -1;
        return;
    }
    int v = ClampI(value + m_gold, 0, 100);
    if (v != m_goldShadow) {
        m_goldShadow = v;
        UpdateGoldBar(m_goldShadowBar, v);
    }
}

void Interface::SetUPshadow(int value) {
    if (value < 1) {
        m_upgradeShadow = -1;
        return;
    }
    int v = ClampI(value + m_upgradePoints, 0, 100);
    if (v != m_upgradeShadow) {
        m_upgradeShadow = v;
        UpdateUPBar(m_upgradeShadowBar, v);
    }
}

void Interface::SetHealthShadow(int value) {
    if (value < 1) {
        m_healthShadow = -1;
        return;
    }
    int v = ClampI(value + m_health, 0, m_maxHealth);
    if (v != m_healthShadow) {
        m_healthShadow = v;
        UpdateHealthBar(m_healthShadowBar, v);
    }
}

void Interface::Reset() {
    m_displayedScore = 0;
    m_scoreGain = 0;
    m_scoreBonus = 0;
    m_scoreTallying = false;
    m_pressedSkill = -1;
    m_tooltip.HideInstantly();
    m_menuButtonPressed = false;
    m_statsButtonPressed = false;
    m_heartbeatDelay = 0.0f;
    m_heartbeatPulse = 0.0f;
    m_heartbeatPulseLength = 1.0f;
    m_poison = 0;
    m_healthShadow = -1;
    m_xpShadow = -1;
    m_upgradeShadow = -1;
    m_goldShadow = -1;
    m_gameMenu.m_active = false;
    m_statsMenu.m_active = false;
    for (int i = 0; i < 4; ++i) {
        m_skillButtons[i].SetColour(kWhite);
        m_skillReadyGlows[i].SetColour(GLcolour4{0xff, 0xff, 0xff, 0});
    }
    m_menuButton.SetColour(kWhite);
    m_statsButton.SetColour(kWhite);
    m_healthBar.SetColour(kWhite);
}

// ================================================================ queries

GLfloat2* Interface::GetTarget(TileType type) {
    switch (type) {
        case TILE_POTION: return &m_healthTarget;
        case TILE_SKULL: return &m_xpTarget;
        case TILE_COIN: return &m_goldTarget;
        case TILE_SHIELD: return &m_shieldTarget;
        default: return &s_noTarget;
    }
}

// Mana tokens charge a random skill that is not Mana and not fully charged (-1: none); other
// tokens: 0.
int Interface::GetSpecialTarget(Token* token) {
    if (token->m_kind != 1) return 0;
    std::set<int> skills;
    for (int i = 0; i < m_game->m_skillCount; ++i) {
        Skill* s = m_game->m_skills[i];
        if (s->m_type != SKILL_MANA && s->m_charge != s->m_cooldown) skills.insert(i);
    }
    if (skills.empty()) return -1;
    int index = RandI(0, static_cast<int>(skills.size()) - 1, RAND_OTHER);
    for (int i : skills) {
        if (index == 0) return i;
        --index;
    }
    return 0;
}

GLfloat2 Interface::GetSpecialTarget(int index) {
    static const GLfloat2 kTopButtonPositions[4] = {   // g_topButtonPositions 0x00089464
        {0.1015625f, 0.1015625f}, {0.2859375f, 0.1015625f}, {0.4703125f, 0.1015625f}, {0.6546875f, 0.1015625f}};
    return kTopButtonPositions[index];
}

void Interface::SetHealth(int health) {
    if (m_health != health) {
        m_health = health;
        UpdateHealthBar(BAR_HEALTH, health);
    }
}

// ================================================================ help pages

void Interface::ShowHelpXP() { m_game->ShowHelp(0.5f, 1.446875f, 0.0f, 0.0f, 0xd); }
void Interface::ShowHelpUP() { m_game->ShowHelp(0.5f, 1.390625f, 0.0f, 0.0f, 0xc); }
void Interface::ShowHelpWeaponDmg() { m_game->ShowHelp(0.640625f, 1.265625f, 0.0f, 0.0f, 10); }
void Interface::ShowHelpArmour() { m_game->ShowHelp(0.496875f, 1.265625f, 0.0f, 0.0f, 0xb); }
void Interface::ShowHelpBaseDmg() { m_game->ShowHelp(0.33125f, 1.265625f, 0.0f, 0.0f, 9); }
void Interface::ShowHelpGold() { m_game->ShowHelp(0.140625f, 1.296875f, 0.0f, 0.0f, 7); }
// x is 0x3f5b9999 (0.857812464), one bit below 0.8578125f
void Interface::ShowHelpHealth() { m_game->ShowHelp(0.857812464f, 1.26875f, 0.0f, 0.0f, 0x12); }

// ================================================================ input and frames

bool Interface::CursorInButton(int index) const {
    GLfloat2 b = GetSpecialTarget(index);
    float tx = m_game->m_touchX;
    float ty = m_game->m_touchY;
    return b.x + -0.0828125f < tx && tx < b.x + 0.0828125f && b.y + -0.0828125f < ty && ty < b.y + 0.0828125f;
}

void Interface::ProcessInput() {
    Game* g = m_game;
    int touch = g->m_touchState;
    if (touch == TOUCHING) {
        if (m_lastTouchState == NOT_TOUCHING) {   // a new press
            bool onSkill = false;
            for (int i = 0; i < 4 && !onSkill; ++i) {
                if (!CursorInButton(i) || i >= g->m_skillCount) continue;
                Skill* s = g->m_skills[i];
                if (s->m_charge == s->m_cooldown) {
                    g->QueueSound(SOUND_CLICK);
                    m_pressedSkill = i;
                    m_skillChargeBars[i].SetColour(GLcolour4{0xdc, 0xdc, 0xdc, 0xff});
                }
                ShowSkillTooltip(s->m_type, s->m_cooldown, s->m_charge,
                                 GLfloat2{GetSpecialTarget(i).x - 0.0828125f, 0.228125f});
                onSkill = true;
            }
            if (!onSkill) {
                GLfloat2 t{g->m_touchX, g->m_touchY};
                if (m_menuButton.ContainsPoint(t)) {
                    g->QueueSound(SOUND_CLICK);
                    m_menuButtonPressed = true;
                    m_menuButton.SetAlpha(0x96);
                } else if (m_statsButton.ContainsPoint(t)) {
                    g->QueueSound(SOUND_CLICK);
                    m_statsButtonPressed = true;
                    m_statsButton.SetAlpha(0x96);
                }
            }
        }
    } else if (touch == FINISHED_TOUCHING || touch == CANCELLED_TOUCHING) {
        if (m_pressedSkill >= 0) {
            m_skillChargeBars[m_pressedSkill].SetColour(kWhite);
            if (CursorInButton(m_pressedSkill) && g->m_skills[m_pressedSkill]->CanActivate()) {
                Skill* s = g->m_skills[m_pressedSkill];
                s->Activate();
                s->m_charge = s->m_cooldown & (s->m_cooldown >> 31);   // 0 (a negative cooldown kept)
                if (g->m_classes[g->m_currentClass].m_race == RACE_DRAGONKIN) g->FireBreath();
                UpdateSkills();
            }
            m_pressedSkill = -1;
        }
        HideTooltip();
        if (m_menuButtonPressed) {
            m_menuButton.SetAlpha(0xff);
            m_gameMenu.Show();
            m_menuButtonPressed = false;
        }
        if (m_statsButtonPressed) {
            m_statsButton.SetAlpha(0xff);
            m_statsMenu.Show();
            m_statsButtonPressed = false;
        }
    }
    m_lastTouchState = g->m_touchState;
}

// Below 35% health: the help page about it, and a heartbeat (faster as health drops) that flashes
// the health bar.
void Interface::ProcessHeartbeat(float dt) {
    float ratio = static_cast<float>(m_health) / static_cast<float>(m_maxHealth);
    if (ratio <= 0.35f) {
        m_game->ShowHelp(0.857812464f, 1.26875f, 0.0f, 0.0f, 0xe);
    } else {
        m_heartbeatDelay = 0.0f;
        if (m_heartbeatPulse == 0.0f) return;
    }
    float t = Clamp((ratio - 0.15f) / 0.199999988f, 0.0f, 1.0f);   // 0x3e4ccccc, one bit below 0.2f
    if (m_heartbeatDelay > 0.0f) {
        float d = m_heartbeatDelay - dt;
        m_heartbeatDelay = d;
        if (d <= 0.0f) {
            m_heartbeatDelay = 0.0f;
            float len = Interpolate(0.5f, 0.3f, t);
            m_heartbeatPulse = len;
            m_heartbeatPulseLength = len;
            m_game->QueueSound(SOUND_HEARTBEAT);
        }
    } else {
        float p = m_heartbeatPulse - dt;
        m_heartbeatPulse = p;
        if (p <= 0.0f) {
            m_heartbeatPulse = 0.0f;
            m_heartbeatDelay = Interpolate(2.0f, 0.1f, t);
        }
        UpdateHealthColour();
    }
}

void Interface::ShowMenu(int menu) { m_foldoutMenu.Initialise(menu); }

void Interface::Process(float dt, bool inputReady) {
    if (m_foldoutMenu.m_active) {
        m_foldoutMenu.Process(dt);
        if (!m_foldoutMenu.m_active) ProcessHeartbeat(dt);
    } else if (!m_gameMenu.m_active && !m_statsMenu.m_active && !inputReady) {
        ProcessHeartbeat(dt);
    } else {
        if (m_gameMenu.m_active) {
            m_gameMenu.Process(dt);
        } else if (m_statsMenu.m_active) {
            m_statsMenu.Process(dt);
        } else {
            ProcessInput();
        }
        if (!m_foldoutMenu.m_active) ProcessHeartbeat(dt);
    }
    m_tooltip.Process(dt);
    Game* g = m_game;
    // the skill-ready glows fade in (ready or active skills) or out (5 x 255 per second); a glow
    // that is not white (an active skill) fades its red and green down to 150
    if (g->m_skillCount > 0) {
        int fade = FloatToIntArm((dt * -255.0f) * 5.0f);
        for (int i = 0; i < g->m_skillCount; ++i) {
            Skill* s = g->m_skills[i];
            int d = (s->m_charge == s->m_cooldown || s->m_active) ? -fade : fade;
            Quad& glow = m_skillReadyGlows[i];
            uint8_t a = static_cast<uint8_t>(ClampI(d + glow.m_vertices[0].colour.a, 0, 0xff));
            glow.SetAlpha(a);
            uint8_t r = glow.m_vertices[0].colour.r;
            if (r != 0xff) {
                float c = static_cast<float>(static_cast<int>(r)) + (dt * 255.0f) * -5.0f;
                uint8_t c8 = static_cast<uint8_t>(ClampI(FloatToIntArm(c), 0x96, 0xff));
                glow.SetColour(GLcolour4{c8, c8, 0xff, a});
            }
            g = m_game;
        }
    }
    if (!g->m_challenge || g->m_mode == MODE_CLASSIC) return;
    // the challenge score: a chain's gain (and the Sprint bonus) count up into the shown score
    if (!m_scoreTallying) {
        if (m_scoreGain != 0 || m_scoreBonus != 0) {
            if (g->m_scoreGain != 0 || g->m_scoreBonus != 0) {
                m_scoreGain = g->m_scoreGain;
                m_scoreBonus = g->m_scoreBonus;
            } else {   // the chain is over: start counting
                m_scoreTallying = true;
                m_scoreTallyDelay = 0.2f;
            }
        } else {
            m_displayedScore = g->GetScore();
            m_scoreGain = g->m_scoreGain;
            m_scoreBonus = g->m_scoreBonus;
        }
    } else if (m_scoreTallyDelay > 0.0f) {
        float d = m_scoreTallyDelay - dt;
        m_scoreTallyDelay = d;
        if (d <= 0.0f) m_scoreTallyStep = 1.0f;
    } else if (g->m_mode == MODE_SPRINT && m_scoreBonus > 0) {   // the bonus moves into the gain
        int k = FloatToIntArm(m_scoreTallyStep);
        if (m_scoreBonus < k) k = m_scoreBonus;
        m_scoreBonus -= k;
        m_scoreGain += k;
        if (m_scoreBonus == 0) m_scoreTallyStep = 1.0f;
        m_scoreTallyStep = m_scoreTallyStep * 1.5f;
    } else {   // the gain moves into the score
        if (m_scoreGain > 0) {
            int k = FloatToIntArm(m_scoreTallyStep);
            if (m_scoreGain < k) k = m_scoreGain;
            m_scoreGain -= k;
            if (m_scoreGain == 0) {
                m_scoreBonus = 0;
                m_displayedScore = g->GetScore();
                m_scoreTallying = false;
            } else {
                m_displayedScore += k;
                if (g->GetScore() < m_displayedScore) m_displayedScore = g->GetScore();
            }
        }
        m_scoreTallyStep = m_scoreTallyStep * 1.5f;
    }
    g = m_game;
    if (g->m_challenge) {
        if (g->m_mode == MODE_SPRINT) {
            if (m_scoreGain < 1) {
                if (m_scoreBonus < 1) {
                    std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d", m_displayedScore);
                } else {
                    std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d bonus", m_displayedScore, m_scoreBonus);
                }
            } else if (m_scoreBonus < 1) {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d", m_displayedScore, m_scoreGain);
            } else {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d + %d bonus", m_displayedScore,
                              m_scoreGain, m_scoreBonus);
            }
        } else if (g->m_mode == MODE_PRETZEL) {
            if (m_scoreGain == 0) {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d", m_displayedScore);
            } else if (m_scoreBonus == 0) {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d", m_displayedScore, m_scoreGain);
            } else if (m_scoreBonus == 1) {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d!", m_displayedScore, m_scoreGain);
            } else if (m_scoreBonus == 2) {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d!!", m_displayedScore, m_scoreGain);
            } else {
                std::snprintf(s_scoreText, sizeof(s_scoreText), "Score %d + %d!!!", m_displayedScore, m_scoreGain);
            }
        }
    }
    m_scoreLabel.SetText(s_scoreText, -1);
    char turn[32];   // static buffer 0x00089424
    std::snprintf(turn, sizeof(turn), "Turn %d", ClampI(g->m_turn + 1, 1, 100));
    m_turnLabel.SetText(turn, -1);
    m_turnLabel.m_position.x = 0.975f - m_turnLabel.m_width * m_turnLabel.m_scale.x;
}

// ================================================================ rendering

void Interface::Render() {
    m_bottomPanel.Render();
    if (m_healthShadow > 0) m_healthShadowBar.Render();
    if (m_xpShadow > 0) m_xpShadowBar.Render();
    if (m_upgradeShadow > 0) m_upgradeShadowBar.Render();
    if (m_goldShadow > 0) {
        m_goldShadowBar[0].Render();
        m_goldShadowBar[1].Render();
    }
    m_healthBar.Render();
    m_xpBar.Render();
    m_upgradeBar.Render();
    m_goldBar[0].Render();
    m_goldBar[1].Render();
    m_topPanel.Render();
    for (int i = 0; i < 4; ++i) {
        m_skillButtons[i].Render();
        if (i < m_game->m_skillCount) {
            m_skillChargeBars[i].Render();
            m_skillReadyGlows[i].Render();
            m_game->m_skills[i]->RenderIcon(true);
            sdlBindTexture(GL_TEXTURE_2D, 1);
        }
    }
    m_menuButton.Render();
    m_statsButton.Render();
    if (m_clockVisible) m_clockLabel.Render();
    if (m_game->m_challenge) {
        if (m_game->m_mode == MODE_SPRINT) {
            m_scoreLabel.Render();
            m_turnLabel.Render();
        } else if (m_game->m_mode == MODE_PRETZEL) {
            m_scoreLabel.Render();
        }
    }
    m_swordLabel.Render();
    m_shieldLabel.Render();
    m_skullLabel.Render();
    m_healthLabel.Render();
    if (m_poison > 0) m_poisonLabel.Render();
}

void Interface::RenderMenu() {
    if (m_foldoutMenu.m_active) {
        m_foldoutMenu.Render();
    } else if (m_gameMenu.m_active) {
        m_gameMenu.Render();
    } else if (m_statsMenu.m_active) {
        m_statsMenu.Render();
    }
}

void Interface::RenderTooltip() {
    if (m_tooltip.m_visible) m_tooltip.Render();
}
