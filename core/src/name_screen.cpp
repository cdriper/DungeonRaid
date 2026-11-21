#include "name_screen.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "character_class.h"
#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "game_tables.h"
#include "rand.h"
#include "skill.h"
#include "util.h"

namespace {
const GLcolour4 kPanelColour{0x96, 0x96, 0xff, 0xff};
const GLcolour4 kHeading{0x7a, 0x67, 0xd3, 0xff};
const float kPx4 = 1.0f / 512.0f;
const float kPx = 1.0f / 1024.0f;

// The mode buttons' icons (0x000895d8 / 0x000895e8, the same rectangles as ModeScreen's)
const GLfloat2 kModeIconTopLeft[2] = {{399.0f, 378.0f}, {409.0f, 324.0f}};
const GLfloat2 kModeIconBottomRight[2] = {{511.0f, 431.0f}, {511.0f, 377.0f}};

// GenerateRandomName's syllables (0x0006da04, 0x0006db00: 5-byte rows)
const char* const kNameStarts[50] = {"Ar", "Ang", "Bal", "Bee", "Clen", "Coy", "Def", "Dil", "En", "Eck",
                                     "Fan", "Fen", "Gam", "Gil", "Hel", "Har", "Il", "Ink", "Jos", "Jen",
                                     "Kart", "Kea", "Lid", "Lin", "Mer", "Mond", "Neg", "Non", "Orn", "Oart",
                                     "Pey", "Put", "Quo", "Quin", "Rin", "Roz", "So", "Sit", "Tec", "Tint",
                                     "Vin", "Vam", "Wel", "Wit", "Xer", "Xin", "Ye", "Yon", "Zil", "Zar"};
const char* const kNameEnds[49] = {"", "and", "aar", "bole", "bea", "cor", "cut", "din", "dier", "een",
                                   "ell", "fet", "fall", "gren", "gap", "het", "hob", "inn", "ire", "jat",
                                   "jel", "kenn", "kaf", "lyt", "lack", "mon", "mud", "ney", "nant", "ons",
                                   "ore", "pol", "pent", "rew", "rog", "sear", "sen", "ten", "treb", "un",
                                   "ure", "ved", "vie", "wim", "wed", "yet", "yore", "zim", "zes"};

char s_level[8];   // 0x000895f8

bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }       // bionic _ctype_ & _U
char ToLower(char c) { return IsUpper(c) ? static_cast<char>(c - 'A' + 'a') : c; }

bool Unlocked(Game* g, int c) { return g->m_classes[c].m_level != 0; }

void PressColour(QuadStrip& button, Label& label) {
    button.SetColour(DivideColour(kPanelColour, 1.2f));
    label.m_colour = DivideColour(kHeading, 1.2f);
    label.UpdateColour();
}

void ReleaseColour(QuadStrip& button, Label& label) {
    button.SetColour(kPanelColour);
    label.m_colour = kHeading;
    label.UpdateColour();
}

void IconUV(Quad& q, const GLfloat2& tl, const GLfloat2& br) {
    q.SetUV(GLfloat2{tl.x / 512.0f, tl.y / 512.0f}, GLfloat2{br.x / 512.0f, br.y / 512.0f});
}

// a quad centred in a strip, scaled to the icon rectangle (texture pixels / 320)
void CentreIcon(Quad& q, QuadStrip& s, const GLfloat2& tl, const GLfloat2& br) {
    q.m_scale = GLfloat2{(br.x - tl.x) / 320.0f, (br.y - tl.y) / 320.0f};
    GLfloat2 size = s.GetSize();
    q.m_position.x = size.x / 2.0f + s.m_position.x;
    q.m_position.y = s.m_position.y + size.y / 2.0f;
}
}  // namespace

