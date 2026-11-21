#include "score_screen.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "dr_gl.h"
#include "font.h"
#include "game.h"
#include "mode_screen.h"
#include "upgrade.h"
#include "util.h"

namespace {
const GLcolour4 kPanelColour{0x96, 0x96, 0xff, 0xff};
const GLcolour4 kHeading{0x7a, 0x67, 0xd3, 0xff};
const float kPx = 1.0f / 1024.0f;
const float kColumns[3] = {0.4f, 0.59000003f, 0.78f};   // right edges of the XP, gold, upgrades columns

// Port: the entries' dates. Each score file ends with "DT" and one 4-byte Unix time per entry
// (after the original's data, so an older reader ignores it; a file without it gives no dates).
const uint8_t kDatesMarker[2] = {'D', 'T'};
const float kDateScale = 0.07f;
const GLcolour4 kDateColour{0xb4, 0xb4, 0xd2, 0xff};

char s_number[32];   // 0x00089608
}  // namespace

ScorePage::ScorePage() = default;

ScoreScreen::ScoreScreen(Game* game) : m_game(game) {
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
    m_banner.SetSize(GLfloat2{0.134375f, 0.753125f});
    m_banner.m_position = GLfloat2{0.5f, 0.0799999982f};   // 0x3da3d70a
    m_banner.SetColour(kHeading);
    m_banner.SetUV(GLfloat2{980 * kPx, 677 * kPx}, GLfloat2{1023 * kPx, 918 * kPx});
    m_banner.m_rotation = -90.0f;
    for (int p = 0; p < 3; ++p) CreatePage(p);
}

ScoreScreen::~ScoreScreen() {
    for (ScorePage* p : m_pages) delete p;
}

int ScoreScreen::CountWord(int k) const {
    if (k == 0) return m_entries[kTypes - 1][kEntries - 1].skills[3];
    if (k <= kTypes) return m_counts[k - 1];
    return m_lastType;
}

// One page: ten rows (a rank panel and a row panel with four skill icons), the previous / next
// buttons with their arrows and the title panel between them above the rows.
void ScoreScreen::CreatePage(int index) {
    ScorePage* page = new ScorePage();
    m_pages[index] = page;
    const float margin = 0.009375f;
    for (int r = 0; r < kEntries; ++r) {
        float y = ((m_backButton.m_position.y - margin) - margin) + 0.00625f;
        y = y + static_cast<float>(10 - r) * -0.106250003f;
        ModeScreen::MakePanel(page->m_rankPanels[r], false, 0.1f, 0.1f, margin, y);
        ModeScreen::MakePanel(page->m_rowPanels[r], true, 0.87499994f, 0.1f, 0.115625f, y, 0.0375f);
        for (int k = 0; k < 4; ++k) {
            GLfloat2 tl, br;
            Upgrade::GetIconUVs(0, 0, 0x400, 0x200, tl, br);
            Quad& q = page->m_skillIcons[r][k];
            q.SetUV(tl, br);
            q.m_scale = GLfloat2{0.05f, 0.05f};
            float rowX = page->m_rowPanels[r].m_position.x;
            GLfloat2 size = page->m_rowPanels[r].GetSize();
            float x = rowX + size.x;
            x = x + static_cast<float>(k) * -0.05f;
            q.m_position.y = (page->m_rowPanels[r].m_position.y + 0.025f) + 0.003125f;
            q.m_position.x = (x - 0.025f) - 0.00625f;
        }
    }
    float y0 = (page->m_rowPanels[0].m_position.y - 0.1f) - 0.0718749985f;
    ModeScreen::MakeButton(page->m_prevButton, 0.1f, 0.1f, margin, y0);
    GLfloat2 size = page->m_prevButton.GetSize();
    ModeScreen::MakePanel(page->m_titlePanel, false, 0.76874995f, 0.1f, (page->m_prevButton.m_position.x + size.x) + 0.00625f, y0);
    size = page->m_titlePanel.GetSize();
    ModeScreen::MakeButton(page->m_nextButton, 0.1f, 0.1f, (page->m_titlePanel.m_position.x + size.x) + 0.00625f, y0);
    page->m_prevArrow.AddQuad(GLfloat2{302.0f, 1.0f}, GLfloat2{329.0f, 30.0f}, 2, -1.0f);
    page->m_prevArrow.AlignWith(page->m_prevButton);
    page->m_prevArrow.Mirror();
    page->m_prevArrow.SetColour(kHeading.r, kHeading.g, kHeading.b, kHeading.a);
    page->m_nextArrow.AddQuad(GLfloat2{302.0f, 1.0f}, GLfloat2{329.0f, 30.0f}, 2, -1.0f);
    page->m_nextArrow.AlignWith(page->m_nextButton);
    page->m_nextArrow.SetColour(kHeading.r, kHeading.g, kHeading.b, kHeading.a);
    Label& title = page->m_title;
    title.m_font = &m_game->m_fonts[1];
    title.m_scale = GLfloat2{0.0712499991f, 0.0712499991f};
    title.m_colour = kHeading;
    title.m_spacing = 2.0f;
    title.UpdateColour();
    size = page->m_titlePanel.GetSize();
    title.m_position.y = (page->m_titlePanel.m_position.y + size.y) - 0.03125f;
}

