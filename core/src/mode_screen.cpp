#include "mode_screen.h"

#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "util.h"

namespace {
// The mode buttons' icons (texture 4 pixels; 0x000895b8 / 0x000895c8, set by a static initialiser)
const GLfloat2 kModeIconTopLeft[2] = {{399.0f, 378.0f}, {409.0f, 324.0f}};
const GLfloat2 kModeIconBottomRight[2] = {{511.0f, 431.0f}, {511.0f, 377.0f}};

// The modes' texts (0x00085768.. 64- and 128-byte rows)
const char* const kSprintDescription[4] = {"Shoot for the high score while", "trying to survive a quick 100 turn",
                                           "game with faster upgrades and", "tougher enemies!"};
const char* const kSprintScoring[4] = {"- 1 point for each coin collected", "- 1 point for each experience point",
                                       "- 1 point for each upgrade point", "- Double score if you survive 100 turns"};
const char* const kSprintRules[6] = {"- Game lasts 100 turns or until you die", "- A special enemy appears every 10 turns",
                                     "- XP, UP & gold earned at double rate", "- Skills start at level 5 and can't be upgraded",
                                     "- Wish skill never offered on first level up", "- Trap skill never offered on level up"};
const char* const kPretzelDescription[4] = {"Trace the most tangled paths to", "get the top scores! Choose",
                                            "upgrades wisely to compensate", "for your limited health!"};
const char* const kPretzelScoring[4] = {"- 1 point for each overlap in a traced path",
                                        "- Double points for 5 or more overlaps in a path",
                                        "- Triple points for 10 or more overlaps in a path",
                                        "- 100 points for 15 overlaps in a single path"};
const char* const kPretzelRules[6] = {"- You start with 100 health", "- Health cannot be upgraded",
                                      "- Teleport skill never offered on level up", "- Treasure Chamber skill unavailable", "",
                                      ""};

const GLcolour4 kPanelColour{0x96, 0x96, 0xff, 0xff};

}  // namespace

// GLcolour4::operator/(float) (0x000545e4): r, g, b divided (unsigned conversion), alpha kept
GLcolour4 DivideColour(const GLcolour4& c, float d) {
    return GLcolour4{static_cast<uint8_t>(FloatToUintArm(static_cast<float>(c.r) / d)),
                     static_cast<uint8_t>(FloatToUintArm(static_cast<float>(c.g) / d)),
                     static_cast<uint8_t>(FloatToUintArm(static_cast<float>(c.b) / d)), c.a};
}

namespace {
GLcolour4 Divide(const GLcolour4& c, float d) { return DivideColour(c, d); }

void Row3(QuadStrip& s, float u0, float u1, float u2, float u3, float v0, float v1, int edge, int middle, float h) {
    s.AddQuad(GLfloat2{u0, v0}, GLfloat2{u1, v1}, edge, h);
    s.AddQuad(GLfloat2{u1, v0}, GLfloat2{u2, v1}, middle, h);
    s.AddQuad(GLfloat2{u2, v0}, GLfloat2{u3, v1}, edge, h);
}
}  // namespace