NameScreen::NameScreen(Game* game)
    : m_game(game), m_nameLabel(&game->m_fonts[0]), m_begin(&game->m_fonts[0]), m_classScreen(game), m_modeScreen(game) {
    const float edge = 0.015625f;
    m_background.SetSize(GLfloat2{2.0f, 2.0f});
    m_background.m_position = GLfloat2{0.5f, 0.5f};
    m_background.SetColour(GLcolour4{0, 0, 0, 0xff});
    m_panelBackground.SetSize(GLfloat2{2.0f, 1.62f});
    m_panelBackground.m_position = GLfloat2{0.5f, 0.5f};
    m_panelBackground.SetColour(GLcolour4{45, 45, 75, 0xff});
    m_keyboardBackground.SetSize(GLfloat2{1.0f, 1.0f});
    m_keyboardBackground.m_position = GLfloat2{0.5f, 0.449999988f};
    m_keyboardBackground.SetColour(GLcolour4{0, 0, 0, 0xff});
    // the keyboard: three rows of letters, then shift, delete, space, OK
    for (int i = 0; i < kKeys; ++i) {
        Label* l = new Label(&game->m_fonts[0]);
        m_keyLabels[i] = l;
        l->m_scale = GLfloat2{0.140000001f, 0.140000001f};
        if (i >= 26) continue;
        Quad& k = m_keys[i];
        k.SetUV(GLfloat2{0.947265625f, 0.921875f}, GLfloat2{0.998046875f, 0.998046875f});
        k.m_scale = GLfloat2{0.081249997f, 0.121875003f};
        float col = static_cast<float>(i % 10);
        float y;
        if (i < 10) {
            y = 0.0703125f;
        } else {
            col = col + 0.5f;
            if (i < 19) {
                y = 0.2390625f;
            } else {
                y = 0.407812506f;
                col = i == 19 ? 1.5f : col + 2.0f;
            }
        }
        k.m_position.y = y;
        float x = 0.049999997f + col * 0.081249997f;
        k.m_position.x = x + col * 0.0187500007f;
    }
    for (Label& l : m_classLevels) {
        l.m_font = &game->m_fonts[0];
        l.m_scale = GLfloat2{0.0799999982f, 0.0799999982f};
    }
    m_keys[26].SetUV(GLfloat2{0.875f, 0.921875f}, GLfloat2{0.9453125f, 0.998046875f});
    m_keys[26].m_scale = GLfloat2{0.112499997f, 0.121875003f};
    m_keys[26].m_position = GLfloat2{0.065624997f, 0.407812506f};
    m_keys[27].SetUV(GLfloat2{0.802734375f, 0.921875f}, GLfloat2{0.873046875f, 0.998046875f});
    m_keys[27].m_scale = GLfloat2{0.112499997f, 0.121875003f};
    m_keys[27].m_position = GLfloat2{0.934374988f, 0.407812506f};
    m_keys[28].SetUV(GLfloat2{0.5f, 0.921875f}, GLfloat2{0.80078125f, 0.998046875f});
    m_keys[28].m_scale = GLfloat2{0.481249988f, 0.121875003f};
    m_keys[28].m_position = GLfloat2{0.5f, 0.576562524f};
    m_keys[29].SetUV(GLfloat2{0.853515625f, 0.84375f}, GLfloat2{0.998046875f, 0.919921875f});
    m_keys[29].m_scale = GLfloat2{0.231250003f, 0.121875003f};
    m_keys[29].m_position = GLfloat2{0.875f, 0.576562524f};
    GLfloat2 dummyTL, dummyBR, smallTL, smallBR;
    CharacterClass::GetDummyIconCoords(dummyTL, dummyBR);
    CharacterClass::GetDummySmallIconCoords(smallTL, smallBR);
    // the name panel
    ModeScreen::MakePanel(m_namePanel, true, 0.96875f, 0.18437499f, edge, edge);
    GLfloat2 size = m_namePanel.GetSize();
    ModeScreen::MakeButton(m_nameField, 0.69921875f, 0.081249997f, m_namePanel.m_position.x + edge,
                           ((m_namePanel.m_position.y + size.y) - 0.081249997f) - edge);
    size = m_namePanel.GetSize();
    ModeScreen::MakeButton(m_randomButton, 0.22265625f, 0.081249997f, ((m_namePanel.m_position.x + size.x) - 0.22265625f) - edge,
                           m_nameField.m_position.y);
    m_nameField.SetColour(GLcolour4{95, 95, 150, 0xff});
    // difficulty (or mode)
    size = m_namePanel.GetSize();
    ModeScreen::MakePanel(m_difficultyPanel, true, 0.96875f, 0.26875f, edge, (m_namePanel.m_position.y + size.y) + 0.03125f);
    for (int i = 0; i < 4; ++i) {
        ModeScreen::MakeButton(m_difficultyButtons[i], 0.22265625f, 0.165625006f,
                               (m_difficultyPanel.m_position.x + edge) + static_cast<float>(i) * 0.23828125f,
                               (m_difficultyPanel.m_position.y + 0.0718749985f) + edge);
    }
    size = m_difficultyPanel.GetSize();
    ModeScreen::MakeButton(m_modeHelpButton, 0.165625006f, 0.065624997f,
                           ((m_difficultyPanel.m_position.x + size.x) - 0.165625006f) - edge,
                           m_difficultyPanel.m_position.y + 0.00312500005f);
    for (int i = 0; i < 2; ++i) {
        ModeScreen::MakeButton(m_modeButtons[i], 0.4609375f, 0.165625006f,
                               (m_difficultyPanel.m_position.x + edge) + static_cast<float>(i) * 0.4765625f,
                               (m_difficultyPanel.m_position.y + 0.0718749985f) + edge);
        m_modeIcons[i].AddQuad(kModeIconTopLeft[i], kModeIconBottomRight[i], 2, -1.0f);
        m_modeIcons[i].AlignWith(m_modeButtons[i]);
    }
    // the classes
    size = m_difficultyPanel.GetSize();
    ModeScreen::MakePanel(m_classPanel, true, 0.96875f, 0.46171877f, edge, (m_difficultyPanel.m_position.y + size.y) + 0.03125f);
    size = m_classPanel.GetSize();
    ModeScreen::MakeButton(m_classHelpButton, 0.165625006f, 0.065624997f, ((m_classPanel.m_position.x + size.x) - 0.165625006f) - edge,
                           m_classPanel.m_position.y + 0.00312500005f);
    const float cell = 0.192968756f;
    for (int i = 0; i < 10; ++i) {
        float x = (m_classPanel.m_position.x + edge) + static_cast<float>(i % 5) * cell;
        float y = ((m_classPanel.m_position.y + 0.0718749985f) + edge) + static_cast<float>(i / 5) * cell;
        ModeScreen::MakeButton(m_classButtons[i], 0.165625006f, 0.165625006f, x, y);
        CentreIcon(m_classIcons[i], m_classButtons[i], dummyTL, dummyBR);
    }
    // the chosen class's properties
    size = m_classPanel.GetSize();
    ModeScreen::MakePanel(m_propertiesPanel, true, 0.96875f, 0.26875f, edge, (m_classPanel.m_position.y + size.y) + 0.03125f);
    size = m_propertiesPanel.GetSize();
    ModeScreen::MakeButton(m_editButton, 0.165625006f, 0.065624997f, ((m_propertiesPanel.m_position.x + size.x) - 0.165625006f) - edge,
                           m_propertiesPanel.m_position.y + 0.00312500005f);
    QuadStrip* slots = m_propertySlots;
    ModeScreen::MakePanel(slots[0], false, 0.165625006f, 0.165625006f, m_propertiesPanel.m_position.x + edge,
                          (m_propertiesPanel.m_position.y + 0.0718749985f) + edge);
    size = slots[0].GetSize();
    ModeScreen::MakePanel(slots[1], false, 0.165625006f, 0.165625006f, (slots[0].m_position.x + size.x) + 0.02734375f,
                          slots[0].m_position.y);
    size = m_propertiesPanel.GetSize();
    ModeScreen::MakePanel(slots[10], false, 0.165625006f, 0.165625006f,
                          ((m_propertiesPanel.m_position.x + size.x) - edge) - 0.165625006f, slots[0].m_position.y);
    CentreIcon(m_propertyIcons[0], slots[0], dummyTL, dummyBR);
    CentreIcon(m_propertyIcons[1], slots[1], dummyTL, dummyBR);
    CentreIcon(m_propertyIcons[10], slots[10], dummyTL, dummyBR);
    // perks (top row) and flaws (bottom row): small slots right of the race
    const float step = 0.084374994f;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 2; ++j) {
            int k = i + 2 + 4 * j;
            size = slots[1].GetSize();
            float x = ((slots[1].m_position.x + size.x) + 0.0406249985f) + static_cast<float>(i) * step;
            ModeScreen::MakePanel(slots[k], false, 0.081249997f, 0.081249997f, x, slots[1].m_position.y + static_cast<float>(j) * step);
            CentreIcon(m_propertyIcons[k], slots[k], smallTL, smallBR);
        }
    }
    float h = ((1.5f - m_panelBackground.m_position.y) + m_panelBackground.m_size.y * -0.5f) - 0.03125f;
    ModeScreen::MakeButton(m_backButton, 0.18125f, h, edge, 1.484375f - h);
}

