#include "achievements_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

#include "achievements.h"
#include "dr_gl.h"
#include "game.h"
#include "mode_screen.h"
#include "util.h"

namespace {
constexpr float kFade = 0.3f;              // seconds, as SettingsScreen
constexpr float kEdge = 0.015625f;         // screen units (the screen is 1 x 1.5)
constexpr float kMargin = 0.009375f;
constexpr float kHeading = 0.075f;         // the panel's title bar and its line
constexpr float kPanelWidth = 0.96875f;
constexpr float kInset = 0.03125f;         // the list inside the panel, left and right
constexpr float kBarWidth = 0.012f;        // the scroll bar, right of the rows
constexpr float kBarGap = 0.012f;
constexpr float kRowHeight = 0.165f;
constexpr float kRowGap = 0.02f;
constexpr float kTickSize = 0.06f;
constexpr float kTextLeft = 0.1f;          // the texts from the row's left
constexpr float kTextRight = 0.025f;
constexpr float kTitleScale = 0.085f;
constexpr float kSmallScale = 0.062f;
constexpr float kMinSmallScale = 0.048f;   // a long description shrinks down to this to fit two lines
constexpr float kCountScale = 0.08f;
constexpr float kWheelStep = kRowHeight + kRowGap;   // per wheel notch: one row
constexpr float kBarReach = 0.03f;         // a press this close to the bar grabs it
constexpr float kThumbReach = 0.02f;       // a finger: this close to the thumb itself
constexpr float kFlingFriction = 3.0f;     // per second: the fling's speed falls by e^-3 a second
constexpr float kFlingMin = 0.3f;          // screen units per second: slower releases do not fling
constexpr float kFlingStop = 0.05f;        // a fling this slow stops
const GLcolour4 kUnlockedRow{0x96, 0x96, 0xff, 0xff};   // SettingsScreen's row colour
const GLcolour4 kLockedRow{0x5a, 0x5a, 0x96, 0xff};
const GLcolour4 kUnlockedText{0xff, 0xff, 0xff, 0xff};
const GLcolour4 kLockedText{0xb4, 0xb4, 0xc8, 0xff};
const GLcolour4 kDescriptionText{0xdc, 0xdc, 0xf0, 0xff};
const GLcolour4 kStateText{0xff, 0xe8, 0xb0, 0xff};    // the title screen's version label colour
const GLcolour4 kBarTrack{30, 30, 55, 0xff};
const GLcolour4 kBarThumb{0xd2, 0xd2, 0xff, 0xff};
const float kPx = 1.0f / 1024.0f;                      // texture 1 pixels

GLcolour4 WithAlpha(GLcolour4 c, uint8_t a) {
    c.a = static_cast<uint8_t>(c.a * a / 0xff);
    return c;
}

float TextWidth(const Label& l) { return l.m_width * l.m_scale.x; }

// Splits `text` into lines no wider than `width` at `scale` (words kept whole); measured with `probe`.
std::vector<std::string> Wrap(Label probe, const std::string& text, float scale, float width) {
    probe.m_scale = GLfloat2{scale, scale};
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find(' ', i);
        if (j == std::string::npos) j = text.size();
        const std::string word = text.substr(i, j - i);
        const std::string candidate = line.empty() ? word : line + " " + word;
        probe.SetText(candidate.c_str(), -1);
        if (!line.empty() && TextWidth(probe) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        i = j + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
}  // namespace

AchievementsScreen::AchievementsScreen(Game* game) : m_game(game), m_heading(&game->m_fonts[1]), m_count(&game->m_fonts[0]) {
    m_background.SetSize(GLfloat2{1.0f, 1.5f});
    m_background.m_position = GLfloat2{0.5f, 0.75f};
    m_background.SetColour(GLcolour4{0, 0, 0, 0xff});
    // the Back button bottom right, the panel's background above it (as SettingsScreen)
    m_backButton.AddQuad(GLfloat2{104.0f, 1.0f}, GLfloat2{193.0f, 27.0f}, 2, -1.0f);
    const GLfloat2 size = m_backButton.GetSize();
    m_backButton.SetPosition(GLfloat2{(1.0f - size.x) - kMargin, (1.5f - size.y) - kMargin});
    const float h = m_backButton.m_position.y - kMargin;
    m_panelBackground.SetSize(GLfloat2{1.0f, h});
    m_panelBackground.m_position = GLfloat2{0.5f, h * 0.5f};
    m_panelBackground.SetColour(GLcolour4{45, 45, 75, 0xff});
    m_barTrack.SetColour(kBarTrack);
    m_barThumb.SetColour(kBarThumb);
}

Label AchievementsScreen::MakeLabel(float scale) const {
    Label l(&m_game->m_fonts[0]);
    l.m_spacing = 2.0f;
    l.m_scale = GLfloat2{scale, scale};
    return l;
}

void AchievementsScreen::Layout() {
    Game* g = m_game;
    const float panelHeight = m_backButton.m_position.y - kMargin - kEdge;
    m_panel = QuadStrip();
    ModeScreen::MakePanel(m_panel, true, kPanelWidth, panelHeight, kEdge, kEdge);
    g->m_nameScreen.m_modeScreen.MakeHeadingLabel(m_panel, m_heading, "Achievements");

    m_listLeft = m_panel.m_position.x + kInset;
    m_listRight = m_panel.m_position.x + kPanelWidth - kInset;
    m_listTop = m_panel.m_position.y + kHeading + kRowGap * 0.5f;
    m_listBottom = m_panel.m_position.y + panelHeight - kRowGap;
    const float rowWidth = (m_listRight - m_listLeft) - kBarWidth - kBarGap;
    const float textWidth = rowWidth - kTextLeft - kTextRight;

    m_rows.clear();
    m_rows.resize(static_cast<size_t>(kAchievementCount - 1));
    const Label probe = MakeLabel(kSmallScale);
    char text[32];
    // the order: the unlocked ones first, newest first (equal times in the list's order), then the
    // locked ones in the list's order
    std::vector<int> order;
    for (int a = 1; a < kAchievementCount; ++a) order.push_back(a);
    std::stable_sort(order.begin(), order.end(), [g](int x, int y) {
        const bool ux = g->IsAchievementUnlocked(x), uy = g->IsAchievementUnlocked(y);
        if (ux != uy) return ux;
        return ux && g->m_achievementRecords[x].time > g->m_achievementRecords[y].time;
    });
    for (size_t i = 0; i < order.size(); ++i) {
        const int a = order[i];
        Row& r = m_rows[i];
        r.achievement = a;
        const AchievementInfo& info = g_achievements[a];
        const Game::AchievementRecord& rec = g->m_achievementRecords[a];
        r.unlocked = g->IsAchievementUnlocked(a);
        r.top = static_cast<float>(i) * (kRowHeight + kRowGap) + kRowGap * 0.5f;
        const float x = m_listLeft;
        const float top = m_listTop + r.top;
        ModeScreen::MakeButton(r.button, rowWidth, kRowHeight, x, top);
        r.button.SetColour(r.unlocked ? kUnlockedRow : kLockedRow);
        r.tick.SetSize(GLfloat2{kTickSize, kTickSize});
        r.tick.m_position = GLfloat2{x + kTextLeft * 0.5f, top + kRowHeight * 0.5f};
        const float v = r.unlocked ? 394.0f : 411.0f;   // GameMenu's tick box: ticked / empty
        r.tick.SetUV(GLfloat2{405.0f * kPx, v * kPx}, GLfloat2{421.0f * kPx, (v + 16.0f) * kPx});

        // right of the title: the date (as the score pages write it) or the progress so far
        text[0] = '\0';
        if (r.unlocked) {
            const std::time_t when = static_cast<std::time_t>(rec.time);
            if (const std::tm* lt = std::localtime(&when)) std::strftime(text, sizeof text, "%d.%m.%y", lt);
        } else if (rec.progress > 0.0f) {
            std::snprintf(text, sizeof text, "%d%%", static_cast<int>(rec.progress));
        }
        r.state = MakeLabel(kSmallScale);
        r.state.SetText(text, -1);
        r.state.m_colour = kStateText;
        const float titleBaseline = top + 0.052f;
        r.state.m_position = GLfloat2{x + rowWidth - kTextRight - TextWidth(r.state), titleBaseline};

        // the title, shrunk if it would reach the date
        r.title = MakeLabel(kTitleScale);
        r.title.SetText(info.title, -1);
        const float titleRoom = textWidth - (text[0] ? TextWidth(r.state) + 0.03f : 0.0f);
        if (TextWidth(r.title) > titleRoom) {
            const float s = kTitleScale * titleRoom / TextWidth(r.title);
            r.title.m_scale = GLfloat2{s, s};
        }
        r.title.m_colour = r.unlocked ? kUnlockedText : kLockedText;
        r.title.m_position = GLfloat2{x + kTextLeft, titleBaseline};

        // the description in at most two lines (smaller if needed)
        float scale = kSmallScale;
        std::vector<std::string> lines = Wrap(probe, info.description, scale, textWidth);
        while (lines.size() > 2 && scale > kMinSmallScale) {
            scale -= 0.002f;
            lines = Wrap(probe, info.description, scale, textWidth);
        }
        for (int k = 0; k < 2; ++k) {
            Label& l = r.lines[k];
            l = MakeLabel(scale);
            std::string s = k < static_cast<int>(lines.size()) ? lines[static_cast<size_t>(k)] : std::string();
            if (k == 1) {   // anything past two lines joins the second
                for (size_t m = 2; m < lines.size(); ++m) s += " " + lines[m];
            }
            l.SetText(s.c_str(), -1);
            l.m_colour = r.unlocked ? kDescriptionText : kLockedText;
            l.m_position = GLfloat2{x + kTextLeft, top + 0.098f + static_cast<float>(k) * 0.045f};
        }
    }
    m_contentHeight = static_cast<float>(kAchievementCount - 1) * (kRowHeight + kRowGap) + kRowGap * 0.5f;
    m_maxScroll = m_contentHeight - (m_listBottom - m_listTop);
    if (m_maxScroll < 0.0f) m_maxScroll = 0.0f;

    std::snprintf(text, sizeof text, "%d / %d unlocked", g->UnlockedAchievementCount(), kAchievementCount - 1);
    m_count = MakeLabel(kCountScale);
    m_count.SetText(text, -1);
    m_count.m_colour = kUnlockedText;
    m_count.m_position = GLfloat2{kEdge + kInset, m_backButton.m_position.y + m_backButton.GetSize().y * 0.5f + kCountScale * 0.22f};

    m_barTrack.SetSize(GLfloat2{kBarWidth, m_listBottom - m_listTop});
    m_barTrack.m_position = GLfloat2{m_listRight - kBarWidth * 0.5f, (m_listTop + m_listBottom) * 0.5f};
    SetScroll(0.0f);
}

void AchievementsScreen::Show() {
    Layout();
    m_active = true;
    m_closing = false;
    m_fadeTime = kFade;
    m_lastTouchState = TOUCHING;   // the touch that opened the screen is not a press here
    m_backPressed = false;
    m_dragging = false;
    m_draggingBar = false;
    m_fling = 0.0f;
    SetAlpha(0);
}

void AchievementsScreen::FadeOut() {
    if (!m_active || m_closing) return;
    m_closing = true;
    m_fadeTime = kFade;
}

void AchievementsScreen::SetScroll(float scroll) {
    if (scroll > m_maxScroll) scroll = m_maxScroll;
    if (scroll < 0.0f) scroll = 0.0f;
    m_scroll = scroll;
    UpdateBar();
}

void AchievementsScreen::Scroll(float amount) {
    if (!m_active || m_closing) return;
    SetScroll(m_scroll + amount * kWheelStep);
}

float AchievementsScreen::ThumbHeight() const {
    const float view = m_listBottom - m_listTop;
    return m_contentHeight > view ? view * view / m_contentHeight : view;
}

bool AchievementsScreen::OnBar(const GLfloat2& p) const {
    return m_maxScroll > 0.0f && p.y >= m_listTop && p.y <= m_listBottom &&
           p.x >= m_barTrack.m_position.x - kBarWidth * 0.5f - kBarReach && p.x <= m_listRight + kBarReach;
}

void AchievementsScreen::UpdateBar() {
    const float view = m_listBottom - m_listTop;
    const float thumb = ThumbHeight();
    const float at = m_maxScroll > 0.0f ? (view - thumb) * m_scroll / m_maxScroll : 0.0f;
    m_barThumb.SetSize(GLfloat2{kBarWidth, thumb});
    m_barThumb.m_position = GLfloat2{m_barTrack.m_position.x, m_listTop + at + thumb * 0.5f};
}

void AchievementsScreen::SetAlpha(uint8_t a) {
    m_alpha = a;
    m_background.SetAlpha(a);
    m_panelBackground.SetAlpha(a);
    m_backButton.SetAlpha(m_backPressed ? static_cast<uint8_t>(a * 0x96 / 0xff) : a);
    m_panel.SetAlpha(a);
    m_heading.m_colour.a = a;
    m_heading.UpdateColour();
    m_count.m_colour.a = a;
    m_count.UpdateColour();
    m_barTrack.SetColour(WithAlpha(kBarTrack, a));
    m_barThumb.SetColour(WithAlpha(kBarThumb, a));
    for (Row& r : m_rows) {
        r.button.SetColour(WithAlpha(r.unlocked ? kUnlockedRow : kLockedRow, a));
        r.tick.SetAlpha(a);
        for (Label* l : {&r.title, &r.lines[0], &r.lines[1], &r.state}) {
            l->m_colour.a = a;
            l->UpdateColour();
        }
    }
}

// Fades in/out; then a drag in the list scrolls it, and a press and release on Back closes it.
void AchievementsScreen::Process(float dt) {
    if (m_fadeTime > 0.0f) {
        float t = m_fadeTime - dt;
        m_fadeTime = t;
        if (t <= 0.0f) {
            m_fadeTime = 0.0f;
            t = 0.0f;
            if (m_closing) {
                m_active = false;
                return;
            }
        }
        const float k = t / kFade;
        SetAlpha(static_cast<uint8_t>(FloatToIntArm(m_closing ? Interpolate(255.0f, 0.0f, k) : Interpolate(0.0f, 255.0f, k))));
        return;
    }
    if (m_closing) return;
    Game* g = m_game;
    const GLfloat2 p{g->m_touchX, g->m_touchY};
    if (m_fling != 0.0f) {   // gliding on after a finger drag (any new touch stops it)
        if (g->m_touchState == TOUCHING) {
            m_fling = 0.0f;
        } else {
            SetScroll(m_scroll + m_fling * dt);
            m_fling *= std::exp(-kFlingFriction * dt);
            if (std::fabs(m_fling) < kFlingStop || m_scroll <= 0.0f || m_scroll >= m_maxScroll) m_fling = 0.0f;
        }
    }
    if (g->m_touchState == TOUCHING && m_lastTouchState == NOT_TOUCHING) {
        m_lastTouchState = TOUCHING;
        const bool onThumb = std::fabs(p.y - m_barThumb.m_position.y) <= ThumbHeight() * 0.5f + kThumbReach;
        if (m_backButton.ContainsPoint(p)) {
            m_backPressed = true;
            g->QueueSound(SOUND_CLICK);
            SetAlpha(m_alpha);
        } else if (OnBar(p) && (g->m_touchIsMouse || onThumb)) {
            // the thumb: a mouse press beside it first centres it there; then it follows the pointer
            // (a finger grabs only the thumb: a swipe at the edge scrolls the list)
            const float half = ThumbHeight() * 0.5f;
            const float thumbY = m_barThumb.m_position.y;
            if (p.y < thumbY - half || p.y > thumbY + half) {
                const float travel = (m_listBottom - m_listTop) - ThumbHeight();
                SetScroll((p.y - m_listTop - half) / travel * m_maxScroll);
            }
            m_draggingBar = true;
            m_dragStartY = p.y;
            m_dragStartScroll = m_scroll;
        } else if (p.y >= m_listTop && p.y <= m_listBottom) {
            m_dragging = true;
            m_dragStartY = p.y;
            m_dragStartScroll = m_scroll;
            m_lastScroll = m_scroll;
            m_velocity = 0.0f;
        }
        return;
    }
    if (g->m_touchState == TOUCHING) {
        if (m_dragging) {   // the list follows the finger; its speed is kept for the fling
            SetScroll(m_dragStartScroll + (m_dragStartY - p.y));
            if (dt > 0.0f) m_velocity = m_velocity * 0.5f + (m_scroll - m_lastScroll) / dt * 0.5f;
            m_lastScroll = m_scroll;
        }
        if (m_draggingBar) {   // the thumb follows the pointer: scrolls the other way, faster
            const float travel = (m_listBottom - m_listTop) - ThumbHeight();
            if (travel > 0.0f) SetScroll(m_dragStartScroll + (p.y - m_dragStartY) / travel * m_maxScroll);
        }
        return;
    }
    if (m_lastTouchState != TOUCHING) return;
    m_lastTouchState = NOT_TOUCHING;
    if (m_dragging && !g->m_touchIsMouse && std::fabs(m_velocity) >= kFlingMin) m_fling = m_velocity;   // a finger flings
    m_dragging = false;
    m_draggingBar = false;
    if (m_backPressed) {
        m_backPressed = false;
        SetAlpha(m_alpha);
        if (m_backButton.ContainsPoint(p) && g->m_touchState == NOT_TOUCHING) FadeOut();
    }
}

void AchievementsScreen::Render() {
    sdlDisable(GL_TEXTURE_2D);
    m_background.Render();
    m_panelBackground.Render();
    sdlEnable(GL_TEXTURE_2D);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_backButton.Render();
    m_panel.Render();
    // the rows in view, scrolled, cut at the list's edges
    const float view = m_listBottom - m_listTop;
    size_t first = m_rows.size(), last = 0;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const float y = m_rows[i].top - m_scroll;
        if (y + kRowHeight < 0.0f || y > view) continue;
        if (first == m_rows.size()) first = i;
        last = i + 1;
    }
    sdlClipBox(m_listLeft, m_listTop, m_listRight, m_listBottom);
    sdlPushMatrix();
    sdlTranslatef(0.0f, -m_scroll, 0.0f);
    for (size_t i = first; i < last; ++i) m_rows[i].button.Render();
    sdlBindTexture(GL_TEXTURE_2D, 1);
    for (size_t i = first; i < last; ++i) m_rows[i].tick.Render();
    for (size_t i = first; i < last; ++i) {
        Row& r = m_rows[i];
        r.title.Render();
        r.lines[0].Render();
        r.lines[1].Render();
        r.state.Render();
    }
    sdlPopMatrix();
    sdlClipOff();
    if (m_maxScroll > 0.0f) {
        sdlDisable(GL_TEXTURE_2D);
        m_barTrack.Render();
        m_barThumb.Render();
        sdlEnable(GL_TEXTURE_2D);
    }
    m_heading.Render();
    m_count.Render();
}