// The column headers (only "Turns" and "Score" get their text here) and the rows' labels.
void ScoreScreen::Initialise() {
    for (int p = 0; p < 3; ++p) {
        ScorePage* page = m_pages[p];
        for (int h = 0; h < 5; ++h) {
            Label& l = page->m_headers[h];
            l.m_scale = GLfloat2{0.0593749993f, 0.0593749993f};
            l.m_spacing = 2.0f;
            l.m_font = &m_game->m_fonts[1];
            l.m_colour = kHeading;
            l.m_position.y = page->m_rowPanels[0].m_position.y - 0.015625f;
            l.UpdateColour();
            if (h == 0) {
                l.SetText("Turns", -1);
                l.m_position.x = page->m_rowPanels[0].m_position.x;
            } else if (h == 4) {
                l.SetText("Score", -1);
                // (the original takes row panel p's x and size here, p being the page index)
                float x = page->m_rowPanels[p].m_position.x;
                GLfloat2 size = page->m_rowPanels[p].GetSize();
                l.m_position.x = ((x + size.x) - 0.00625f) - l.m_width * l.m_scale.x;
            }
        }
        for (int r = 0; r < kEntries; ++r) {
            Label* row = page->m_rowLabels[r];
            row[0].m_font = &m_game->m_fonts[0];
            row[0].m_spacing = 2.0f;
            // the rank number
            row[0].m_scale = GLfloat2{0.131249994f, 0.131249994f};
            row[0].m_font = &m_game->m_fonts[2];
            char text[32];
            std::snprintf(text, sizeof(text), "%d", r + 1);
            row[0].SetText(text, -1);
            float x = page->m_rankPanels[r].m_position.x;
            GLfloat2 size = page->m_rankPanels[r].GetSize();
            x = x + size.x * 0.5f;
            x = x + (row[0].m_width * row[0].m_scale.x) * -0.5f;
            row[0].m_position.x = x - 0.003125f;
            float y = page->m_rankPanels[r].m_position.y;
            size = page->m_rankPanels[r].GetSize();
            row[0].m_position.y = (y + size.y) - 0.0281249993f;
            for (int k = 1; k < 7; ++k) {
                row[k].m_spacing = 2.0f;
                row[k].m_font = &m_game->m_fonts[0];
                if (k == 1) {   // the name
                    row[k].m_scale = GLfloat2{0.1f, 0.1f};
                    float rx = page->m_rowPanels[r].m_position.x;
                    row[k].m_position.y = (page->m_rowPanels[r].m_position.y + 0.053125f) - 0.00625f;
                    row[k].m_position.x = rx + 0.0125f;
                    Label& date = page->m_dates[r];   // port
                    date.m_font = &m_game->m_fonts[0];
                    date.m_spacing = 2.0f;
                    date.m_scale = GLfloat2{kDateScale, kDateScale};
                    date.m_colour = kDateColour;
                    date.m_position.y = row[k].m_position.y;
                } else {
                    row[k].m_scale = GLfloat2{0.09f, 0.09f};
                    float ry = page->m_rowPanels[r].m_position.y;
                    GLfloat2 rs = page->m_rowPanels[r].GetSize();
                    row[k].m_position.y = (ry + rs.y) - 0.00937500037f;
                }
            }
        }
    }
}