NameScreen::~NameScreen() {
    for (Label* l : m_keyLabels) delete l;
}

void NameScreen::Initialise() {
    m_begin.m_scale = GLfloat2{0.13f, 0.13f};
    m_begin.SetText("Begin", -1);
    m_begin.m_position.x = (1.0f - m_begin.m_width * m_begin.m_scale.x) * 0.5f;
    m_begin.m_position.y = 1.43f;
    for (int i = 0; i < kKeys; ++i) {
        Label* l = m_keyLabels[i];
        l->SetText(g_keyText[i], -1);
        l->m_position.x = m_keys[i].m_position.x + (l->m_width * l->m_scale.x) * -0.5f;
        l->m_position.y = m_keys[i].m_position.y + 0.0234375f;
    }
    m_nameLabel.m_scale = GLfloat2{0.119999997f, 0.119999997f};
    m_nameLabel.m_spacing = 2.0f;
    MakeHeadingLabel(m_namePanel, m_nameHeading, "Character Name");
    MakeHeadingLabel(m_difficultyPanel, m_difficultyHeading, "Difficulty");
    MakeHeadingLabel(m_difficultyPanel, m_modeHeading, "Challenge Mode");
    MakeHeadingLabel(m_classPanel, m_classHeading, "Character Class");
    MakeHeadingLabel(m_propertiesPanel, m_propertiesHeading, "Class Properties");
    MakeButtonLabel(m_randomButton, m_randomLabel, "Random");
    MakeButtonLabel(m_modeHelpButton, m_modeHelpLabel, "Help");
    MakeButtonLabel(m_classHelpButton, m_classHelpLabel, "Help");
    MakeButtonLabel(m_editButton, m_editLabel, "Edit");
    MakeButtonLabel(m_backButton, m_backLabel, "Back");
    for (int i = 0; i < 4; ++i) MakeButtonLabel(m_difficultyButtons[i], m_difficultyLabels[i], Game::GetDifficultyName(i));
    m_modeScreen.Initialise();
}

void NameScreen::MakeHeadingLabel(QuadStrip& panel, Label& label, const char* text) {
    m_modeScreen.MakeHeadingLabel(panel, label, text);   // (the same code)
}

void NameScreen::MakeButtonLabel(QuadStrip& button, Label& label, const char* text) {
    label.m_scale = GLfloat2{0.059375f, 0.059375f};
    label.m_font = &m_game->m_fonts[1];
    label.m_spacing = 2.0f;
    label.SetText(text, -1);
    label.m_colour = kHeading;
    label.UpdateColour();
    float x = button.m_position.x;
    GLfloat2 size = button.GetSize();
    x = x + size.x * 0.5f;
    x = (x + (label.m_width * label.m_scale.x) * -0.5f) - 0.00625f;
    float y = button.m_position.y;
    size = button.GetSize();
    label.m_position.x = x;
    label.m_position.y = (y + size.y * 0.5f) + 0.01875f;
}