ModeScreen::ModeScreen(Game* game) : m_game(game) {
    const float margin = 0.009375f;
    m_background.SetSize(GLfloat2{1.0f, 1.5f});
    m_background.m_position = GLfloat2{0.5f, 0.75f};
    m_background.SetColour(GLcolour4{0, 0, 0, 0xff});
    m_backButton.AddQuad(GLfloat2{104.0f, 1.0f}, GLfloat2{193.0f, 27.0f}, 2, -1.0f);
    GLfloat2 size = m_backButton.GetSize();
    m_backButton.SetPosition(GLfloat2{(1.0f - size.x) - margin, (1.5f - size.y) - margin});
    float h = m_backButton.m_position.y - margin;
    m_panelBackground.SetSize(GLfloat2{1.0f, h});
    m_panelBackground.m_position = GLfloat2{0.5f, h * 0.5f};
    m_panelBackground.SetColour(GLcolour4{45, 45, 75, 0xff});
    const float edge = 0.015625f;
    MakePanel(m_modePanel, true, 0.96875f, 0.26875f, edge, edge);
    for (int i = 0; i < 2; ++i) {
        MakeButton(m_modeButtons[i], 0.4609375f, 0.165625f,
                   (m_modePanel.m_position.x + edge) + static_cast<float>(i) * 0.4765625f,
                   (m_modePanel.m_position.y + 0.0718749985f) + edge);
        m_modeButtonIcons[i].AddQuad(kModeIconTopLeft[i], kModeIconBottomRight[i], 2, -1.0f);
        m_modeButtonIcons[i].AlignWith(m_modeButtons[i]);
    }
    size = m_modePanel.GetSize();
    MakePanel(m_descriptionPanel, false, 0.96875f, size.y, edge, (m_modePanel.m_position.y + size.y) + 0.03125f);
    m_modeIcon.AddQuad(GLfloat2{77.0f, 225.0f}, GLfloat2{152.0f, 301.0f}, 2, -1.0f);
    m_modeIcon.AlignWith(m_descriptionPanel);
    float iconY = m_modeIcon.m_position.y;
    m_modeIcon.SetPosition(GLfloat2{(iconY + m_descriptionPanel.m_position.x) - m_descriptionPanel.m_position.y, iconY});
    size = m_descriptionPanel.GetSize();
    MakePanel(m_scoringPanel, true, 0.96875f, 0.3125f, edge, (m_descriptionPanel.m_position.y + size.y) + 0.03125f);
    size = m_scoringPanel.GetSize();
    float scoringBottom = m_scoringPanel.m_position.y + size.y;
    float rest = h - scoringBottom;
    MakePanel(m_rulesPanel, true, 0.96875f, (rest - 0.03125f) - edge, edge, scoringBottom + 0.03125f);
}

void ModeScreen::Initialise() {
    MakeHeadingLabel(m_modePanel, m_modeHeading, "Challenge Mode");
    MakeHeadingLabel(m_scoringPanel, m_scoringHeading, "Scoring");
    MakeHeadingLabel(m_rulesPanel, m_rulesHeading, "Special Rules");
}

void ModeScreen::Show() {
    m_active = true;
    m_fadeTime = 0.3f;
    m_closing = false;
    m_lastTouchState = TOUCHING;
    m_backPressed = false;
    SetAlpha(0);
}

void ModeScreen::FadeOut() {
    m_closing = true;
    m_fadeTime = 0.3f;
}

// A 3 x 3 frame (5-pixel corners) stretched to w x h.
void ModeScreen::MakeButton(QuadStrip& s, float w, float h, float x, float y) {
    Row3(s, 194.0f, 199.0f, 242.0f, 247.0f, 1.0f, 6.0f, 2, 1, -1.0f);
    ++s.m_currentRow;
    Row3(s, 194.0f, 199.0f, 242.0f, 247.0f, 6.0f, 49.0f, 0, 3, -1.0f);
    ++s.m_currentRow;
    Row3(s, 194.0f, 199.0f, 242.0f, 247.0f, 49.0f, 54.0f, 2, 1, -1.0f);
    s.m_fixedSize = true;
    s.m_size = GLfloat2{w, h};
    s.UpdateSize();
    s.SetPosition(GLfloat2{x, y});
    s.SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
}

// A frame stretched to w x h; with a heading: a darker title bar (0.05625 high) and a thin line
// under it.
void ModeScreen::MakePanel(QuadStrip& s, bool heading, float w, float h, float x, float y, float headingHeight) {
    if (!heading) {
        Row3(s, 50.0f, 55.0f, 98.0f, 103.0f, 1.0f, 6.0f, 2, 1, -1.0f);
    } else {
        Row3(s, 194.0f, 199.0f, 242.0f, 247.0f, 1.0f, 6.0f, 2, 1, -1.0f);
        ++s.m_currentRow;
        Row3(s, 194.0f, 199.0f, 242.0f, 247.0f, 6.0f, 49.0f, 2, 1, headingHeight);
        ++s.m_currentRow;
        Row3(s, 194.0f, 199.0f, 242.0f, 247.0f, 6.0f, 49.0f, 2, 1, 0.003125f);
    }
    ++s.m_currentRow;
    Row3(s, 50.0f, 55.0f, 98.0f, 103.0f, 6.0f, 49.0f, 0, 3, -1.0f);
    ++s.m_currentRow;
    Row3(s, 50.0f, 55.0f, 98.0f, 103.0f, 49.0f, 54.0f, 2, 1, -1.0f);
    s.m_fixedSize = true;
    s.m_size = GLfloat2{w, h};
    s.UpdateSize();
    s.SetPosition(GLfloat2{x, y});
    s.SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
    if (heading) {
        const GLcolour4 bar{95, 95, 150, 0xff};
        for (int q = 0; q < 6; ++q) s.m_quads[q].SetColour(bar);
        for (int q = 6; q < 9; ++q) s.m_quads[q].SetColour(Divide(bar, 1.7f));
    }
}