// Page `index` shows table m_type - 1 + index (if there is one): title, the buttons dimmed at the
// ends, the rows (the newest entry yellow), the skills' icons.
void ScoreScreen::InitialisePage(int index) {
    int u = m_type - 1 + index;
    if (static_cast<unsigned>(u) >= kTypes) return;
    ScorePage* page = m_pages[index];
    page->m_title.SetText(u < 4 ? Game::GetDifficultyName(u) : Game::GetModeName(u - 4), -1);
    float x = page->m_titlePanel.m_position.x;
    GLfloat2 size = page->m_titlePanel.GetSize();
    x = x + size.x * 0.5f;
    page->m_title.m_position.x = x + (page->m_title.m_width * page->m_title.m_scale.x) * -0.5f;
    const GLcolour4 dim = DivideColour(kPanelColour, 2.0f);
    if (u == 0) {
        page->m_prevButton.SetColour(dim);
        page->m_nextButton.SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
    } else {
        page->m_prevButton.SetColour(kPanelColour.r, kPanelColour.g, kPanelColour.b, kPanelColour.a);
        const GLcolour4 next = u == 5 ? dim : kPanelColour;
        page->m_nextButton.SetColour(next.r, next.g, next.b, next.a);
    }
    int count = m_counts[u];
    for (int r = 0; r < kEntries; ++r) page->m_dates[r].SetText("", -1);   // port: filled below
    for (int r = 0; r < kEntries; ++r) {
        uint8_t a = r < count ? 0xff : 0x80;
        page->m_rankPanels[r].SetAlpha(a);
        page->m_rowPanels[r].SetAlpha(a);
        Label* headers = page->m_headers;
        if (u == 5) {
            SetLabelText(headers[1], "Avg", kColumns[0]);
            SetLabelText(headers[2], "Max", kColumns[1]);
            SetLabelText(headers[3], "Total", kColumns[2]);
        } else {
            SetLabelText(headers[1], "XP", kColumns[0]);
            SetLabelText(headers[2], "GP", kColumns[1]);
            SetLabelText(headers[3], "UP", kColumns[2]);
        }
        if (r >= count) continue;
        const Entry& e = m_entries[u][r];
        Label* row = page->m_rowLabels[r];
        row[1].SetText(e.name, -1);
        SetLabelNumber(row[2], e.turn, row[1].m_position.x, true, 0);
        SetLabelNumber(row[3], e.a, kColumns[0], false, u == 5 ? 10 : 0);
        SetLabelNumber(row[4], e.b, kColumns[1], false, 0);
        SetLabelNumber(row[5], e.c, kColumns[2], false, 0);
        float rx = page->m_rowPanels[r].m_position.x;
        GLfloat2 rs = page->m_rowPanels[r].GetSize();
        SetLabelNumber(row[6], e.score, (rx + rs.x) - 0.0125f, false, 0);
        // port: the date, right-aligned before the skill icons; left out if the name reaches it
        if (e.time != 0) {
            const std::time_t when = static_cast<std::time_t>(e.time);
            char text[16] = "";
            if (const std::tm* lt = std::localtime(&when)) std::strftime(text, sizeof text, "%d.%m.%y", lt);
            Label& date = page->m_dates[r];
            date.SetText(text, -1);
            const float iconsLeft = (rx + rs.x) - 4 * 0.05f - 0.00625f;
            date.m_position.x = iconsLeft - 0.0125f - date.m_width * kDateScale;
            if (row[1].m_position.x + row[1].m_width * row[1].m_scale.x + 0.02f > date.m_position.x) date.SetText("", -1);
        }
        bool newest = m_lastType == u && m_lastIndex == r;
        for (int k = 0; k < 7; ++k) {
            row[k].m_colour = newest ? GLcolour4{0xff, 0xff, 0, 0xff} : GLcolour4{0xff, 0xff, 0xff, 0xff};
            row[k].UpdateColour();
        }
        for (int k = 0; k < 4; ++k) {
            GLfloat2 tl{0.0f, 0.0f}, br{0.0f, 0.0f};
            int s = e.skills[k];
            if (s != -1) Upgrade::GetIconUVs(s % 10, s / 10, 0x400, 0x200, tl, br);
            page->m_skillIcons[r][k].SetUV(tl, br);
        }
    }
}