void NameScreen::GenerateRandomName() {
    if (RandI(0, 1, RAND_COSMETIC) == 0) {
        int a = RandI(0, 0x31, RAND_COSMETIC);
        int b = RandI(0, 0x30, RAND_COSMETIC);
        std::snprintf(m_name, sizeof(m_name), "%s%s", kNameStarts[a], kNameEnds[b]);
    } else {
        int a = RandI(0, 0x31, RAND_COSMETIC);
        int b = RandI(0, 0x30, RAND_COSMETIC);
        int c = RandI(0, 0x31, RAND_COSMETIC);
        int d = RandI(0, 0x30, RAND_COSMETIC);
        std::snprintf(m_name, sizeof(m_name), "%s%s %s%s", kNameStarts[a], kNameEnds[b], kNameStarts[c], kNameEnds[d]);
    }
    m_nameLength = static_cast<int>(std::strlen(m_name));
}

// The name in its field, shortened until it fits.
void NameScreen::UpdateName() {
    m_nameLabel.SetText(m_name, -1);
    for (;;) {
        float w = m_nameLabel.m_width * m_nameLabel.m_scale.x;
        GLfloat2 size = m_nameField.GetSize();
        if (!(size.x - 0.015625f < w)) break;
        int n = m_nameLength;
        m_nameLength = n - 1;
        m_name[n - 1] = '\0';
        m_nameLabel.SetText(m_name, -1);
    }
    float x = m_nameField.m_position.x;
    float y = m_nameField.m_position.y;
    GLfloat2 size = m_nameField.GetSize();
    m_nameLabel.m_position.x = x + 0.015625f;
    m_nameLabel.m_position.y = (y + size.y) - 0.015625f;
}

void NameScreen::UpdateDifficulty() {
    for (int i = 0; i < 4; ++i) {
        if (i == static_cast<int>(m_game->m_difficulty)) {
            m_difficultyButtons[i].SetColour(DivideColour(kPanelColour, 2.0f));
        } else {
            m_difficultyButtons[i].SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
        }
    }
}

void NameScreen::UpdateMode() {
    for (int i = 0; i < 2; ++i) {
        if (i == static_cast<int>(m_game->m_mode)) {
            m_modeButtons[i].SetColour(DivideColour(kPanelColour, 2.0f));
        } else {
            m_modeButtons[i].SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
        }
    }
    m_modeScreen.UpdateMode();
}

void NameScreen::FadeOut() {
    m_closing = true;
    m_fadeTime = 0.3f;
}

void NameScreen::Show(const char* name, bool challenge) {
    Game* g = m_game;
    m_challenge = challenge;
    m_fadeTime = 0.3f;
    m_active = true;
    m_lastTouchState = TOUCHING;
    m_keyboardT = 0.0f;
    m_editing = false;
    m_deleteTimer = 0.0f;
    m_begin.m_colour = GLcolour4{0xff, 0xff, 0, 0};
    m_closing = false;
    m_begin.UpdateColour();
    m_nameLength = static_cast<int>(std::strlen(name));
    if (m_nameLength == 0) {
        std::memcpy(m_name, "Enter name", 11);
        m_named = false;
    } else {
        std::strncpy(m_name, name, 0x10);
        m_name[16] = '\0';
        m_named = true;
    }
    UpdateName();
    UpdateDifficulty();
    UpdateMode();
    for (int c = 0; c < 10; ++c) {
        if (Unlocked(g, c)) {
            GLfloat2 tl, br;
            CharacterClass::GetIconCoords(static_cast<ClassType>(c), tl, br);
            IconUV(m_classIcons[c], tl, br);
        } else {   // a lock
            m_classIcons[c].SetUV(GLfloat2{0.72265625f, 0.5078125f}, GLfloat2{421 * kPx4, 0.607421875f});
        }
    }
    for (int c = 0; c < 10; ++c) {
        Label& l = m_classLevels[c];
        if (c == 0 || !Unlocked(g, c)) {
            l.SetText("", -1);
        } else {
            std::snprintf(s_level, sizeof(s_level), "%d", static_cast<int>(g->m_classes[c].m_level));
            l.SetText(s_level, -1);
        }
        float x = m_classButtons[c].m_position.x;
        GLfloat2 size = m_classButtons[c].GetSize();
        float lx = ((x + size.x) - l.m_width * l.m_scale.x) - 0.00937500037f;
        float y = m_classButtons[c].m_position.y;
        size = m_classButtons[c].GetSize();
        l.m_position.x = lx;
        l.m_position.y = (y + size.y) - 0.00937500037f;
    }
    SelectClass(g->m_classes[g->m_currentClass]);
    SetAlpha(0);
}