void ModeScreen::MakeHeadingLabel(QuadStrip& panel, Label& label, const char* text) {
    label.m_scale = GLfloat2{0.059375f, 0.059375f};
    label.m_font = &m_game->m_fonts[1];
    label.m_spacing = 2.0f;
    label.SetText(text, -1);
    label.m_colour = GLcolour4{122, 103, 211, 0xff};
    label.UpdateColour();
    label.m_position.y = panel.m_position.y + 0.05625f;
    label.m_position.x = panel.m_position.x + 0.015625f;
}

// The next line of a panel (a new label when all are in use): below the heading, or (indent > 0)
// right of the mode icon.
void ModeScreen::AddLabel(const char* text, QuadStrip& panel, std::vector<Label>& labels, int& count, float indent) {
    if (count >= static_cast<int>(labels.size())) {
        labels.push_back(Label());
        Label& n = labels.back();
        n.m_spacing = 2.0f;
        n.m_font = &m_game->m_fonts[0];
        n.m_scale = GLfloat2{0.1f, 0.1f};
    }
    Label& l = labels[count];
    l.SetText(text, -1);
    l.m_position.x = (panel.m_position.x + 0.015625f) + indent;
    float extra = indent > 0.0f ? 0.015625f : 0.0718749985f;
    l.m_position.y = (panel.m_position.y + static_cast<float>(count + 1) * 0.054f) + extra;
    ++count;
}

void ModeScreen::ShowMode() {
    float iconX = m_modeIcon.m_position.x;
    m_descriptionCount = 0;
    m_scoringCount = 0;
    m_rulesCount = 0;
    float indent = iconX + m_modeIcon.GetSize().x;
    bool sprint = m_shownMode == 0;
    if (sprint) {
        m_modeIcon.SetUV(GLfloat2{77.0f, 225.0f}, GLfloat2{152.0f, 301.0f});
    } else {
        m_modeIcon.SetUV(GLfloat2{1.0f, 225.0f}, GLfloat2{76.0f, 301.0f});
    }
    const char* const* description = sprint ? kSprintDescription : kPretzelDescription;
    const char* const* scoring = sprint ? kSprintScoring : kPretzelScoring;
    const char* const* rules = sprint ? kSprintRules : kPretzelRules;
    for (int i = 0; i < 4; ++i) AddLabel(description[i], m_descriptionPanel, m_descriptionLabels, m_descriptionCount, indent);
    for (int i = 0; i < 4; ++i) AddLabel(scoring[i], m_scoringPanel, m_scoringLabels, m_scoringCount, 0.0f);
    for (int i = 0; i < 6; ++i) AddLabel(rules[i], m_rulesPanel, m_rulesLabels, m_rulesCount, 0.0f);
}

void ModeScreen::UpdateMode() {
    if (!m_active || m_shownMode == m_game->m_mode) return;
    m_shownMode = m_game->m_mode;
    uint8_t alpha = m_background.m_vertices[0].colour.a;
    ShowMode();
    for (int i = 0; i < 2; ++i) {
        if (m_shownMode == i) {
            m_modeButtons[i].SetColour(Divide(kPanelColour, 2.0f));
        } else {
            m_modeButtons[i].SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
        }
    }
    SetAlpha(alpha);
}