void ScoreScreen::Show(bool instant) {
    Game* g = m_game;
    int page = m_type;
    g->m_scorePageChanged = g->m_scorePage != ClampI(page, 0, 5);
    g->m_scorePage = ClampI(page, 0, 5);
    m_closing = false;
    m_backPressed = false;
    m_offset = 0.0f;
    m_swiping = false;
    m_unused180 = 0;
    m_sliding = false;
    m_active = true;
    m_lastTouchState = TOUCHING;
    for (int p = 0; p < 3; ++p) InitialisePage(p);
    if (!instant) {
        m_fadeTime = 0.3f;
        SetAlpha(0);
    } else {
        m_fadeTime = 0.0f;
        SetAlpha(0xff);
    }
}

void ScoreScreen::Show(int type, bool instant) {
    m_type = type;
    Show(instant);
}

void ScoreScreen::ShowMode(int mode, bool instant) {
    if (mode == MODE_SPRINT) {
        m_type = 4;
    } else if (mode == MODE_PRETZEL) {
        m_type = 5;
    }
    Show(instant);
}

void ScoreScreen::ShowDifficulty(int difficulty, bool instant) {
    if (difficulty >= 0 && difficulty <= 3) m_type = difficulty;
    Show(instant);
}

void ScoreScreen::FadeOut() {
    m_closing = true;
    m_fadeTime = 0.3f;
}

void ScoreScreen::SetAlpha(uint8_t a) {
    m_background.SetAlpha(a);
    m_panelBackground.SetAlpha(a);
    m_banner.SetAlpha(a);
    m_backButton.SetAlpha(a);
    for (int p = 0; p < 3; ++p) {
        ScorePage* page = m_pages[p];
        int count = CountWord(m_type + p);
        uint8_t half = static_cast<uint8_t>(a >> 1);
        for (int r = 0; r < kEntries; ++r) {
            page->m_rankPanels[r].SetAlpha(r < count ? a : half);
            page->m_rowPanels[r].SetAlpha(r < count ? a : half);
            for (int k = 0; k < 7; ++k) {
                page->m_rowLabels[r][k].m_colour.a = a;
                page->m_rowLabels[r][k].UpdateColour();
            }
            for (int k = 0; k < 4; ++k) page->m_skillIcons[r][k].SetAlpha(a);
            page->m_dates[r].m_colour.a = a;   // port
            page->m_dates[r].UpdateColour();
        }
        page->m_prevButton.SetAlpha(a);
        page->m_prevArrow.SetAlpha(a);
        page->m_titlePanel.SetAlpha(a);
        page->m_nextButton.SetAlpha(a);
        page->m_nextArrow.SetAlpha(a);
        page->m_title.m_colour.a = a;
        page->m_title.UpdateColour();
        for (int h = 0; h < 5; ++h) {
            page->m_headers[h].m_colour.a = a;
            page->m_headers[h].UpdateColour();
        }
    }
}