// The chosen class's icons (class, race, special skill, perks and flaws, or locks for the ones
// not unlocked yet) and the class buttons (the chosen one darker, locked ones dimmed).
void NameScreen::SelectClass(CharacterClass& c) {
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(c.m_class, tl, br);
    IconUV(m_propertyIcons[0], tl, br);
    CharacterClass::GetIconCoords(c.m_race, tl, br);
    IconUV(m_propertyIcons[1], tl, br);
    if (c.m_class == CLASS_ADVENTURER) {
        Skill::GetIconUVs(SKILL_TELEPORT, tl, br);
    } else {
        Skill::GetIconUVs(c.m_baseSkills[0], tl, br);
    }
    m_propertyIcons[10].SetUV(tl, br);
    // an open slot without a perk/flaw: the dummy icon (texture pixels -24..0, which the original's
    // repeating texture wraps to the transparent top-right 24 x 24 corner of texture 4: just the slot
    // frame shows). Port: the same texels without relying on wrapping, through IconUV like the others
    // (the pixels were passed to SetUV unconverted, which drew the whole texture tiled: noise).
    GLfloat2 noneTL, noneBR;
    CharacterClass::GetDummySmallIconCoords(noneTL, noneBR);
    noneTL.x += 512.0f;
    noneBR.x += 512.0f;
    const GLfloat2 lockTL{0.673828125f, 0.560546875f};
    const GLfloat2 lockBR{0.720703125f, 0.607421875f};
    for (int k = 0; k < 4; ++k) {
        Quad& perk = m_propertyIcons[2 + k];
        Quad& flaw = m_propertyIcons[6 + k];
        if (!c.IsPerkUnlocked(k) || c.m_class == CLASS_ADVENTURER) {
            perk.SetUV(lockTL, lockBR);
            flaw.SetUV(lockTL, lockBR);
            continue;
        }
        if (c.m_perks[k] == PERK_NONE) {
            IconUV(perk, noneTL, noneBR);
        } else {
            CharacterClass::GetIconCoords(c.m_perks[k], tl, br);
            IconUV(perk, tl, br);
        }
        if (c.m_flaws[k] == FLAW_NONE) {
            IconUV(flaw, noneTL, noneBR);
        } else {
            CharacterClass::GetIconCoords(c.m_flaws[k], tl, br);
            IconUV(flaw, tl, br);
        }
    }
    for (int k = 0; k < 10; ++k) {
        if (k == static_cast<int>(c.m_class) || !Unlocked(m_game, k)) {
            m_classButtons[k].SetColour(DivideColour(kPanelColour, 2.0f));
        } else {
            m_classButtons[k].SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
        }
    }
}

void NameScreen::SetAlpha(int alpha) {
    uint8_t a = static_cast<uint8_t>(alpha);
    auto label = [a](Label& l) {
        l.m_colour.a = a;
        l.UpdateColour();
    };
    m_background.SetAlpha(a);
    m_panelBackground.SetAlpha(a);
    m_keyboardBackground.SetAlpha(a);
    m_namePanel.SetAlpha(a);
    label(m_nameHeading);
    m_nameField.SetAlpha(a);
    label(m_nameLabel);
    m_randomButton.SetAlpha(a);
    label(m_randomLabel);
    m_difficultyPanel.SetAlpha(a);
    label(m_difficultyHeading);
    for (int i = 0; i < 4; ++i) {
        m_difficultyButtons[i].SetAlpha(a);
        label(m_difficultyLabels[i]);
    }
    label(m_modeHeading);
    m_modeHelpButton.SetAlpha(a);
    label(m_modeHelpLabel);
    m_modeButtons[0].SetAlpha(a);
    m_modeIcons[0].SetAlpha(a);
    m_modeButtons[1].SetAlpha(a);
    m_modeIcons[1].SetAlpha(a);
    m_classPanel.SetAlpha(a);
    label(m_classHeading);
    m_classHelpButton.SetAlpha(a);
    label(m_classHelpLabel);
    for (int i = 0; i < 10; ++i) {
        m_classButtons[i].SetAlpha(a);
        m_classIcons[i].SetAlpha(a);
        label(m_classLevels[i]);
    }
    m_propertiesPanel.SetAlpha(a);
    label(m_propertiesHeading);
    m_editButton.SetAlpha(a);
    label(m_editLabel);
    for (int i = 0; i < 11; ++i) {
        m_propertySlots[i].SetAlpha(a);
        m_propertyIcons[i].SetAlpha(a);
    }
    for (int i = 0; i < kKeys; ++i) {
        m_keys[i].SetAlpha(a);
        label(*m_keyLabels[i]);
    }
    m_backButton.SetAlpha(a);
    label(m_backLabel);
    float f = static_cast<float>(static_cast<int>(m_begin.m_colour.a) * alpha);
    m_begin.m_colour.a = static_cast<uint8_t>(FloatToUintArm(f / 255.0f));
    m_begin.UpdateColour();
}