void ModeScreen::SetAlpha(uint8_t a) {
    m_background.SetAlpha(a);
    m_panelBackground.SetAlpha(a);
    m_backButton.SetAlpha(a);
    m_modePanel.SetAlpha(a);
    m_modeHeading.m_colour.a = a;
    m_modeHeading.UpdateColour();
    m_modeButtons[0].SetAlpha(a);
    m_modeButtonIcons[0].SetAlpha(a);
    m_modeButtons[1].SetAlpha(a);
    m_modeButtonIcons[1].SetAlpha(a);
    m_descriptionPanel.SetAlpha(a);
    m_modeIcon.SetAlpha(a);
    for (int i = 0; i < m_descriptionCount; ++i) {
        m_descriptionLabels[i].m_colour.a = a;
        m_descriptionLabels[i].UpdateColour();
    }
    m_scoringPanel.SetAlpha(a);
    m_scoringHeading.m_colour.a = a;
    m_scoringHeading.UpdateColour();
    for (int i = 0; i < m_scoringCount; ++i) {
        m_scoringLabels[i].m_colour.a = a;
        m_scoringLabels[i].UpdateColour();
    }
    m_rulesPanel.SetAlpha(a);
    m_rulesHeading.m_colour.a = a;
    m_rulesHeading.UpdateColour();
    for (int i = 0; i < m_rulesCount; ++i) {
        m_rulesLabels[i].m_colour.a = a;
        m_rulesLabels[i].UpdateColour();
    }
}

// Fades in/out (0.3 s); a press on a mode button chooses that mode, a press and release on the
// back button fades the screen out.
void ModeScreen::Process(float dt) {
    if (m_fadeTime > 0.0f) {
        float t = m_fadeTime - dt;
        m_fadeTime = t;
        float from = 0.0f;
        float to = 255.0f;
        if (t <= 0.0f) {
            m_fadeTime = 0.0f;
            t = 0.0f;
            if (m_closing) {
                m_active = false;
                return;
            }
        } else if (m_closing) {
            from = 255.0f;
            to = 0.0f;
        }
        SetAlpha(static_cast<uint8_t>(FloatToIntArm(Interpolate(from, to, t / 0.3f))));
        return;
    }
    Game* g = m_game;
    GLfloat2 p{g->m_touchX, g->m_touchY};
    if (g->m_touchState == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
        m_lastTouchState = TOUCHING;
        if (m_backButton.ContainsPoint(p)) {
            m_backPressed = true;
            m_backButton.SetAlpha(0x96);
            g->QueueSound(SOUND_CLICK);
            return;
        }
        int mode;
        if (m_modeButtons[0].ContainsPoint(p)) {
            mode = 0;
        } else if (m_modeButtons[1].ContainsPoint(p)) {
            mode = 1;
        } else {
            return;
        }
        g->QueueSound(SOUND_CLICK);
        g->m_modeChanged = static_cast<int>(g->m_mode) != ClampI(mode, 0, 1);
        g->m_mode = static_cast<GameMode>(ClampI(mode, 0, 1));
        return;
    }
    if (m_lastTouchState != TOUCHING || g->m_touchState != NOT_TOUCHING) return;
    m_lastTouchState = NOT_TOUCHING;
    if (!m_backPressed) return;
    if (m_backButton.ContainsPoint(p)) FadeOut();
    m_backButton.SetAlpha(0xff);
    m_backPressed = false;
}

void ModeScreen::Render() {
    sdlDisable(GL_TEXTURE_2D);
    m_background.Render();
    m_panelBackground.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_backButton.Render();
    m_modePanel.Render();
    m_modeButtons[0].Render();
    m_modeButtonIcons[0].Render();
    m_modeButtons[1].Render();
    m_modeButtonIcons[1].Render();
    m_descriptionPanel.Render();
    m_modeIcon.Render();
    m_scoringPanel.Render();
    m_rulesPanel.Render();
    m_modeHeading.Render();
    m_scoringHeading.Render();
    m_rulesHeading.Render();
    for (int i = 0; i < m_descriptionCount; ++i) m_descriptionLabels[i].Render();
    for (int i = 0; i < m_scoringCount; ++i) m_scoringLabels[i].Render();
    for (int i = 0; i < m_rulesCount; ++i) m_rulesLabels[i].Render();
}