// Fades in/out (0.3 s); a swipe over the rows or the arrow buttons slides to the neighbouring
// table; back returns to the main menu.
void ScoreScreen::Process(float dt) {
    Game* g = m_game;
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
    if (m_sliding) {
        float dir = static_cast<float>(m_slideDirection);
        float offset = Interpolate(dir, m_offset, 0.3f);
        m_offset = offset;
        if (!(std::fabs(offset - dir) < 0.00625f)) return;
        m_offset = 0.0f;
        m_sliding = false;
        if (dir > 0.5f) {   // to the previous table
            ScorePage* p2 = m_pages[2];
            m_pages[2] = m_pages[1];
            ScorePage* p0 = m_pages[0];
            m_pages[0] = p2;
            m_pages[1] = p0;
            m_type = ClampI(m_type - 1, 0, 5);
            InitialisePage(0);
        } else if (dir < -0.5f) {   // to the next
            ScorePage* p0 = m_pages[0];
            m_pages[0] = m_pages[1];
            ScorePage* p2 = m_pages[2];
            m_pages[2] = p0;
            m_pages[1] = p2;
            m_type = ClampI(m_type + 1, 0, 5);
            InitialisePage(2);
        }
        g->m_scorePageChanged = g->m_scorePage != ClampI(m_type, 0, 5);
        g->m_scorePage = ClampI(m_type, 0, 5);
        return;
    }
    GLfloat2 p{g->m_touchX, g->m_touchY};
    int touch = g->m_touchState;
    if (touch == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
        m_lastTouchState = TOUCHING;
        ScorePage* first = m_pages[0];
        float ty = g->m_touchY;
        if (first->m_rowPanels[0].m_position.y < ty) {
            float y9 = first->m_rowPanels[9].m_position.y;
            GLfloat2 size = first->m_rowPanels[9].GetSize();
            if (ty < y9 + size.y) {   // a swipe over the rows begins
                m_swiping = true;
                m_velocity = 0.0f;
                m_startX = g->m_touchX;
                m_lastX = g->m_touchX;
            }
        }
        QuadStrip* pressed;
        if (m_backButton.ContainsPoint(p)) {
            m_backPressed = true;
            pressed = &m_backButton;
        } else {
            if (m_type > 0) {
                if (m_pages[1]->m_prevButton.ContainsPoint(p)) {
                    m_prevPressed = true;
                    m_pages[1]->m_prevButton.SetAlpha(0x96);
                    g->QueueSound(SOUND_CLICK);
                    return;
                }
                if (m_type > 4) return;
            }
            if (!m_pages[1]->m_nextButton.ContainsPoint(p)) return;
            m_nextPressed = true;
            pressed = &m_pages[1]->m_nextButton;
        }
        pressed->SetAlpha(0x96);
        g->QueueSound(SOUND_CLICK);
        return;
    }
    if (m_lastTouchState == TOUCHING && touch == NOT_TOUCHING) {
        m_lastTouchState = NOT_TOUCHING;
        if (!m_swiping) {
            if (m_backPressed) {
                if (m_backButton.ContainsPoint(p)) {
                    FadeOut();
                    g->m_mainMenu.Show();
                    g->m_mainMenu.SetActive(true);
                }
                m_backButton.SetAlpha(0xff);
                m_backPressed = false;
                return;
            }
            if (m_prevPressed) {
                if (m_pages[1]->m_prevButton.ContainsPoint(p)) {
                    m_sliding = true;
                    m_slideDirection = 1;
                }
                m_pages[1]->m_prevButton.SetAlpha(0xff);
                m_prevPressed = false;
                return;
            }
            if (!m_nextPressed) return;
            if (m_pages[1]->m_nextButton.ContainsPoint(p)) {
                m_sliding = true;
                m_slideDirection = -1;
            }
            m_pages[1]->m_nextButton.SetAlpha(0xff);
            m_nextPressed = false;
            return;
        }
        // the swipe ends: slide by its speed or distance (not past the first / last table)
        m_swiping = false;
        m_sliding = true;
        float v = m_velocity;
        if (m_type >= 1) {
            if (v > 0.1f || m_offset > 0.5f) {
                m_slideDirection = 1;
                return;
            }
            if (m_type > 4) {
                m_slideDirection = 0;
                return;
            }
        }
        if (v < -0.1f || m_offset < -0.5f) {
            m_slideDirection = -1;
            return;
        }
        m_slideDirection = 0;
        return;
    }
    if (!m_swiping) return;
    float tx = g->m_touchX;
    float offset = tx - m_startX;
    m_offset = offset;
    float last;
    if (m_type == 0) {
        if (offset > 0.0f) {   // no table before the first
            m_startX = tx;
            m_offset = 0.0f;
            m_lastX = tx;
        }
        last = m_lastX;
    } else if (m_type == 5 && offset < 0.0f) {   // nor after the last
        m_startX = tx;
        m_offset = 0.0f;
        m_lastX = tx;
        last = tx;
    } else {
        last = m_lastX;
    }
    m_velocity = Interpolate((g->m_touchX - last) / dt, m_velocity, 0.4f);
    m_lastX = g->m_touchX;
}