void NameScreen::Process(float dt) {
    if (m_classScreen.m_active) {
        m_classScreen.Process(dt);
        return;
    }
    if (m_modeScreen.m_active) {
        UpdateMode();
        m_modeScreen.Process(dt);
        return;
    }
    if (m_fadeTime > 0.0f) {
        float t = m_fadeTime - dt;
        m_fadeTime = t;
        if (t < 0.0f) {
            m_fadeTime = 0.0f;
            t = 0.0f;
            if (m_closing) m_active = false;
        }
        int a = FloatToIntArm(Interpolate(0.0f, 255.0f, t / 0.3f));
        if (m_closing) a = 0xff - a;
        SetAlpha(a);
        return;
    }
    float step = dt * 3.0f;
    if (!m_editing) step = -step;
    m_keyboardT = Clamp(m_keyboardT + step, 0.0f, 1.0f);
    ProcessInput(dt);
    m_lastTouchState = m_game->m_touchState;
    UpdateDifficulty();
    UpdateMode();
    int delta = !m_editing && m_named && m_nameLength != 0 ? FloatToIntArm(dt * 300.0f) : -FloatToIntArm(dt * 900.0f);
    m_begin.m_colour.a = static_cast<uint8_t>(ClampI(static_cast<int>(m_begin.m_colour.a) + delta, 0, 0xff));
    m_begin.UpdateColour();
    if (!m_editing) {
        if (m_named) {
            m_name[m_nameLength] = '\0';
            UpdateName();
        }
        return;
    }
    m_blink = std::fmod(dt + m_blink, 1.0f);
    if (m_blink > 0.5f) {
        m_name[m_nameLength] = '\0';
    } else {   // the cursor
        m_name[m_nameLength] = '|';
        m_name[m_nameLength + 1] = '\0';
    }
    UpdateName();
    uint8_t okAlpha = m_nameLength < 1 ? 0x40 : 0xff;
    m_keys[29].SetAlpha(okAlpha);
    m_keyLabels[29]->m_colour.a = okAlpha;
    m_keyLabels[29]->UpdateColour();
    if (m_caps) {
        m_keys[26].SetUV(GLfloat2{0.5625f, 0.765625f}, GLfloat2{0.6328125f, 0.841796875f});
    } else {
        m_keys[26].SetUV(GLfloat2{0.875f, 0.921875f}, GLfloat2{0.9453125f, 0.998046875f});
    }
}