void ScoreScreen::Render() {
    sdlDisable(GL_TEXTURE_2D);
    m_background.Render();
    m_panelBackground.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 1);
    m_banner.Render();
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_backButton.Render();
    sdlPushMatrix();
    sdlTranslatef(m_offset, 0.0f, 0.0f);
    RenderPage(1);
    sdlPopMatrix();
    if (m_offset > 0.009375f) {
        sdlPushMatrix();
        sdlTranslatef(m_offset - 1.0f, 0.0f, 0.0f);
        RenderPage(0);
        sdlPopMatrix();
    } else if (m_offset < -0.009375f) {
        sdlPushMatrix();
        sdlTranslatef(m_offset + 1.0f, 0.0f, 0.0f);
        RenderPage(2);
        sdlPopMatrix();
    }
}

void ScoreScreen::RenderPage(int index) {
    ScorePage* page = m_pages[index];
    sdlBindTexture(GL_TEXTURE_2D, 4);
    for (int r = 0; r < kEntries; ++r) {
        page->m_rankPanels[r].Render();
        page->m_rowPanels[r].Render();
    }
    page->m_prevButton.Render();
    page->m_titlePanel.Render();
    page->m_nextButton.Render();
    int t = index + m_type;
    if (t >= 2) page->m_prevArrow.Render();
    if (t < 6) page->m_nextArrow.Render();
    page->m_title.Render();
    int count = CountWord(t);
    for (int r = 0; r < kEntries; ++r) {
        if (r >= count) continue;
        for (int k = 0; k < 7; ++k) page->m_rowLabels[r][k].Render();
        page->m_dates[r].Render();   // port
    }
    sdlBindTexture(GL_TEXTURE_2D, 1);
    for (int r = 0; r < kEntries; ++r) {
        if (r >= count) continue;
        for (int k = 0; k < 4; ++k) page->m_skillIcons[r][k].Render();
    }
    for (int h = 0; h < 5; ++h) page->m_headers[h].Render();
}

void ScoreScreen::AddScore(const char* name, int turn, int a, int b, int c, int score, int type) {
    if (m_lastType >= 0 && m_lastType != type) m_changed[m_lastType] = true;
    m_lastType = type;
    m_changed[type] = true;
    m_lastIndex = 0;
    if (m_counts[type] >= 1 && m_entries[type][0].score >= score) {
        int i = 1;
        for (;;) {
            if (m_counts[type] <= i) break;
            if (!(score <= m_entries[type][i].score)) break;
            ++i;
        }
        m_lastIndex = i;
        if (i == 10) m_lastIndex = 9;
    }
    int n = m_counts[type];
    if (n > 8) n = 9;
    for (int j = n; j > m_lastIndex; --j) m_entries[type][j] = m_entries[type][j - 1];
    Entry& e = m_entries[type][m_lastIndex];
    std::strcpy(e.name, name);
    e.turn = turn;
    e.a = a;
    e.b = b;
    e.c = c;
    e.score = score;
    for (int& s : e.skills) s = -1;
    e.time = GetUnixTime();   // port
    int count = m_counts[type] + 1;
    if (count > 9) count = 10;
    m_counts[type] = count;
}

void ScoreScreen::SetSkills(int s0, int s1, int s2, int s3) {
    if (m_lastType < 0 || m_lastIndex < 0) return;   // (the original writes before the tables)
    int* s = m_entries[m_lastType][m_lastIndex].skills;
    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;
}

int ScoreScreen::GetScoreCount() const {
    int n = 0;
    for (int c : m_counts) n += c;
    return n;
}

bool ScoreScreen::HaveScoresChanged() const {
    for (bool c : m_changed) {
        if (c) return true;
    }
    return false;
}

void ScoreScreen::SerialiseState(int type) {
    m_state.ResetForWriting();
    m_state.WriteChar(static_cast<uint8_t>(m_counts[type]));
    m_state.WriteChar(type == m_lastType ? static_cast<uint8_t>(m_lastIndex) : 0xff);
    for (int i = 0; i < m_counts[type]; ++i) {
        const Entry& e = m_entries[type][i];
        m_state.WriteInteger(e.turn);
        m_state.WriteInteger(e.a);
        m_state.WriteInteger(e.b);
        m_state.WriteInteger(e.c);
        m_state.WriteInteger(e.score);
        for (int s : e.skills) m_state.WriteChar(static_cast<uint8_t>(s));
        for (int k = 0; k < 0x11; ++k) {
            m_state.WriteChar(static_cast<uint8_t>(e.name[k]));
            if (e.name[k] == '\0') break;
        }
    }
    m_state.WriteChar(kDatesMarker[0]);   // port: the dates
    m_state.WriteChar(kDatesMarker[1]);
    for (int i = 0; i < m_counts[type]; ++i) m_state.WriteInteger(static_cast<int>(m_entries[type][i].time));
    m_changed[type] = false;
    m_serialisedType = type;
}