void NameScreen::ProcessInput(float dt) {
    Game* g = m_game;
    if (g->m_touchState == NOT_TOUCHING) {   // a release: the pressed key or button back to normal
        int p = m_pressed;
        if (p < 0) return;
        if (p < 26) {
            m_keys[p].SetUV(GLfloat2{0.947265625f, 0.921875f}, GLfloat2{0.998046875f, 0.998046875f});
        } else if (p == 28) {
            m_keys[28].SetUV(GLfloat2{0.5f, 0.921875f}, GLfloat2{0.80078125f, 0.998046875f});
        } else if (p == 27) {
            m_keys[27].SetUV(GLfloat2{0.802734375f, 0.921875f}, GLfloat2{0.873046875f, 0.998046875f});
        } else if (p == 29) {
            m_keys[29].SetUV(GLfloat2{0.853515625f, 0.84375f}, GLfloat2{0.998046875f, 0.919921875f});
        } else if (p == 0x1e) {
            ReleaseColour(m_randomButton, m_randomLabel);
        } else if (p == 0x1f) {
            ReleaseColour(m_editButton, m_editLabel);
        } else if (p == 0x20) {
            ReleaseColour(m_classHelpButton, m_classHelpLabel);
        } else if (p == 0x21) {
            ReleaseColour(m_backButton, m_backLabel);
        } else if (p == 0x22) {
            ReleaseColour(m_modeHelpButton, m_modeHelpLabel);
        }
        m_pressed = -1;
        return;
    }
    GLfloat2 p{g->m_touchX, g->m_touchY};
    if (g->m_touchState != TOUCHING) return;
    if (m_editing && m_pressed == 0x1b) {   // delete held: repeats every 0.1 s
        GLfloat2 k{p.x, p.y - 0.853125f};
        if (m_keys[27].ContainsPoint(k)) {
            float t = m_deleteTimer - dt;
            m_deleteTimer = t;
            if (!(t < 0.0f) || m_nameLength < 1) return;
            g->QueueSound(SOUND_CLICK);
            int n = m_nameLength;
            m_deleteTimer = 0.1f;
            m_nameLength = n - 1;
            m_caps = n - 1 == 0 ? true : IsUpper(m_name[n - 1]);
            return;
        }
    }
    if (m_lastTouchState != NOT_TOUCHING) return;   // only new presses below
    if (m_nameField.ContainsPoint(p)) {
        if (!m_editing) g->QueueSound(SOUND_CLICK);
        m_editing = true;
        if (m_named) return;
        m_nameLength = 0;
        m_blink = 0.0f;
        m_named = true;
        return;
    }
    if (m_randomButton.ContainsPoint(p)) {
        g->QueueSound(SOUND_CLICK);
        m_pressed = 0x1e;
        PressColour(m_randomButton, m_randomLabel);
        GenerateRandomName();
        m_caps = false;
        m_named = true;
        return;
    }
    if (!m_editing) {
        if (m_editButton.ContainsPoint(p)) {
            g->QueueSound(SOUND_CLICK);
            m_pressed = 0x1f;
            PressColour(m_editButton, m_editLabel);
            g->ShowClassScreen();   // ClassScreen::Initialise
            return;
        }
        if (m_classHelpButton.ContainsPoint(p)) {
            g->QueueSound(SOUND_CLICK);
            m_pressed = 0x20;
            PressColour(m_classHelpButton, m_classHelpLabel);
            HelpMenu& help = g->m_helpMenu;
            GLfloat2 m = m_classPanel.GetMiddle();
            help.QueueHelp("Character classes are an optional part of the game! Ignore these until you're comfortable with the gameplay",
                           m.x, m.y, 0.0f);
            m = m_classPanel.GetMiddle();
            help.QueueHelp("You can choose a character class each time you start a game. Character classes offer special abilities "
                           "as well as a set of advantages & disadvantages, and become more customisable the more you play with them",
                           m.x, m.y, 0.0f);
            help.QueueHelp("Initially you will only have the Adventurer class unlocked, but killing special enemies on non-Easy "
                           "difficulties gives you a chance to unlock and level up other classes",
                           m_classIcons[0].m_position.x, m_classIcons[0].m_position.y, 0.0f);
            m = m_propertiesPanel.GetMiddle();
            help.QueueHelp("These icons show your selected class, its race, perks & flaws and special skill", m.x, m.y, 0.0f);
            float ex = m_editButton.m_position.x;
            GLfloat2 size = m_editButton.GetSize();
            help.QueueHelp("Tap the Edit button to view and customise your unlocked classes (the Adventurer class cannot be customised)",
                           ex + size.x * 0.5f, m_editButton.m_position.y, 0.0f);
            return;
        }
        if (m_backButton.ContainsPoint(p)) {
            g->QueueSound(SOUND_CLICK);
            m_pressed = 0x21;
            PressColour(m_backButton, m_backLabel);
            FadeOut();
            g->m_mainMenu.Show();
            g->m_mainMenu.SetActive(true);
            return;
        }
    }
    if (m_modeHelpButton.ContainsPoint(p)) {
        g->QueueSound(SOUND_CLICK);
        m_pressed = 0x22;
        PressColour(m_modeHelpButton, m_modeHelpLabel);
        m_modeScreen.Show();
        return;
    }
    if (!m_challenge) {
        for (int i = 0; i < 4; ++i) {
            if (m_difficultyButtons[i].ContainsPoint(p)) {
                g->QueueSound(SOUND_CLICK);
                g->m_difficultyChanged = static_cast<int>(g->m_difficulty) != ClampI(i, 0, 3);
                g->m_difficulty = static_cast<Difficulty>(ClampI(i, 0, 3));
                return;
            }
        }
    } else {
        int mode = -1;
        if (m_modeButtons[0].ContainsPoint(p)) {
            mode = 0;
        } else if (m_modeButtons[1].ContainsPoint(p)) {
            mode = 1;
        }
        if (mode >= 0) {
            g->QueueSound(SOUND_CLICK);
            g->m_modeChanged = static_cast<int>(g->m_mode) != ClampI(mode, 0, 1);
            g->m_mode = static_cast<GameMode>(ClampI(mode, 0, 1));
            return;
        }
    }
    if (!m_editing) {
        for (int c = 0; c < 10; ++c) {
            if (!Unlocked(g, c) || !m_classButtons[c].ContainsPoint(p)) continue;
            g->QueueSound(SOUND_CLICK);
            g->m_currentClass = Unlocked(g, c) ? c : 0;
            g->m_nameScreen.SelectClass(g->m_classes[g->m_currentClass]);
            g->m_currentClassChanged = true;
            return;
        }
        // Begin (once it has faded in): the name taken, the intro and a new game
        if (m_begin.m_colour.a != 0xff || p.y <= 1.32999992f || p.x <= 0.25f || !(p.x < 0.75f)) return;
        m_begin.m_colour = GLcolour4{0xb4, 0xb4, 0, 0xff};
        m_begin.UpdateColour();
        g->QueueSound(SOUND_CLICK);
        g->m_nameChanged = std::strcmp(g->m_name, m_name) != 0;
        std::strncpy(g->m_name, m_name, 0x10);
        g->m_name[16] = '\0';
        g->m_introScreen.Initialise();
        g->InitNewGame();
        m_active = false;
        return;
    }
    // the keyboard (it slides up to y 0.853125)
    GLfloat2 k{p.x, p.y - 0.853125f};
    for (int i = 0; i < kKeys; ++i) {
        if (!m_keys[i].ContainsPoint(k)) continue;
        bool click = true;
        if (i < 26) {
            if (m_nameLength < 0x10) {
                char c = m_keyLabels[i]->m_text.c_str()[0];
                if (!m_caps) c = ToLower(c);
                m_name[m_nameLength++] = c;
                m_caps = false;
            }
            m_pressed = i;
            m_keys[i].SetUV(GLfloat2{0.80078125f, 0.84375f}, GLfloat2{0.8515625f, 0.919921875f});
        } else if (i == 28) {
            if (m_nameLength != 0 && m_nameLength < 0x10) {
                m_name[m_nameLength++] = ' ';
                m_caps = false;
            }
            m_pressed = 28;
            m_keys[28].SetUV(GLfloat2{0.498046875f, 0.84375f}, GLfloat2{0.798828125f, 0.919921875f});
        } else if (i == 27) {
            int n = m_nameLength;
            if (n > 0) {
                m_nameLength = n - 1;
                m_caps = n - 1 == 0 ? true : IsUpper(m_name[n - 1]);
            }
            m_deleteTimer = 0.5f;
            m_pressed = 27;
            m_keys[27].SetUV(GLfloat2{0.634765625f, 0.765625f}, GLfloat2{0.705078125f, 0.841796875f});
        } else if (i == 26) {
            m_caps = !m_caps;
        } else {   // OK
            if (m_nameLength > 0) {
                m_editing = false;
                m_pressed = 29;
                m_keys[29].SetUV(GLfloat2{0.560546875f, 0.6875f}, GLfloat2{0.705078125f, 0.763671875f});
            }
            click = m_nameLength >= 1;
        }
        if (click) g->QueueSound(SOUND_CLICK);
        m_blink = 0.0f;
        return;
    }
}

// ================================================================ port-only: physical keyboard

bool NameScreen::AcceptsTyping() const {
    return m_active && !m_closing && m_editing && !m_classScreen.m_active && !m_modeScreen.m_active &&
           !m_game->m_helpMenu.m_active;
}

// As the letter and space keys above.
void NameScreen::TypeCharacter(char c) {
    if (!AcceptsTyping()) return;
    const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    if (letter) {
        if (m_nameLength >= 0x10) return;
        if (m_caps && !IsUpper(c)) c = static_cast<char>(c - 'a' + 'A');
        m_name[m_nameLength++] = c;
        m_caps = false;
    } else if (c == ' ') {
        if (m_nameLength == 0 || m_nameLength >= 0x10) return;
        m_name[m_nameLength++] = ' ';
        m_caps = false;
    } else {
        return;
    }
    m_game->QueueSound(SOUND_CLICK);
    m_blink = 0.0f;
}

// As the delete key above (one character; the host's key repeat replaces the hold-to-repeat).
void NameScreen::TypeBackspace() {
    if (!AcceptsTyping() || m_nameLength < 1) return;
    int n = m_nameLength;
    m_nameLength = n - 1;
    m_caps = n - 1 == 0 ? true : IsUpper(m_name[n - 1]);
    m_game->QueueSound(SOUND_CLICK);
    m_blink = 0.0f;
}

// As the OK key above.
void NameScreen::TypeEnter() {
    if (!AcceptsTyping() || m_nameLength < 1) return;
    m_editing = false;
    m_game->QueueSound(SOUND_CLICK);
    m_blink = 0.0f;
}

void NameScreen::Render() {
    if (m_classScreen.m_active && !(m_classScreen.m_fade < 1.0f)) {
        if (m_pressed == 0x1f) {
            ReleaseColour(m_editButton, m_editLabel);
            m_pressed = -1;
        }
        m_classScreen.Render();
        return;
    }
    if (m_modeScreen.m_active && !(m_modeScreen.m_fadeTime < 1.0f)) {
        if (m_pressed == 0x22) {
            ReleaseColour(m_modeHelpButton, m_modeHelpLabel);
            m_pressed = -1;
        }
        m_modeScreen.Render();
        return;
    }
    sdlDisable(GL_TEXTURE_2D);
    m_background.Render();
    m_panelBackground.Render();
    sdlEnable(GL_TEXTURE_2D);
    m_begin.Render();
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_backButton.Render();
    m_namePanel.Render();
    m_nameField.Render();
    m_randomButton.Render();
    m_difficultyPanel.Render();
    if (!m_challenge) {
        for (QuadStrip& b : m_difficultyButtons) b.Render();
    } else {
        m_modeHelpButton.Render();
        m_modeButtons[0].Render();
        m_modeIcons[0].Render();
        m_modeButtons[1].Render();
        m_modeIcons[1].Render();
    }
    m_classPanel.Render();
    m_classHelpButton.Render();
    for (int i = 0; i < 10; ++i) {
        m_classButtons[i].Render();
        m_classIcons[i].Render();
    }
    m_propertiesPanel.Render();
    m_editButton.Render();
    m_propertySlots[0].Render();
    for (int i = 0; i < 10; ++i) {
        m_propertyIcons[i].Render();
        m_propertySlots[i + 1].Render();
    }
    sdlBindTexture(GL_TEXTURE_2D, 1);
    m_propertyIcons[10].Render();
    sdlBindTexture(GL_TEXTURE_2D, 4);
    for (Label& l : m_classLevels) l.Render();
    m_nameHeading.Render();
    m_nameLabel.Render();
    m_randomLabel.Render();
    if (!m_challenge) {
        m_difficultyHeading.Render();
        for (Label& l : m_difficultyLabels) l.Render();
    } else {
        m_modeHeading.Render();
        m_modeHelpLabel.Render();
    }
    m_classHeading.Render();
    m_classHelpLabel.Render();
    m_propertiesHeading.Render();
    m_editLabel.Render();
    m_backLabel.Render();
    sdlBindTexture(GL_TEXTURE_2D, 4);
    sdlPushMatrix();
    sdlTranslatef(0.0f, Interpolate(0.853125f, 1.6f, m_keyboardT), 0.0f);
    sdlDisable(GL_TEXTURE_2D);
    m_keyboardBackground.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    for (Quad& k : m_keys) k.Render();
    for (Label* l : m_keyLabels) l->Render();
    sdlPopMatrix();
    if (m_classScreen.m_active) {
        m_classScreen.Render();
        return;
    }
    if (!m_modeScreen.m_active) return;
    m_modeScreen.Render();
}