void ScoreScreen::SerialiseChangedState() {
    for (int t = 0; t < kTypes; ++t) {
        if (m_changed[t]) {
            SerialiseState(t);
            return;
        }
    }
}

void ScoreScreen::DeserialiseState(int version, int type) {
    m_state.ResetForReading();
    m_counts[type] = ClampI(m_state.ReadChar(), 0, 10);
    int last = m_state.ReadChar();
    if (last != 0xff) {
        m_lastIndex = last;
        m_lastType = type;
    }
    for (int i = 0; i < m_counts[type]; ++i) {
        Entry& e = m_entries[type][i];
        e.turn = m_state.ReadInteger();
        e.a = m_state.ReadInteger();
        e.b = m_state.ReadInteger();
        e.c = m_state.ReadInteger();
        e.score = m_state.ReadInteger();
        for (int& s : e.skills) {
            int v = m_state.ReadChar();
            s = v == 0xff ? -1 : ClampI(v, 0, 0x29);
        }
        for (int k = 0; k < 0x11; ++k) {
            int v = m_state.ReadChar();
            e.name[k] = static_cast<char>(v);
            if (v == 0) break;
        }
    }
    // port: the dates, if the file has them (`version` is the file's length)
    const int at = m_state.GetReadPosition();
    const bool dated = version - at >= 2 + 4 * m_counts[type] && m_state.GetChar(at) == kDatesMarker[0] &&
                       m_state.GetChar(at + 1) == kDatesMarker[1];
    if (dated) {
        m_state.ReadChar();
        m_state.ReadChar();
    }
    for (int i = 0; i < m_counts[type]; ++i) {
        m_entries[type][i].time = dated ? static_cast<uint32_t>(m_state.ReadInteger()) : 0;
    }
    m_changed[type] = false;
}

// The old single-table format: entries with their difficulty, re-added in order.
void ScoreScreen::DeserialiseLegacyState() {
    int count = m_state.ReadChar();
    int last = m_state.ReadChar();
    int lastType = -1;
    int lastIndex = -1;
    for (int i = 0; i < count; ++i) {
        int turn = m_state.ReadInteger();
        int a = m_state.ReadInteger();
        int b = m_state.ReadInteger();
        int c = m_state.ReadInteger();
        int score = m_state.ReadInteger();
        for (int k = 0; k < 5; ++k) m_state.ReadChar();
        int skills[4];
        for (int& s : skills) {
            int v = m_state.ReadChar();
            s = v == 0xff ? -1 : v;
        }
        int difficulty = m_state.ReadChar();
        m_state.ReadChar();
        char name[20] = {};
        for (int k = 0; k < 20; ++k) {
            int v = m_state.ReadChar();
            name[k] = static_cast<char>(v);
            if (v == 0) break;
        }
        AddScore(name, turn, a, b, c, score, difficulty);
        SetSkills(skills[0], skills[1], skills[2], skills[3]);
        if (last == i) {
            lastIndex = m_counts[difficulty] - 1;
            lastType = difficulty;
        }
    }
    m_lastType = lastType;
    m_lastIndex = lastIndex;
    for (auto& table : m_entries) {   // port: the old format has no dates
        for (Entry& e : table) e.time = 0;
    }
}

void ScoreScreen::SetLabelText(Label& label, const char* text, float right) {
    label.SetText(text, -1);
    label.m_position.x = right - label.m_width * label.m_scale.x;
}

void ScoreScreen::SetLabelNumber(Label& label, int value, float x, bool left, int divisor) {
    if (divisor == 0) {
        std::snprintf(s_number, sizeof(s_number), "%d", value);
    } else {
        std::snprintf(s_number, sizeof(s_number), "%.1f",
                      static_cast<double>(static_cast<float>(value) / static_cast<float>(divisor)));
    }
    label.SetText(s_number, -1);
    float w = left ? 0.0f : label.m_width * label.m_scale.x;
    label.m_position.x = x - w;
}
