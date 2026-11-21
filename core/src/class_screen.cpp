// ClassScreen, sub-step 1: the constructor and AddPanel. See class_screen.h.
//
// The constructor builds every strip once: the four tabs along the top, the bars and shades, the
// back button (bottom right) and its partner (bottom left), the content area between, and per tab
// its icon frames, icons, panels and labels. The decompiler shows its loops with constants moving
// between register pairs; the values here follow its data flow (pixel rectangles of the menus
// texture, policies, and the layout arithmetic, all single precision).
#include "class_screen.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "character_class.h"
#include "dr_gl.h"
#include "game.h"
#include "help_menu.h"
#include "skill.h"
#include "tile.h"
#include "util.h"

namespace {
void Add(QuadStrip& s, float x0, float y0, float x1, float y1, int policy, float height = -1.0f) {
    s.AddQuad(GLfloat2{x0, y0}, GLfloat2{x1, y1}, policy, height);
}

QuadStrip& Push(std::vector<QuadStrip>& v) {
    v.push_back(QuadStrip());
    return v.back();
}

Label& PushLabel(std::vector<Label>& v, const Font* font) {
    v.push_back(Label(font));
    return v.back();
}
}  // namespace

void ClassScreen::AddPanel(int tab, float w, float h, float x, float y) {
    QuadStrip& s = Push(m_panels[tab]);
    Add(s, 50, 1, 55, 6, 2);
    Add(s, 55, 1, 98, 6, 1);
    Add(s, 98, 1, 103, 6, 2);
    ++s.m_currentRow;
    Add(s, 50, 6, 55, 49, 0);
    Add(s, 55, 6, 98, 49, 3);
    Add(s, 98, 6, 103, 49, 0);
    ++s.m_currentRow;
    Add(s, 50, 49, 55, 54, 2);
    Add(s, 55, 49, 98, 54, 1);
    Add(s, 98, 49, 103, 54, 2);
    s.m_size = GLfloat2{w, h};
    s.m_fixedSize = true;
    s.UpdateSize();
    s.SetPosition(GLfloat2{x, y});
}

ClassScreen::ClassScreen(Game* game) : m_game(game) {
    const Font* font = &game->m_fonts[0];
    QuadStrip* st = m_strips;

    // the black shades (7: 1 x 1.5, 6: 1 x 1)
    st[7].AddQuad();
    st[7].m_size = GLfloat2{1.0f, 1.5f};
    st[7].m_fixedSize = true;
    st[7].UpdateSize();
    st[7].SetPosition(GLfloat2{0.0f, 0.0f});
    st[7].SetColour(0, 0, 0, 0xff);
    st[6].AddQuad();
    st[6].m_fixedSize = true;
    st[6].m_size = GLfloat2{1.0f, 1.0f};
    st[6].UpdateSize();
    st[6].SetPosition(GLfloat2{0.0f, 0.0f});
    st[6].SetColour(0, 0, 0, 0xff);

    // the four tabs: a 3x3 frame each, overlapping (step 0.203125; the wide tab 2 steps 0.38125)
    float x = -0.0078125f;
    for (int t = 0; t < 4; ++t) {
        QuadStrip& s = st[t];
        Add(s, 1, 1, 11, 9, 2);
        Add(s, 11, 1, 12, 9, 1);
        Add(s, 12, 1, 22, 9, 2);
        ++s.m_currentRow;
        Add(s, 1, 9, 11, 63, 0);
        Add(s, 11, 9, 12, 63, 3);
        Add(s, 12, 9, 22, 63, 0);
        ++s.m_currentRow;
        Add(s, 1, 63, 11, 69, 2);
        Add(s, 11, 63, 12, 69, 1);
        Add(s, 12, 63, 22, 69, 2);
        float width = 0.228125f;   // 0x3e69999a
        float step = 0.203125f;
        if (t == 2) {
            step = 0.38125f;       // 0x3ec33333
            width = 0.40625f;
        }
        s.m_size = GLfloat2{width, 0.25f};
        s.m_fixedSize = true;
        s.UpdateSize();
        s.SetPosition(GLfloat2{x, 0.003125f});
        Label& l = PushLabel(m_tabLabels, font);
        l.m_spacing = 2.0f;
        l.m_scale = GLfloat2{0.1f, 0.1f};
        if (t == 3) break;
        x = x + step;
    }

    // bar under the tabs (4) and the line across (5)
    Add(st[4], 1, 65, 11, 69, 2);
    Add(st[4], 11, 65, 12, 69, 1);
    Add(st[4], 12, 65, 22, 69, 2);
    Add(st[5], 24, 28, 25, 32, 1);
    st[5].m_fixedSize = true;
    st[5].m_size.x = 1.0f;
    st[5].UpdateSize();
    {
        float y = st[0].m_position.y + st[0].GetSize().y;
        st[5].SetPosition(GLfloat2{0.0f, y - st[4].GetSize().y});
    }

    // the back button (10) at the bottom right, the button 9 at the bottom left
    Add(st[10], 104, 1, 193, 27, 2);
    {
        const float margin = 0.009375f;
        float bx = 1.0f - st[10].GetSize().x;
        float by = 1.5f - st[10].GetSize().y;
        st[10].SetPosition(GLfloat2{bx - margin, by - margin});
    }
    Add(st[9], 422, 285, 511, 311, 2);
    st[9].SetPosition(GLfloat2{0.009375f, st[10].m_position.y});

    // the content area (8) from the line down to the buttons
    Add(st[8], 11, 11, 12, 12, 3);
    st[8].SetPosition(GLfloat2{0.0f, st[5].m_position.y + st[5].GetFixedSize(-1, -1).y});
    const float contentTop = st[8].m_position.y;
    const float bottom = st[10].m_position.y - 0.009375f;
    st[8].m_fixedSize = true;
    st[8].m_size = GLfloat2{1.0f, bottom - contentTop};
    st[8].UpdateSize();

    // icon frames 11..21: 13..20 small frames (23,1)-(49,27), the others big (50,1)-(103,54)
    for (int i = 11; i < 22; ++i) {
        if (i >= 13 && i <= 20) Add(st[i], 23, 1, 49, 27, 2);
        else Add(st[i], 50, 1, 103, 54, 2);
    }
    st[11].SetPosition(GLfloat2{st[0].m_position.x + 0.03125f, st[0].m_position.y + 0.0625f});
    st[12].SetPosition(GLfloat2{st[1].m_position.x + 0.03125f, st[1].m_position.y + 0.0625f});
    st[21].SetPosition(GLfloat2{st[3].m_position.x + 0.03125f, st[3].m_position.y + 0.0625f});
    {
        const float fx = st[2].m_position.x + 0.03125f;
        const float fy = st[2].m_position.y + 0.0625f;
        for (int i = 13; i < 21; ++i) {
            int k = i < 17 ? i - 13 : i - 17;
            float dy = i < 17 ? 0.0f : 0.0875f;
            st[i].SetPosition(GLfloat2{fx + static_cast<float>(k) * 0.0875f, dy + fy});
        }
    }

    // icons in the frames: 22, 23 and 32 big, 24..31 small; the grabbed-icon strips big
    GLfloat2 bigTL, bigBR, smallTL, smallBR;
    CharacterClass::GetDummyIconCoords(bigTL, bigBR);
    CharacterClass::GetDummySmallIconCoords(smallTL, smallBR);
    m_grabbedSmallIcon.AddQuad(bigTL, bigBR, 2, -1.0f);
    m_grabbedIcon.AddQuad(bigTL, bigBR, 2, -1.0f);
    for (int i = 22; i < 33; ++i) {
        bool big = i == 22 || i == 23 || i == 32;
        st[i].AddQuad(big ? bigTL : smallTL, big ? bigBR : smallBR, 2, -1.0f);
        st[i].AlignWith(st[i - 11]);
    }
    st[32].m_textureSize = 0x400;

    // ---- tab 0 (class): 10 class frames in two rows of five above the buttons, their icons
    const float rowBase = bottom;
    float gap = 0.0f;
    for (int i = 0; i < 10; ++i) {
        QuadStrip& s = Push(m_panels[0]);
        Add(s, 248, 1, 301, 54, 2);
        float w = s.GetFixedSize(-1, -1).x;
        gap = (1.0f + w * -5.0f) / 6.0f;
        float cx = gap + static_cast<float>(i % 5) * (w + gap);
        float cy = rowBase - (w + gap) * static_cast<float>((9 - i) / 5 + 1);
        s.SetPosition(GLfloat2{cx, cy});
    }
    for (int i = 0; i < 10; ++i) {
        QuadStrip& s = Push(m_panels[0]);
        s.AddQuad(bigTL, bigBR, 2, -1.0f);
        s.AlignWith(m_panels[0][static_cast<size_t>(i)]);
        s.SetColour(0, 0, 0, 0xff);
    }
    const float g = gap;
    const float panelW = g * -2.0f + 1.0f;
    const float halfGap = g * 0.5f;
    m_classPanels = static_cast<int>(m_panels[0].size());
    AddPanel(0, panelW, 0.36f, g, halfGap + contentTop);
    const float inset = (panelW - (((smallBR.x - smallTL.x) * 10.0f) / 320.0f + 0.056250002f)) * 0.25f;
    {
        QuadStrip& p0 = m_panels[0][static_cast<size_t>(m_classPanels)];
        float h1 = (smallBR.y - smallTL.y) / 320.0f + (inset + inset);
        AddPanel(0, panelW, h1, g, halfGap + (p0.m_position.y + p0.GetSize().y));
        QuadStrip& p1 = m_panels[0][static_cast<size_t>(m_classPanels + 1)];
        float h2 = m_panels[0][0].m_position.y - (p1.m_position.y + p1.GetSize().y);
        AddPanel(0, panelW, h2 - g, g, halfGap + (p1.m_position.y + p1.GetSize().y));
    }

    // the class tab's 11 labels: 0 the title (0.12), 1..5 below it, 6 on the line of 1, 7..10 below
    // it (0.0945), x inset from the first panel, lines 0.04914 apart
    {
        const QuadStrip& p0 = m_panels[0][static_cast<size_t>(m_classPanels)];
        float y = inset + p0.m_position.y + 0.042f;
        float line1 = 0.0f;
        for (int i = 0; i < 11; ++i) {
            Label& l = PushLabel(m_classLabels, font);
            l.m_spacing = 2.0f;
            float scale = i == 0 ? 0.12f : 0.0945f;   // 0x3df5c28f, 0x3dc18937
            if (i == 6) y = line1;
            else if (i > 0) y = y + 0.04914f;
            l.m_scale = GLfloat2{scale, scale};
            l.m_position.y = y;
            l.m_position.x = inset + p0.m_position.x;
            if (i == 1) line1 = y;
        }
    }

    // skill frames of the class (10 small frames in a row in the second panel), their icons
    m_skillFrames = static_cast<int>(m_panels[0].size());
    {
        const float sx = g + inset;
        for (int i = 0; i < 10; ++i) {
            QuadStrip& s = Push(m_panels[0]);
            Add(s, 23, 1, 49, 27, 2);
            float step = s.GetFixedSize(-1, -1).x + 0.00625f;
            s.AlignWith(m_panels[0][static_cast<size_t>(m_classPanels + 1)]);
            s.SetPosition(GLfloat2{sx + static_cast<float>(i) * step, s.m_position.y});
        }
        for (int i = 0; i < 10; ++i) {
            QuadStrip& s = Push(m_panels[0]);
            s.AddQuad(smallTL, smallBR, 2, -1.0f);
            s.AlignWith(m_panels[0][static_cast<size_t>(m_skillFrames + i)]);
            s.SetColour(0, 0, 0, 0xff);
        }
    }

    // ---- tab 1 (race): 10 race frames as the class frames, three panels, two portrait frames
    float frameW = 0.0f;
    for (int i = 0; i < 10; ++i) {
        QuadStrip& s = Push(m_panels[1]);
        Add(s, 248, 1, 301, 54, 2);
        float w = s.GetFixedSize(-1, -1).x;
        frameW = w;
        float gp = (1.0f + w * -5.0f) / 6.0f;
        float step = w + gp;
        float cx = gp + static_cast<float>(i % 5) * step;
        float cy = rowBase - step * static_cast<float>((9 - i) / 5 + 1);
        s.SetPosition(GLfloat2{cx, cy});
    }
    for (int i = 0; i < 10; ++i) {
        QuadStrip& s = Push(m_panels[1]);
        s.AddQuad(bigTL, bigBR, 2, -1.0f);
        s.AlignWith(m_panels[1][static_cast<size_t>(i)]);
        s.SetColour(0, 0, 0, 0xff);
    }
    m_racePanels = static_cast<int>(m_panels[1].size());
    AddPanel(1, panelW, g + frameW * 1.35f, g, halfGap + contentTop);
    {
        QuadStrip& r0 = m_panels[1][static_cast<size_t>(m_racePanels)];
        AddPanel(1, panelW, g + frameW * 1.55f, g, halfGap + (r0.m_position.y + r0.GetSize().y));
        QuadStrip& r1 = m_panels[1][static_cast<size_t>(m_racePanels + 1)];
        float h = m_panels[1][0].m_position.y - (r1.m_position.y + r1.GetSize().y);
        AddPanel(1, panelW, h - g, g, halfGap + (r1.m_position.y + r1.GetSize().y));
    }
    {
        const float px = g * 1.5f;
        QuadStrip& a = Push(m_panels[1]);
        Add(a, 50, 1, 103, 54, 2);
        a.SetPosition(GLfloat2{px, halfGap + m_panels[1][static_cast<size_t>(m_racePanels)].m_position.y});
        QuadStrip& b = Push(m_panels[1]);
        Add(b, 50, 1, 103, 54, 2);
        b.SetPosition(GLfloat2{px, halfGap + m_panels[1][static_cast<size_t>(m_racePanels + 1)].m_position.y});
    }
    m_raceIcons = static_cast<int>(m_panels[1].size());
    {
        QuadStrip& s = Push(m_panels[1]);
        s.AddQuad(bigTL, bigBR, 2, -1.0f);
        s.AlignWith(m_panels[1][m_panels[1].size() - 3]);
        s.SetColour(0, 0, 0, 0xff);
        QuadStrip& t = Push(m_panels[1]);
        t.m_textureSize = 0x400;
        t.AddQuad(bigTL, bigBR, 2, -1.0f);
        t.AlignWith(m_panels[1][m_panels[1].size() - 3]);
        t.SetColour(0, 0, 0, 0xff);
    }

    // ---- tab 2 (perks): 20 small frames in two rows of ten above the buttons, their icons, three
    // panels, 8 frames for the perks and flaws (4 in each of the first two panels), one more icon
    float perkGap = 0.0f, perkW = 0.0f;
    for (int i = 0; i < 20; ++i) {
        QuadStrip& s = Push(m_panels[2]);
        Add(s, 23, 33, 49, 59, 2);
        float w = s.GetFixedSize(-1, -1).x;
        float gp = (1.0f + w * -10.0f) / 11.0f;
        perkGap = gp;
        perkW = w;
        float cx = gp + static_cast<float>(i % 10) * (w + gp);
        float cy = rowBase - (w + gp) * static_cast<float>((19 - i) / 10 + 1);
        s.SetPosition(GLfloat2{cx, cy});
    }
    for (int i = 0; i < 20; ++i) {
        QuadStrip& s = Push(m_panels[2]);
        s.AddQuad(smallTL, smallBR, 2, -1.0f);
        s.AlignWith(m_panels[2][static_cast<size_t>(i)]);
        s.SetColour(0, 0, 0, 0xff);
    }
    const float gp = perkGap;
    m_perkPanel2 = static_cast<int>(m_panels[2].size());
    const float listH = gp * 6.0f + (perkW + 0.0f) * 8.0f;
    const float gp2 = gp * -2.0f;
    const float half = listH * 0.5f - gp;
    const float perkPanelW = gp2 + 1.0f;
    const float firstY = contentTop + gp;
    const float twoGp = gp + gp;
    const float listBottom = m_panels[2][0].m_position.y - contentTop;
    AddPanel(2, perkPanelW, half, gp, firstY);
    AddPanel(2, perkPanelW, half, gp, twoGp + contentTop + half);
    m_perkPanel3 = static_cast<int>(m_panels[2].size());
    AddPanel(2, perkPanelW, gp2 + (listBottom - listH), gp, contentTop + gp * 3.0f + half + half);
    m_perkIconsStart = static_cast<int>(m_panels[2].size());
    for (int i = 0; i < 8; ++i) {
        QuadStrip& s = Push(m_panels[2]);
        Add(s, 23, 33, 49, 59, 2);
        float w = s.GetFixedSize(-1, -1).x;
        const QuadStrip& base = m_panels[2][static_cast<size_t>(i < 4 ? m_perkPanel2 : m_perkPanel2 + 1)];
        float y = gp + static_cast<float>(i & 3) * (w + 0.0f) + base.m_position.y;
        s.SetPosition(GLfloat2{twoGp, y});
    }
    for (int i = 0; i < 8; ++i) {
        QuadStrip& s = Push(m_panels[2]);
        s.AddQuad(smallTL, smallBR, 2, -1.0f);
        s.AlignWith(m_panels[2][static_cast<size_t>(m_perkIconsStart + i)]);
        s.SetColour(0, 0, 0, 0xff);
    }
    {
        QuadStrip& s = Push(m_panels[2]);
        s.AddQuad(smallTL, smallBR, 2, -1.0f);
        const QuadStrip& p = m_panels[2][static_cast<size_t>(m_perkPanel3)];
        s.AlignWith(m_panels[2][static_cast<size_t>(m_perkPanel3)]);
        float y = s.m_position.y;
        s.SetPosition(GLfloat2{p.m_position.x + (y - p.m_position.y), y});
        s.SetColour(0, 0, 0, 0xff);
    }

    // ---- tab 3 (abilities): 20 big frames in rows of five, 22 small frames in rows of ten above
    // the buttons, the ability icons (Steal's icon as a placeholder; the small ones at half scale),
    // a panel, one more icon
    float abilityBottom = 0.0f;
    for (int i = 0; i < 20; ++i) {
        QuadStrip& s = Push(m_panels[3]);
        Add(s, 50, 1, 103, 54, 2);
        float w = s.GetFixedSize(-1, -1).x;
        float y = firstY + static_cast<float>(i / 5) * (w + gp);
        float xgap = (gp2 + (1.0f + w * -5.0f)) * 0.25f;
        float cx = gp + static_cast<float>(i % 5) * (w + xgap);
        s.SetPosition(GLfloat2{cx, y});
        abilityBottom = y + w;
    }
    float panel3Top = 0.0f;
    for (int i = 0; i < 22; ++i) {
        QuadStrip& s = Push(m_panels[3]);
        Add(s, 23, 33, 49, 59, 2);
        float w = s.GetFixedSize(-1, -1).x;
        float gpi = (1.0f + w * -10.0f) / 11.0f;
        float rows;
        if (i == 0) {
            rows = 3.0f;
            panel3Top = rowBase - rows * (w + gpi);
        } else {
            rows = static_cast<float>((21 - i) / 10 + 1);
        }
        float cx = gpi + static_cast<float>(i % 10) * (w + gpi);
        float cy = rowBase - (w + gpi) * rows;
        s.SetPosition(GLfloat2{cx, cy});
    }
    GLfloat2 uvTL, uvBR;
    Skill::GetIconUVs(SKILL_STEAL, uvTL, uvBR);
    for (int i = 0; i < 42; ++i) {
        QuadStrip& s = Push(m_panels[3]);
        s.m_textureSize = 0x400;
        s.AddQuad(GLfloat2{uvTL.x * 1024.0f, uvTL.y * 1024.0f}, GLfloat2{uvBR.x * 1024.0f, uvBR.y * 1024.0f}, 2, -1.0f);
        s.AlignWith(m_panels[3][static_cast<size_t>(i)]);
        if (i >= 20) s.m_quads[0].m_scale = GLfloat2{0.5f, 0.5f};
        s.SetColour(0, 0, 0, 0xff);
    }
    {
        float top = abilityBottom + gp;
        AddPanel(3, perkPanelW, (panel3Top - gp) - top, gp, top);
        m_abilityPanel = static_cast<int>(m_panels[3].size()) - 1;
        QuadStrip& s = Push(m_panels[3]);
        s.m_textureSize = 0x400;
        s.AddQuad(GLfloat2{uvTL.x * 1024.0f, uvTL.y * 1024.0f}, GLfloat2{uvBR.x * 1024.0f, uvBR.y * 1024.0f}, 2, -1.0f);
        const QuadStrip& p = m_panels[3][static_cast<size_t>(m_abilityPanel)];
        s.AlignWith(m_panels[3][static_cast<size_t>(m_abilityPanel)]);
        float y = s.m_position.y;
        s.SetPosition(GLfloat2{p.m_position.x + (y - p.m_position.y), y});
        s.SetColour(0, 0, 0, 0xff);
    }

    // tab colours: the tab, its frame strips and its panel frames get the tab's colour; the icons
    // (black so far) become white
    auto tintList = [](std::vector<QuadStrip>& v, uint8_t r, uint8_t gg, uint8_t b) {
        for (QuadStrip& s : v) {
            if (s.m_quads.empty() || s.m_quads[0].m_vertices[0].colour.r != 0) s.SetColour(r, gg, b, 0xff);
            else s.SetColour(0xff, 0xff, 0xff, 0xff);
        }
    };
    st[0].SetColour(150, 150, 255, 0xff);
    st[11].SetColour(150, 150, 255, 0xff);
    tintList(m_panels[0], 150, 150, 255);
    st[1].SetColour(200, 200, 150, 0xff);
    st[12].SetColour(200, 200, 150, 0xff);
    tintList(m_panels[1], 200, 200, 150);
    st[2].SetColour(255, 150, 230, 0xff);
    for (int i = 13; i < 21; ++i) st[i].SetColour(255, 150, 230, 0xff);
    tintList(m_panels[2], 255, 150, 230);
    st[3].SetColour(50, 200, 200, 0xff);
    st[21].SetColour(50, 200, 200, 0xff);
    tintList(m_panels[3], 50, 200, 200);
}

// ================================================================ sub-step 2: texts, tabs, updates
// (Process, GrabIcon and Render follow in sub-step 3.)

namespace {
constexpr GLfloat2 kFrameTL{50.0f, 1.0f}, kFrameBR{103.0f, 54.0f};          // big frame, not selectable
constexpr GLfloat2 kOpenTL{194.0f, 1.0f}, kOpenBR{247.0f, 54.0f};           // big frame, selectable
constexpr GLfloat2 kSmallTL{23.0f, 1.0f}, kSmallBR{49.0f, 27.0f};           // small frame
constexpr GLfloat2 kLockedTL{23.0f, 33.0f}, kLockedBR{49.0f, 59.0f};        // small frame, locked
constexpr GLfloat2 kSlotTL{104.0f, 28.0f}, kSlotBR{130.0f, 54.0f};          // small frame, open slot

GLfloat2 Px(const GLfloat2& uv) { return GLfloat2{uv.x * 1024.0f, uv.y * 1024.0f}; }

char g_raceBonusText[0x40];    // 0x0008923c ("Racial bonus: %s")
char g_mortalEnemyText[0x40];  // 0x0008927c ("Mortal enemy: %s")
char g_skillTitle[0x40];       // 0x000892bc
char g_levelText[8];           // 0x00089234
}  // namespace

void ClassScreen::SetText(int index, const char* text, std::vector<Label>& labels, int& count) {
    if (count < index + 1) count = index + 1;
    while (static_cast<unsigned>(labels.size()) <= static_cast<unsigned>(index)) {
        Label& l = PushLabel(labels, &m_game->m_fonts[0]);
        l.m_spacing = 2.0f;
    }
    Label& l = labels[static_cast<size_t>(index)];
    l.m_scale = GLfloat2{0.09f, 0.09f};   // 0x3db851ec
    l.SetText(text);
}

void ClassScreen::SetText(const char* text, float x, float y, float width, float scale, std::vector<Label>& labels,
                          int& count, GLcolour4 colour) {
    if (y < 0.0f) y = labels[static_cast<size_t>(count - 1)].m_position.y + scale * 0.15f;
    std::vector<std::string> lines;
    m_game->m_fonts[0].SplitLines(text, scale, width, lines);
    const int start = count;
    for (size_t i = 0; i < lines.size(); ++i) {
        y = y + scale * (i == 0 ? 0.35f : 0.5f);
        int index = start + static_cast<int>(i);
        SetText(index, lines[i].c_str(), labels, count);
        Label& l = labels[static_cast<size_t>(index)];
        l.m_scale = GLfloat2{scale, scale};
        l.m_position.y = y;
        l.m_position.x = x;
        l.m_colour = colour;
        l.UpdateColour();
    }
}

void ClassScreen::SetAlpha(unsigned alpha) {
    const uint8_t a = static_cast<uint8_t>(alpha);
    for (QuadStrip& s : m_strips) s.SetAlpha(a);
    m_strips[6].SetAlpha(static_cast<uint8_t>(alpha / 3));
    for (std::vector<Label>* list : {&m_textLabels, &m_levelLabels, &m_classLabels}) {
        for (Label& l : *list) {
            l.m_colour.a = a;
            l.UpdateColour();
        }
    }
    for (std::vector<QuadStrip>& v : m_panels) {
        for (QuadStrip& s : v) s.SetAlpha(a);
    }
    for (Label& l : m_tabLabels) {
        l.m_colour.a = a;
        l.UpdateColour();
    }
    for (int i = 0; i < m_raceLabelCount; ++i) {
        Label& l = m_raceLabels[static_cast<size_t>(i)];
        l.m_colour.a = a;
        l.UpdateColour();
    }
}

void ClassScreen::Initialise() {
    m_active = true;
    m_a74 = 1;
    m_fade = 0.0f;
    m_a78 = -1;
    m_closing = false;
    m_a7c = false;
    m_classLabels[1].SetText("Games Played:");
    m_classLabels[2].SetText("Enemies Killed:");
    m_classLabels[3].SetText("Highest Normal Score:");
    m_classLabels[4].SetText("Highest Hard Score:");
    m_classLabels[5].SetText("Highest Harder Score:");
    m_tabLabels[0].SetText("Class");
    m_tabLabels[1].SetText("Race");
    m_tabLabels[2].SetText("Perks & Flaws");
    m_tabLabels[3].SetText("Skills");
    for (int t = 0; t < 4; ++t) {
        float x = m_strips[t].m_position.x;
        GLfloat2 size = m_strips[t].GetSize();
        Label& l = m_tabLabels[static_cast<size_t>(t)];
        l.m_position.x = x + size.x * 0.5f + l.m_width * l.m_scale.x * -0.5f;
        l.m_position.y = m_strips[t].m_position.y + 0.05f;
    }
    Game* g = m_game;
    for (int c = 0; c < CLASS_COUNT; ++c) {
        if (g->m_classes[c].m_level == 0) continue;
        GLfloat2 tl, br;
        CharacterClass::GetIconCoords(static_cast<ClassType>(c), tl, br);
        m_panels[0][static_cast<size_t>(10 + c)].SetUV(tl, br);
        m_panels[0][static_cast<size_t>(c)].SetUV(kOpenTL, kOpenBR);
        if (c == CLASS_ADVENTURER) SetClassLevel(g->m_classes[0].m_level, 0, m_strips[11]);
        else SetClassLevel(g->m_classes[c].m_level, c, m_panels[0][static_cast<size_t>(c)]);
    }
    for (int r = 0; r < 10; ++r) {
        if (!g->IsRaceUnlocked(static_cast<RaceType>(r))) continue;
        GLfloat2 tl, br;
        CharacterClass::GetIconCoords(static_cast<RaceType>(r), tl, br);
        m_panels[1][static_cast<size_t>(10 + r)].SetUV(tl, br);
        m_panels[1][static_cast<size_t>(r)].SetUV(kOpenTL, kOpenBR);
    }
    for (int p = 0; p < FLAW_COUNT; ++p) {
        if (!g->IsPerkUnlocked(static_cast<PerkType>(p))) continue;
        GLfloat2 tl, br;
        CharacterClass::GetIconCoords(static_cast<PerkType>(p), tl, br);
        m_panels[2][static_cast<size_t>(20 + p)].SetUV(tl, br);
        CharacterClass::GetIconCoords(static_cast<FlawType>(p), tl, br);
        m_panels[2][static_cast<size_t>(30 + p)].SetUV(tl, br);
    }
    UpdateClass();
    SelectTab(0);
    SetAlpha(0);
}

// The class shown may differ from the current one; a locked class falls back to the Adventurer.
void ClassScreen::FadeOut() {
    Game* g = m_game;
    g->m_classes[g->m_currentClass].Validate();
    int shown = g->m_classes[g->m_currentClass].m_class;
    if (g->m_classes[shown].m_level == 0) shown = 0;
    g->m_currentClass = shown;
    g->m_nameScreen.SelectClass(g->m_classes[shown]);
    g->m_currentClassChanged = true;
    g->SerialiseClasses();
    m_closing = true;
}

void ClassScreen::SetClassLevel(int level, int index, QuadStrip& frame) {
    int missing = index + 1 - static_cast<int>(m_levelLabels.size());
    if (static_cast<unsigned>(m_levelLabels.size()) <= static_cast<unsigned>(index)) {
        for (int i = 0; i < missing; ++i) {
            Label& l = PushLabel(m_levelLabels, &m_game->m_fonts[0]);
            l.m_scale = GLfloat2{0.08f, 0.08f};   // 0x3da3d70a
        }
    }
    std::snprintf(g_levelText, sizeof g_levelText, "%d", level);
    Label& l = m_levelLabels[static_cast<size_t>(index)];
    l.SetText(g_levelText);
    GLfloat2 size = frame.GetSize();
    l.m_position.y = (frame.m_position.y + size.y) - 0.009375f;
    l.m_position.x = (frame.m_position.x + size.x) - (l.m_width * l.m_scale.x + 0.009375f);
}

void ClassScreen::ShowText(const char* title, const char* text, bool withIcon) {
    int start = 0;
    bool centred = false;
    float left = 0.0f, width = 0.0f, centreY = 0.0f;
    switch (m_tab) {
        case 0:
        case 1: {
            const bool cls = m_tab == 0;
            std::vector<QuadStrip>& v = m_panels[m_tab];
            float dx = cls ? v[static_cast<size_t>(m_skillFrames)].m_position.x - v[static_cast<size_t>(m_classPanels + 1)].m_position.x
                           : v[static_cast<size_t>(m_raceIcons)].m_position.x - v[static_cast<size_t>(m_racePanels)].m_position.x;
            QuadStrip& p = v[static_cast<size_t>((cls ? m_classPanels : m_racePanels) + 2)];
            centred = cls ? title == nullptr : true;
            float px = p.m_position.x;
            GLfloat2 size = p.GetSize();
            left = dx + px;
            float right = (px + size.x) - dx;
            width = right - left;
            centreY = p.m_position.y + p.GetSize().y * 0.5f;
            break;
        }
        case 2: {
            std::vector<QuadStrip>& v = m_panels[2];
            m_perkIcon = withIcon;
            QuadStrip& p = v[static_cast<size_t>(m_perkPanel3)];
            float px = p.m_position.x;
            float right;
            if (withIcon) {
                QuadStrip& icon = v.back();
                float inset = icon.m_position.y - p.m_position.y;
                left = icon.m_position.x + icon.GetSize().x + inset;
                right = p.m_position.x;
            } else {
                left = px + px;
                right = px;
            }
            centred = !withIcon;
            start = 8;
            width = ((right + p.GetSize().x) - px) - left;
            centreY = p.m_position.y + p.GetSize().y * 0.5f;
            break;
        }
        case 3: {
            std::vector<QuadStrip>& v = m_panels[3];
            m_abilityIcon = withIcon;
            QuadStrip& p = v[static_cast<size_t>(m_abilityPanel)];
            QuadStrip& icon = v.back();
            float inset = icon.m_position.y - p.m_position.y;
            float px;
            if (withIcon) {
                left = icon.m_position.x + icon.GetSize().x + inset;
                px = p.m_position.x;
            } else {
                inset = inset + 0.009375f;
                px = p.m_position.x;
                left = px + inset;
            }
            centred = !withIcon;
            width = ((px + p.GetSize().x) - inset) - left;
            centreY = p.m_position.y + p.GetSize().y * 0.5f;
            break;
        }
        default:
            break;
    }
    std::vector<std::string> lines;
    m_game->m_fonts[0].SplitLines(text, 0.09f, width, lines);
    const int n = static_cast<int>(lines.size());
    float height = static_cast<float>(static_cast<unsigned>(n)) * 0.5f * 0.09f;
    float top;
    if (title == nullptr) {
        top = 0.009000001f;
    } else {
        height = height + 0.06f;
        top = 0.012000001f;
    }
    const int hasTitle = title != nullptr ? 1 : 0;
    m_textLabelCount = n + hasTitle + start;
    if (start >= m_textLabelCount) return;
    float y = (centreY + height * -0.5f) - top;
    for (int i = start; i < m_textLabelCount; ++i) {
        bool isTitle = hasTitle && i == start;
        float step = isTitle ? 0.06f : 0.045f;
        float scale = isTitle ? 0.12f : 0.09f;   // 0x3df5c28f, 0x3db851ec
        const char* s = isTitle ? title : lines[static_cast<size_t>(i - start - hasTitle)].c_str();
        y = y + step;
        SetText(i, s, m_textLabels, m_textLabelCount);
        Label& l = m_textLabels[static_cast<size_t>(i)];
        l.m_scale = GLfloat2{scale, scale};
        l.m_position.y = y;
        l.m_position.x = centred ? left + (width - l.m_width * l.m_scale.x) * 0.5f : left;
    }
}

void ClassScreen::ShowSkillText(SkillType skill) {
    GLfloat2 tl, br;
    Skill::GetIconUVs(skill, tl, br);
    m_panels[3].back().SetUV(Px(tl), Px(br));
    if (Skill::GetBaseCooldown(skill) < 1) {
        std::snprintf(g_skillTitle, sizeof g_skillTitle, "%s", Skill::GetSkillName(skill));
    } else {
        std::snprintf(g_skillTitle, sizeof g_skillTitle, "%s (%d)", Skill::GetSkillName(skill), Skill::GetBaseCooldown(skill));
    }
    ShowText(g_skillTitle, Skill::GetSkillDescription(skill), true);
}

void ClassScreen::ShowFlawText(FlawType flaw) {
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(flaw, tl, br);
    m_panels[2].back().SetUV(tl, br);
    ShowText(CharacterClass::GetFlawName(flaw), CharacterClass::GetFlawDescription(flaw), true);
}

void ClassScreen::ShowPerkText(PerkType perk) {
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(perk, tl, br);
    m_panels[2].back().SetUV(tl, br);
    ShowText(CharacterClass::GetPerkName(perk), CharacterClass::GetPerkDescription(perk), true);
}

void ClassScreen::ShowRace(RaceType race) {
    const float big = 0.095f, small = 0.085f;
    std::vector<QuadStrip>& v = m_panels[1];
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(race, tl, br);
    v[static_cast<size_t>(m_raceIcons)].SetUV(tl, br);
    Tile::GetEnemyUV(CharacterClass::GetMortalEnemyType(race), tl, br);
    v[static_cast<size_t>(m_raceIcons + 1)].SetUV(Px(tl), Px(br));
    QuadStrip& p1 = v[static_cast<size_t>(m_raceIcons)];
    QuadStrip& r0 = v[static_cast<size_t>(m_racePanels)];
    const float inset = p1.m_position.x - r0.m_position.x;
    const float y1 = p1.m_position.y;
    const float x = inset + (p1.m_position.x + p1.GetSize().x);
    const float width = ((r0.m_position.x + r0.GetSize().x) - x) - inset;
    m_raceLabelCount = 0;
    std::snprintf(g_raceBonusText, sizeof g_raceBonusText, "Racial bonus: %s", CharacterClass::GetRacialBonusName(race));
    const GLcolour4 white{0xff, 0xff, 0xff, 0xff};
    SetText(CharacterClass::GetRaceName(race), x, y1, width, big, m_raceLabels, m_raceLabelCount, white);
    SetText(g_raceBonusText, x, -1.0f, width, big, m_raceLabels, m_raceLabelCount, white);
    SetText(CharacterClass::GetRacialBonusDescription(race), x, -1.0f, width, small, m_raceLabels, m_raceLabelCount, white);
    QuadStrip& p2 = v[static_cast<size_t>(m_raceIcons + 1)];
    const float x2 = inset + (p2.m_position.x + p2.GetSize().x);
    const float y2 = p2.m_position.y;
    EnemyType enemy = CharacterClass::GetMortalEnemyType(race);
    std::snprintf(g_mortalEnemyText, sizeof g_mortalEnemyText, "Mortal enemy: %s", Tile::GetEnemyName(enemy));
    SetText(g_mortalEnemyText, x2, y2, width, big, m_raceLabels, m_raceLabelCount, white);
    const char* text;
    GLcolour4 colour;
    if (race == RACE_HUMAN) {
        text = CharacterClass::GetMortalEnemyDescription(RACE_HUMAN);
        colour = white;
    } else {
        SetText(Tile::GetEnemyDescription(enemy), x2, -1.0f, width, small, m_raceLabels, m_raceLabelCount, white);
        text = CharacterClass::GetMortalEnemyDescription(race);
        colour = GLcolour4{0xff, 0, 0, 0xff};
    }
    SetText(text, x2, -1.0f, width, small, m_raceLabels, m_raceLabelCount, colour);
}

void ClassScreen::UpdateRace() {
    Game* g = m_game;
    CharacterClass& cls = g->m_classes[g->m_currentClass];
    for (int r = 0; r < 10; ++r) {
        if (!g->IsRaceUnlocked(static_cast<RaceType>(r))) continue;
        if (cls.m_class == CLASS_ADVENTURER || cls.m_level < 5 || cls.m_race == r) {
            m_panels[1][static_cast<size_t>(r)].SetUV(kFrameTL, kFrameBR);
        } else {
            m_panels[1][static_cast<size_t>(r)].SetUV(kOpenTL, kOpenBR);
        }
    }
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(cls.m_race, tl, br);
    m_strips[23].SetUV(tl, br);
    ShowRace(cls.m_race);
}

void ClassScreen::UpdateClass() {
    Game* g = m_game;
    CharacterClass& cls = g->m_classes[g->m_currentClass];
    std::vector<QuadStrip>& v = m_panels[0];
    for (int c = 0; c < CLASS_COUNT; ++c) {
        if (g->m_classes[c].m_level == 0) continue;
        if (c == cls.m_class) v[static_cast<size_t>(c)].SetUV(kFrameTL, kFrameBR);
        else v[static_cast<size_t>(c)].SetUV(kOpenTL, kOpenBR);
    }
    GLfloat2 tl, br;
    CharacterClass::GetIconCoords(cls.m_class, tl, br);
    m_strips[22].SetUV(tl, br);
    GLfloat2 dTL, dBR;
    CharacterClass::GetDummySmallIconCoords(dTL, dBR);
    for (int i = 0; i < 10; ++i) {
        QuadStrip& frame = v[static_cast<size_t>(m_skillFrames + i)];
        QuadStrip& icon = v[static_cast<size_t>(m_skillFrames + 10 + i)];
        if (i == 0 || cls.m_class != CLASS_ADVENTURER) {
            if (i < cls.m_level) frame.SetUV(kSlotTL, kSlotBR);
            else frame.SetUV(kSmallTL, kSmallBR);
            float u = static_cast<float>(26 * i);
            icon.SetUV(GLfloat2{u, 196.0f}, GLfloat2{u + 26.0f, 222.0f});
        } else {
            frame.SetUV(kLockedTL, kLockedBR);
            icon.SetUV(dTL, dBR);
        }
    }
    SetClassLevel(cls.m_level, 0, m_strips[11]);
    UpdateRace();
    UpdatePerks();
    UpdateAbilities();
    ShowText(nullptr,
             "Apart from the Adventurer, killing special enemies with a class will level it up. Tap the buttons above to see level rewards",
             false);
    std::snprintf(m_text, sizeof m_text, "%s (Level %d)", CharacterClass::GetClassName(cls.m_class), cls.m_level);
    m_classLabels[0].SetText(m_text);
    const int values[5] = {cls.m_gamesPlayed, cls.m_enemiesKilled, static_cast<int>(cls.m_bestScore[0]),
                           static_cast<int>(cls.m_bestScore[1]), static_cast<int>(cls.m_bestScore[2])};
    for (int k = 0; k < 5; ++k) {
        std::snprintf(m_text, sizeof m_text, "%d", values[k]);
        m_classLabels[static_cast<size_t>(6 + k)].SetText(m_text);
    }
    const float dx = v[static_cast<size_t>(m_skillFrames)].m_position.x - v[static_cast<size_t>(m_classPanels + 2)].m_position.x;
    for (int k = 6; k < 11; ++k) {
        QuadStrip& p0 = v[static_cast<size_t>(m_classPanels)];
        float px = p0.m_position.x;
        GLfloat2 size = p0.GetSize();
        Label& l = m_classLabels[static_cast<size_t>(k)];
        l.m_position.x = ((px + size.x) - dx) - l.m_width * l.m_scale.x;
    }
}

void ClassScreen::UpdateAbilities() {
    Game* g = m_game;
    CharacterClass& cls = g->m_classes[g->m_currentClass];
    std::vector<QuadStrip>& v = m_panels[3];
    GLfloat2 tl, br;
    if (cls.m_class == CLASS_ADVENTURER) {
        Skill::GetIconUVs(SKILL_TELEPORT, tl, br);
        m_strips[32].SetUV(Px(tl), Px(br));
    }
    for (int k = 0; k < 20; ++k) {
        Skill::GetIconUVs(cls.m_baseSkills[static_cast<size_t>(k)], tl, br);
        if (k == 0 && cls.m_class != CLASS_ADVENTURER) {
            m_strips[32].SetUV(Px(tl), Px(br));
        } else if (cls.m_replacementSkills[static_cast<size_t>(k)] == 0xff) {
            v[static_cast<size_t>(k)].SetUV(kFrameTL, kFrameBR);
        } else {
            v[static_cast<size_t>(k)].SetUV(kOpenTL, kOpenBR);
            Skill::GetIconUVs(cls.m_replacementSkills[static_cast<size_t>(k)], tl, br);
        }
        v[static_cast<size_t>(42 + k)].SetUV(Px(tl), Px(br));
    }
    for (int k = 0; k < 22; ++k) {
        SkillType skill = cls.m_nonBaseSkills[static_cast<size_t>(k)];
        m_aac[k] = 0;
        QuadStrip& icon = v[static_cast<size_t>(62 + k)];
        QuadStrip& frame = v[static_cast<size_t>(20 + k)];
        if (!g->IsSkillUnlocked(skill)) {
            icon.SetUV(GLfloat2{0.0f, 0.0f}, GLfloat2{0.0f, 0.0f});
            frame.SetUV(kLockedTL, kLockedBR);
            continue;
        }
        Skill::GetIconUVs(skill, tl, br);
        icon.SetUV(Px(tl), Px(br));
        if (cls.GetReplacementSkillsAllowed() == 0 || cls.IsReplacementSkill(skill) || m_aa8 == k ||
            cls.m_class == CLASS_ADVENTURER) {
            frame.SetUV(kSmallTL, kSmallBR);
        } else {
            frame.SetUV(kSlotTL, kSlotBR);
            m_aac[k] = 1;
        }
    }
}

void ClassScreen::UpdatePerks() {
    Game* g = m_game;
    CharacterClass& cls = g->m_classes[g->m_currentClass];
    std::vector<QuadStrip>& v = m_panels[2];
    GLfloat2 dTL, dBR;
    CharacterClass::GetDummySmallIconCoords(dTL, dBR);
    QuadStrip& f0 = v[static_cast<size_t>(m_perkIconsStart)];
    const float textX = f0.m_position.x + f0.GetSize().x + v[static_cast<size_t>(m_perkPanel2)].m_position.x;
    const float textDy = (v[static_cast<size_t>(m_perkIconsStart)].GetSize().y + 0.0315f) * 0.5f;
    auto frame = [&](int k) -> QuadStrip& { return v[static_cast<size_t>(m_perkIconsStart + k)]; };
    for (int k = 0; k < 4; ++k) {
        if (cls.m_class == CLASS_ADVENTURER || !cls.IsPerkUnlocked(k)) {
            m_strips[13 + k].SetUV(kLockedTL, kLockedBR);
            m_strips[17 + k].SetUV(kLockedTL, kLockedBR);
            m_strips[24 + k].SetUV(dTL, dBR);
            m_strips[28 + k].SetUV(dTL, dBR);
            frame(k).SetUV(kLockedTL, kLockedBR);
            frame(4 + k).SetUV(kLockedTL, kLockedBR);
            frame(8 + k).SetUV(dTL, dBR);
            frame(12 + k).SetUV(dTL, dBR);
            SetText(k, "", m_textLabels, m_textLabelCount);
            SetText(4 + k, "", m_textLabels, m_textLabelCount);
            continue;
        }
        m_strips[13 + k].SetUV(kSmallTL, kSmallBR);
        m_strips[17 + k].SetUV(kSmallTL, kSmallBR);
        PerkType perk = cls.m_perks[k];
        if (perk == PERK_NONE) {
            frame(k).SetUV(kSmallTL, kSmallBR);
            frame(8 + k).SetUV(dTL, dBR);
            m_strips[24 + k].SetUV(dTL, dBR);
            SetText(k, "", m_textLabels, m_textLabelCount);
        } else {
            GLfloat2 tl, br;
            CharacterClass::GetIconCoords(perk, tl, br);
            m_strips[24 + k].SetUV(tl, br);
            if (k == 0) frame(0).SetUV(kSmallTL, kSmallBR);
            else frame(k).SetUV(kSlotTL, kSlotBR);
            frame(8 + k).SetUV(tl, br);
            SetText(k, CharacterClass::GetPerkDescription(perk), m_textLabels, m_textLabelCount);
            Label& l = m_textLabels[static_cast<size_t>(k)];
            l.m_position.x = textX;
            l.m_position.y = textDy + frame(k).m_position.y;
        }
        FlawType flaw = cls.m_flaws[k];
        if (flaw == FLAW_NONE) {
            frame(4 + k).SetUV(kSmallTL, kSmallBR);
            frame(12 + k).SetUV(dTL, dBR);
            m_strips[28 + k].SetUV(dTL, dBR);
            SetText(4 + k, "", m_textLabels, m_textLabelCount);
        } else {
            GLfloat2 tl, br;
            CharacterClass::GetIconCoords(flaw, tl, br);
            m_strips[28 + k].SetUV(tl, br);
            if (k == 0) frame(4).SetUV(kSmallTL, kSmallBR);
            else frame(4 + k).SetUV(kSlotTL, kSlotBR);
            frame(12 + k).SetUV(tl, br);
            SetText(4 + k, CharacterClass::GetFlawDescription(flaw), m_textLabels, m_textLabelCount);
            Label& l = m_textLabels[static_cast<size_t>(4 + k)];
            l.m_position.x = textX;
            l.m_position.y = textDy + frame(4 + k).m_position.y;
        }
    }
    // the perk and flaw lists: unlocked ones are slots to drag from when the class has a free perk slot
    for (int p = 0; p < 10; ++p) {
        if (!g->IsPerkUnlocked(static_cast<PerkType>(p))) {
            m_a84[p] = 0;
            m_a84[p + 10] = 0;
        } else if (cls.m_class == CLASS_ADVENTURER || !cls.IsPerkUnlocked(1)) {
            v[static_cast<size_t>(p)].SetUV(kSmallTL, kSmallBR);
            v[static_cast<size_t>(10 + p)].SetUV(kSmallTL, kSmallBR);
            m_a84[p] = 0;
            m_a84[p + 10] = 0;
        } else {
            m_a84[p] = 1;
            m_a84[p + 10] = 1;
            v[static_cast<size_t>(p)].SetUV(kSlotTL, kSlotBR);
            v[static_cast<size_t>(10 + p)].SetUV(kSlotTL, kSlotBR);
        }
    }
    for (int k = 0; k < 4; ++k) {
        int perk = cls.m_perks[k];
        if (perk != 0xff) {
            v[static_cast<size_t>(perk)].SetUV(kSmallTL, kSmallBR);
            m_a84[perk] = 0;
        }
        int flaw = cls.m_flaws[k];
        if (flaw != 0xff) {
            v[static_cast<size_t>(flaw + 10)].SetUV(kSmallTL, kSmallBR);
            m_a84[flaw + 10] = 0;
        }
    }
    if (m_a80 >= 0) {
        v[static_cast<size_t>(m_a80)].SetUV(kSmallTL, kSmallBR);
        m_a84[m_a80] = 0;
    }
}

void ClassScreen::SelectTab(int tab) {
    m_tab = tab;
    QuadStrip& bar = m_strips[4];
    GLfloat2 size = m_strips[tab].GetSize();
    bar.m_fixedSize = true;
    bar.m_size.x = size.x;
    bar.UpdateSize();
    bar.SetPosition(GLfloat2{m_strips[m_tab].m_position.x, m_strips[5].m_position.y});
    Game* g = m_game;
    m_textLabelCount = 0;
    m_perkIcon = false;
    m_abilityIcon = false;
    CharacterClass& cls = g->m_classes[g->m_currentClass];
    cls.Validate();
    UpdatePerks();
    uint8_t r = 0, gg = 0, b = 0;
    const char* name = CharacterClass::GetClassName(cls.m_class);
    switch (tab) {
        case 0:
            m_textLabelCount = 0;
            ShowText(nullptr,
                     "Tap on an unlocked class below to view and customise its current setup. Kill special enemies to unlock and level up classes",
                     false);
            r = 150, gg = 150, b = 255;
            break;
        case 1: {
            m_textLabelCount = 0;
            const char* text = m_text;
            if (cls.m_class == CLASS_ADVENTURER) {
                text = "The Adventurer can only be Human. Kill special enemies to unlock and level up other classes to unlock other races!";
            } else if (cls.m_level < 5) {
                std::snprintf(m_text, sizeof m_text,
                              "The %s can currently only be %s. Kill special enemies to upgrade this class and unlock race selection at level 5",
                              name, CharacterClass::GetRaceName(cls.m_race));
            } else {
                std::strcpy(m_text, "Tap the buttons below to choose a different race for this class");
            }
            ShowText(nullptr, text, false);
            r = 200, gg = 200, b = 150;
            break;
        }
        case 2:
            if (cls.m_class == CLASS_ADVENTURER) {
                std::memcpy(m_text,
                            "The Adventurer class cannot be customised with any perks or flaws. Kill special enemies to unlock and customise other classes!",
                            0x7f);
            } else if (!cls.IsPerkUnlocked(1)) {
                std::snprintf(m_text, sizeof m_text,
                              "The %s's perks (blue) and flaws (red) are listed above. Level up this class to open more perk & flaw slots", name);
            } else {
                std::snprintf(m_text, sizeof m_text,
                              "The %s's perks (blue) and flaws (red) are listed above. Drag in new ones from below list, but make sure each perk is matched by a flaw",
                              name);
            }
            ShowText(nullptr, m_text, false);
            r = 255, gg = 150, b = 230;
            break;
        case 3: {
            m_textLabelCount = 0;
            const char* text = m_text;
            if (cls.m_class == CLASS_ADVENTURER) {
                text = "The Adventurer's level up skills are chosen from the pool of all currently unlocked skills";
            } else if (cls.GetReplacementSkillsAllowed() == 0) {
                std::snprintf(m_text, sizeof m_text,
                              "The %s's level up skills are chosen from the above list. Level up this class to have the option to customise its skills",
                              name);
            } else {
                std::snprintf(m_text, sizeof m_text,
                              "The %s's level up skills are chosen from the above list. You can replace %d of them (currently replacing %d) by dragging them from below",
                              name, cls.GetReplacementSkillsAllowed(), cls.GetReplacementSkillCount());
            }
            ShowText(nullptr, text, false);
            r = 50, gg = 200, b = 200;
            break;
        }
        default:
            break;   // (the original leaves the colour registers unset)
    }
    bar.SetColour(r, gg, b, 0xff);
    m_strips[8].SetColour(r, gg, b, 0xff);
    m_strips[5].SetColour(r, gg, b, 0xff);
    m_grabbedIcon.SetColour(r, gg, b, 0xff);
}

void ClassScreen::RenderTab(int tab) {
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_strips[tab].Render();
    Game* g = m_game;
    switch (tab) {
        case 0:
            m_strips[11].Render();
            m_strips[22].Render();
            if (g->m_classes[g->m_currentClass].m_class == CLASS_ADVENTURER) {
                if (m_levelLabels.size() > 1) m_levelLabels[1].Render();
            } else {
                m_levelLabels[0].Render();
            }
            break;
        case 1:
            m_strips[12].Render();
            m_strips[23].Render();
            break;
        case 2:
            for (int i = 13; i < 21; ++i) {
                m_strips[i].Render();
                m_strips[i + 11].Render();
            }
            break;
        case 3:
            m_strips[21].Render();
            sdlBindTexture(GL_TEXTURE_2D, 1);
            m_strips[32].Render();
            break;
        default:
            break;
    }
    m_tabLabels[static_cast<size_t>(tab)].Render();
}

// ================================================================ sub-step 3: input and drawing

// Starts dragging: the frame `frame` and icon `icon` of the tab's list follow the touch (offsets kept;
// the frame's offset ends up equal to the icon's, as in the original). Skills (tab 3) drag at half scale
// with the skill's icon (texture 1024) in an open big frame; perks/flaws in a slot frame.
void ClassScreen::GrabIcon(int tab, int frame, int icon, int item) {
    Game* g = m_game;
    std::vector<QuadStrip>& v = m_panels[tab];
    QuadStrip& f = v[static_cast<size_t>(frame)];
    m_a98[1] = f.m_position.y - g->m_touchY;
    m_a98[0] = f.m_position.x - g->m_touchX;
    QuadStrip& ic = v[static_cast<size_t>(icon)];
    m_a98[2] = ic.m_position.x - g->m_touchX;
    m_a98[3] = ic.m_position.y - g->m_touchY;
    m_a98[0] = m_a98[2];
    m_a98[1] = m_a98[3];
    float scale;
    GLfloat2 frameTL, frameBR;
    if (tab == 3) {
        m_a98[0] = m_a98[0] - 0.00625f;
        m_a98[1] = m_a98[1] - 0.00625f;
        GLfloat2 tl, br;
        Skill::GetIconUVs(g->m_classes[g->m_currentClass].m_nonBaseSkills[static_cast<size_t>(item)], tl, br);
        m_grabbedSmallIcon.m_textureSize = 0x400;
        m_grabbedSmallIcon.SetUV(Px(tl), Px(br));
        scale = 0.5f;
        frameTL = kOpenTL;
        frameBR = kOpenBR;
    } else {
        GLfloat2 tl, br;
        if (item < 10) CharacterClass::GetIconCoords(static_cast<PerkType>(item), tl, br);
        else CharacterClass::GetIconCoords(static_cast<FlawType>(item - FLAW_COUNT), tl, br);
        m_grabbedSmallIcon.m_textureSize = 0x200;
        m_grabbedSmallIcon.SetUV(tl, br);
        scale = 1.0f;
        frameTL = kSlotTL;
        frameBR = kSlotBR;
    }
    m_grabbedSmallIcon.m_fixedSize = true;
    m_grabbedSmallIcon.m_size = GLfloat2{0.0f, 0.0f};
    m_grabbedSmallIcon.UpdateSize();
    m_grabbedSmallIcon.m_quads[0].m_scale = GLfloat2{scale, scale};
    m_grabbedIcon.SetUV(frameTL, frameBR);
    m_grabbedIcon.m_fixedSize = true;
    m_grabbedIcon.m_size = GLfloat2{0.0f, 0.0f};
    m_grabbedIcon.UpdateSize();
    m_grabbedIcon.m_quads[0].m_scale = GLfloat2{scale, scale};
    m_grabbedIcon.SetPosition(GLfloat2{g->m_touchX + m_a98[0], g->m_touchY + m_a98[1]});
    m_grabbedSmallIcon.SetPosition(GLfloat2{g->m_touchX + m_a98[2], g->m_touchY + m_a98[3]});
}

namespace {
// Squared distance between a strip's top left and the touch moved up-left by half the strip.
float DropDistance(QuadStrip& s, const Game* g) {
    GLfloat2 size = s.GetSize();
    GLfloat2 half{size.x / 2.0f, size.y / 2.0f};
    float dx = s.m_position.x - (g->m_touchX - half.x);
    float dy = s.m_position.y - (g->m_touchY - half.y);
    return dy * dy + dx * dx;
}
}  // namespace

void ClassScreen::Process(float dt) {
    Game* g = m_game;
    float fade = m_fade;
    if (fade < 1.0f || m_closing) {
        if (fade < 1.0f && !m_closing) fade = fade + dt * 5.0f;
        else fade = fade + dt * -5.0f;
        m_fade = fade;
        if (fade < 0.0f) {
            m_active = false;
            fade = m_fade;
        }
        fade = Clamp(fade, 0.0f, 1.0f);
        m_fade = fade;
        float a = fade * 255.0f;
        SetAlpha(static_cast<unsigned>(0.0f < a ? static_cast<int>(a) : 0));
        return;
    }
    CharacterClass& cls = g->m_classes[g->m_currentClass];
    const GLfloat2 touch{g->m_touchX, g->m_touchY};
    // ---- a new touch
    if (m_a74 == 0) {
        if (g->m_touchState != 1) return;
        m_a74 = 1;
        if (m_strips[10].ContainsPoint(touch)) {
            m_a7c = true;
            m_strips[10].SetAlpha(150);
            g->QueueSound(SOUND_CLICK);
            return;
        }
        if (m_strips[9].ContainsPoint(touch)) {
            m_a7d = true;
            m_strips[9].SetAlpha(150);
            g->QueueSound(SOUND_CLICK);
            return;
        }
        for (int t = 0; t < 4; ++t) {
            if (t == m_tab || !m_strips[t].ContainsPoint(touch)) continue;
            g->QueueSound(SOUND_CLICK);
            SelectTab(t);
            g->SerialiseClasses();
            return;
        }
        switch (m_tab) {
            case 0: {
                std::vector<QuadStrip>& v = m_panels[0];
                for (int c = 0; c < 10; ++c) {
                    if (!v[static_cast<size_t>(c)].ContainsPoint(touch)) continue;
                    if (g->m_classes[c].m_level == 0) return;
                    v[static_cast<size_t>(c)].SetAlpha(150);
                    m_a78 = c;
                    g->QueueSound(SOUND_CLICK);
                    return;
                }
                int i = 0;
                while (!v[static_cast<size_t>(m_skillFrames + i)].ContainsPoint(touch)) {
                    if (++i == 10) return;
                }
                if (cls.m_class == CLASS_ADVENTURER && i != 0) {
                    ShowText("Adventurer", "The adventurer class doesn't level up and so doesn't receive any level rewards", false);
                } else {
                    std::snprintf(m_text, sizeof m_text, "Level %d", i + 1);
                    ShowText(m_text, cls.GetLevelDescription(i + 1), false);
                }
                break;
            }
            case 1: {
                std::vector<QuadStrip>& v = m_panels[1];
                int r = 0;
                while (!v[static_cast<size_t>(r)].ContainsPoint(touch)) {
                    if (++r == RACE_COUNT) {
                        if (v[static_cast<size_t>(m_racePanels)].ContainsPoint(touch)) {
                            g->QueueSound(SOUND_CLICK);
                            ShowText(nullptr,
                                     "Most races have a racial bonus, which acts a passive benefit while you're playing a game using the associated race",
                                     false);
                        } else if (v[static_cast<size_t>(m_racePanels + 1)].ContainsPoint(touch)) {
                            g->QueueSound(SOUND_CLICK);
                            ShowText(nullptr,
                                     "Mortal enemies are tougher versions of special enemies. 5% of the time a special enemy will be upgraded to your race's mortal enemy",
                                     false);
                        }
                        return;
                    }
                }
                if (!g->IsRaceUnlocked(static_cast<RaceType>(r))) return;
                if (cls.m_class == CLASS_ADVENTURER || cls.m_level < 5) {
                    ShowRace(static_cast<RaceType>(r));
                } else {
                    v[static_cast<size_t>(r)].SetAlpha(150);
                    m_a78 = r;
                }
                break;
            }
            case 2: {
                std::vector<QuadStrip>& v = m_panels[2];
                int p = 0;
                while (!v[static_cast<size_t>(p)].ContainsPoint(touch)) {
                    if (++p == 20) {
                        // the class's own perk and flaw slots: drag one out (the fixed first ones only show text)
                        for (int slot = 0; slot < 8; ++slot) {
                            if (!v[static_cast<size_t>(m_perkIconsStart + slot)].ContainsPoint(touch)) continue;
                            int value = slot < 4 ? cls.m_perks[slot] : cls.m_flaws[slot & 3];
                            if (value == 0xff) return;
                            int k = slot & 3;
                            if (k != 0) {
                                if (slot < 4) {
                                    m_a80 = cls.m_perks[slot];
                                    ShowPerkText(cls.m_perks[slot]);
                                    cls.SetPerk(slot, PERK_NONE);
                                } else {
                                    m_a80 = cls.m_flaws[k] + 10;
                                    ShowFlawText(cls.m_flaws[k]);
                                    cls.SetFlaw(k, FLAW_NONE);
                                }
                                g->QueueSound(SOUND_CLICK);
                                GrabIcon(2, slot + m_perkIconsStart, slot + m_perkIconsStart + 8, m_a80);
                                UpdatePerks();
                                return;
                            }
                            g->QueueSound(SOUND_CLICK);
                            if (slot < 4) ShowPerkText(cls.m_perks[slot]);
                            else ShowFlawText(cls.m_flaws[0]);
                        }
                        return;
                    }
                }
                if (m_a84[p] != 0) {
                    g->QueueSound(SOUND_CLICK);
                    m_a80 = p;
                    GrabIcon(2, p, p + 20, p);
                    UpdatePerks();
                }
                if (!g->IsPerkUnlocked(static_cast<PerkType>(p % 10))) return;
                g->QueueSound(SOUND_CLICK);
                if (p < 10) ShowPerkText(static_cast<PerkType>(p));
                else ShowFlawText(static_cast<FlawType>(p % 10));
                return;
            }
            case 3: {
                std::vector<QuadStrip>& v = m_panels[3];
                int k = 0;
                while (!v[static_cast<size_t>(20 + k)].ContainsPoint(touch)) {
                    if (++k == 22) {
                        // the active skills: a replaced one can be dragged out
                        for (int s = 0; s < 20; ++s) {
                            if (!v[static_cast<size_t>(s)].ContainsPoint(touch)) continue;
                            SkillType replacement = cls.m_replacementSkills[static_cast<size_t>(s)];
                            if (replacement != 0xff) {
                                g->QueueSound(SOUND_CLICK);
                                m_aa8 = cls.GetNonBaseSkillIndex(replacement);
                                cls.SetReplacementSkill(s, SKILL_NONE);
                                GrabIcon(3, s, s + 42, m_aa8);
                                m_grabbedSmallIcon.m_quads[0].m_scale = GLfloat2{1.0f, 1.0f};
                                m_grabbedIcon.m_quads[0].m_scale = GLfloat2{1.0f, 1.0f};
                                UpdateAbilities();
                                ShowSkillText(cls.m_nonBaseSkills[static_cast<size_t>(m_aa8)]);
                                return;
                            }
                            g->QueueSound(SOUND_CLICK);
                            ShowSkillText(cls.m_baseSkills[static_cast<size_t>(s)]);
                            return;
                        }
                        return;
                    }
                }
                if (m_aac[k] != 0) {
                    g->QueueSound(SOUND_CLICK);
                    m_aa8 = k;
                    GrabIcon(3, k + 20, k + 62, k);
                    UpdateAbilities();
                }
                SkillType skill = cls.m_nonBaseSkills[static_cast<size_t>(k)];
                if (!g->IsSkillUnlocked(skill)) return;
                g->QueueSound(SOUND_CLICK);
                ShowSkillText(skill);
                return;
            }
            default:
                return;
        }
        g->QueueSound(SOUND_CLICK);
        return;
    }
    // ---- dragging (or no release yet)
    if (m_a74 != 1 || g->m_touchState != 0) {
        if (g->m_touchState != 1) return;
        if (m_a80 < 0 && m_aa8 < 0) return;
        m_grabbedIcon.SetPosition(GLfloat2{g->m_touchX + m_a98[0], g->m_touchY + m_a98[1]});
        m_grabbedSmallIcon.SetPosition(GLfloat2{g->m_touchX + m_a98[2], g->m_touchY + m_a98[3]});
        if (m_aa8 < 0) return;
        QuadStrip& p = m_panels[3][static_cast<size_t>(m_abilityPanel)];
        float top = p.m_position.y;
        GLfloat2 size = p.GetSize();
        float t = Clamp((g->m_touchY - top) / ((top + size.y) - top), 0.0f, 1.0f);
        float s = Interpolate(0.5f, 1.0f, t);
        m_grabbedIcon.m_quads[0].m_scale = GLfloat2{s, s};
        m_grabbedSmallIcon.m_quads[0].m_scale = GLfloat2{s, s};
        return;
    }
    // ---- the release
    m_a74 = 0;
    if (m_a7c) {
        if (m_strips[10].ContainsPoint(touch)) FadeOut();
        m_strips[10].SetAlpha(0xff);
        m_a7c = false;
        return;
    }
    if (m_a7d) {
        if (m_strips[9].ContainsPoint(touch)) QueueTabHelp();
        m_strips[9].SetAlpha(0xff);
        m_a7d = false;
        return;
    }
    switch (m_tab) {
        case 0: {
            if (m_a78 < 0) return;
            QuadStrip& s = m_panels[0][static_cast<size_t>(m_a78)];
            s.SetAlpha(0xff);
            if (s.ContainsPoint(touch)) {
                int c = m_a78;
                if (g->m_classes[c].m_level == 0) {
                    g->m_currentClass = 0;
                    c = 0;
                } else {
                    g->m_currentClass = c;
                }
                g->m_nameScreen.SelectClass(g->m_classes[c]);
                g->m_currentClassChanged = true;
                UpdateClass();
            }
            m_a78 = -1;
            break;
        }
        case 1: {
            if (m_a78 < 0) return;
            QuadStrip& s = m_panels[1][static_cast<size_t>(m_a78)];
            s.SetAlpha(0xff);
            if (s.ContainsPoint(touch)) {
                cls.m_race = static_cast<RaceType>(m_a78);
                UpdateRace();
            }
            m_a78 = -1;
            break;
        }
        case 2: {
            if (m_a80 < 0) break;
            std::vector<QuadStrip>& v = m_panels[2];
            if (v[static_cast<size_t>(m_perkPanel2)].m_position.y < g->m_touchY &&
                g->m_touchY < v[static_cast<size_t>(m_perkPanel2 + 2)].m_position.y) {
                // drop into the nearest unlocked slot of its kind (perks 1..3, flaws 5..7) within 0.02,
                // else into the first free unlocked one
                const bool perk = m_a80 < 10;
                int best = 0;
                float bestDist = 1000000.0f;   // 0x497423f0
                for (int slot = 0; slot < 8; ++slot) {
                    if (perk ? slot > 3 : slot < 4) {
                        if (perk) break;
                        continue;
                    }
                    if ((slot & 3) == 0) continue;
                    if (!cls.IsPerkUnlocked(slot & 3)) break;
                    float d = DropDistance(v[static_cast<size_t>(m_perkIconsStart + slot)], g);
                    if (d < bestDist) {
                        bestDist = d;
                        best = slot;
                    }
                }
                bool dropped = false;
                if (bestDist <= 0.02f) {
                    if (perk) cls.SetPerk(best, static_cast<PerkType>(m_a80));
                    else cls.SetFlaw(best - 4, static_cast<FlawType>(m_a80 - FLAW_COUNT));
                    dropped = true;
                } else {
                    for (int slot = 1; slot < 4 && cls.IsPerkUnlocked(slot); ++slot) {
                        if (perk) {
                            if (cls.m_perks[slot] == PERK_NONE) {
                                cls.SetPerk(slot, static_cast<PerkType>(m_a80));
                                dropped = true;
                                break;
                            }
                        } else if (cls.m_flaws[slot] == FLAW_NONE) {
                            cls.SetFlaw(slot, static_cast<FlawType>(m_a80 - FLAW_COUNT));
                            dropped = true;
                            break;
                        }
                    }
                }
                if (dropped) g->QueueSound(SOUND_CLICK);
            }
            m_a80 = -1;
            UpdatePerks();
            break;
        }
        case 3: {
            if (m_aa8 < 0) return;
            std::vector<QuadStrip>& v = m_panels[3];
            if (m_strips[8].m_position.y < g->m_touchY && g->m_touchY < v[static_cast<size_t>(m_abilityPanel)].m_position.y) {
                // replace the nearest active skill (not the class skill, and within the allowance)
                int best = 0;
                float bestDist = 1000000.0f;
                for (int s = 0; s < 20; ++s) {
                    float d = DropDistance(v[static_cast<size_t>(s)], g);
                    if (d < bestDist) {
                        bestDist = d;
                        best = s;
                    }
                }
                const char* name = CharacterClass::GetClassName(cls.m_class);
                if (best == 0) {
                    std::snprintf(m_text, sizeof m_text, "You cannot replace the %s's class skill (%s)!", name,
                                  Skill::GetSkillName(cls.m_baseSkills[0]));
                    ShowText(nullptr, m_text, false);
                } else if (cls.m_replacementSkills[static_cast<size_t>(best)] == 0xff &&
                           cls.GetReplacementSkillsAllowed() <= cls.GetReplacementSkillCount()) {
                    int allowed = cls.GetReplacementSkillsAllowed();
                    if (allowed == 0) {
                        std::snprintf(m_text, sizeof m_text,
                                      "The %s's skills cannot be customised at the current level! Level up this class to gain the option to customise its skills",
                                      name);
                    } else if (allowed == 10) {
                        std::snprintf(m_text, sizeof m_text, "You can only replace up to %d of the %s's skills!", allowed, name);
                    } else {
                        std::snprintf(m_text, sizeof m_text,
                                      "You can only replace up to %d of the %s's skills! Level up this class to gain more replacement skills",
                                      allowed, name);
                    }
                    ShowText(nullptr, m_text, false);
                } else {
                    cls.SetReplacementSkill(best, cls.m_nonBaseSkills[static_cast<size_t>(m_aa8)]);
                }
                g->QueueSound(SOUND_CLICK);
            }
            m_aa8 = -1;
            UpdateAbilities();
            break;
        }
        default:
            break;
    }
}

// The help pages of the help button, per tab, pointing at their parts (HelpMenu::QueueHelp(text, x, y, w)).
void ClassScreen::QueueTabHelp() {
    HelpMenu& h = m_game->m_helpMenu;
    auto centreX = [](QuadStrip& s) { return s.m_position.x + s.GetSize().x * 0.5f; };
    auto bottom = [](QuadStrip& s) { return s.m_position.y + s.GetSize().y; };
    auto middle = [](QuadStrip& s) { return s.m_position.y + s.GetSize().y * 0.5f; };
    switch (m_tab) {
        case 0: {
            std::vector<QuadStrip>& v = m_panels[0];
            QuadStrip& p1 = v[static_cast<size_t>(m_classPanels + 1)];
            QuadStrip& p2 = v[static_cast<size_t>(m_classPanels + 2)];
            h.QueueHelp("This tab shows the currently selected class. A class is made up of a race, up to 4 perks and flaws and a set of skills",
                        centreX(m_strips[0]), bottom(m_strips[0]), 0.0f);
            h.QueueHelp("If you have other classes unlocked you can switch to them using these buttons. Kill special enemies on non-easy difficulty for a chance to unlock locked classes",
                        centreX(p2), bottom(p2) + 0.01f, 0.0f);
            h.QueueHelp("Killing special enemies gives you a chance to level up the class you're playing with. As a class levels it becomes more customisable. Tap these icons to see what the class gains for reaching a particular level",
                        centreX(p1), middle(p2), p2.GetSize().y);
            h.QueueHelp("Tap these tabs to view or customise aspects of the selected class (Adventurer class has no special properties and cannot be customised)",
                        centreX(m_strips[1]), bottom(m_strips[1]), 0.0f);
            h.QueueHelp("If all of this is confusing, DON'T PANIC! Play with the Adventurer class and ignore other classes & customisation until you're comfortable with the gameplay!",
                        centreX(v[0]), v[0].m_position.y, 0.0f);
            break;
        }
        case 1: {
            std::vector<QuadStrip>& v = m_panels[1];
            QuadStrip& r0 = v[static_cast<size_t>(m_racePanels)];
            QuadStrip& r1 = v[static_cast<size_t>(m_racePanels + 1)];
            QuadStrip& r2 = v[static_cast<size_t>(m_racePanels + 2)];
            h.QueueHelp("This tab shows the race used by the selected class. Races provide a passive bonus during the game, but also have a chance to encounter an extra-deadly Mortal Enemy!",
                        centreX(m_strips[1]), bottom(m_strips[1]), 0.0f);
            h.QueueHelp("This shows the special bonus that's always active when you're playing as this race",
                        centreX(r0), bottom(r0), 0.0f);
            h.QueueHelp("This shows the mortal enemy of the current race. These tougher versions of special enemies will sometimes appear while playing as this race and have extra abilities shown in red",
                        centreX(r1), middle(r1), r1.GetSize().y * 0.5f);
            h.QueueHelp("All your unlocked races are shown here. When this class reaches level 5, you will unlock its race and gain the option to choose another unlocked race for this class",
                        centreX(r2), bottom(r2) + 0.01f, 0.0f);
            break;
        }
        case 2: {
            std::vector<QuadStrip>& v = m_panels[2];
            QuadStrip& a = v[static_cast<size_t>(m_perkPanel2)];
            QuadStrip& b = v[static_cast<size_t>(m_perkPanel2 + 1)];
            QuadStrip& c = v[static_cast<size_t>(m_perkPanel3)];
            h.QueueHelp("This tab shows the perks and flaws of the selected class. Perks (blue) provide a minor benefit while flaws (red) incur a minor penalty during the game",
                        centreX(m_strips[2]), bottom(m_strips[2]), 0.0f);
            h.QueueHelp("Each class except the Adventurer starts with one fixed perk but can be customised with up to 3 other perks as it levels up",
                        centreX(a), middle(a), 0.0f);
            h.QueueHelp("Likewise, a class can have up to 4 flaws at level 9, and for each perk it must also have a flaw",
                        centreX(b), middle(b), 0.0f);
            h.QueueHelp("All your unlocked perks & flaws are shown here. Perks & flaws are unlocked when your unlock their corresponding class. If you have free perk & flaw slots, you can drag these icons into them",
                        centreX(c), bottom(c) + 0.01f, 0.0f);
            break;
        }
        case 3: {
            std::vector<QuadStrip>& v = m_panels[3];
            QuadStrip& p = v[static_cast<size_t>(m_abilityPanel)];
            h.QueueHelp("This tab shows the current class' special skill. You can choose its active skills on this screen",
                        centreX(m_strips[3]), bottom(m_strips[3]), 0.0f);
            h.QueueHelp("The big icons show the selected class' active skills. When your character levels up in a game, the skills you can choose are selected randomly from this list",
                        0.5f, 0.5f, 0.0f);
            h.QueueHelp("At higher class levels, some of the active skills can be replaced with inactive unlocked skills from this list",
                        centreX(p), bottom(p) + 0.01f, 0.0f);
            h.QueueHelp("The first skill is unique to this class and cannot be replaced. This skill is always offered the first time you level up in a game. At class level 10 you unlock this skill and can then use it with another class",
                        centreX(v[0]), bottom(v[0]), 0.0f);
            h.QueueHelp("The Adventurer class has no special skill and all unlocked skills count as its active skills, regardless of how they're displayed on this screen",
                        0.5f, 0.5f, 0.0f);
            break;
        }
        default:
            break;
    }
}

// Shade and the other tabs, the shade strip over them, the selected tab, bars and buttons, the tab's
// list (with texture switches: the race's enemy picture, the skill icons; the perk/ability tabs'
// inset icon only while their text has one), the level numbers, texts, and the dragged icon.
void ClassScreen::Render() {
    Game* g = m_game;
    sdlDisable(GL_TEXTURE_2D);
    m_strips[7].Render();
    sdlEnable(GL_TEXTURE_2D);
    for (int t = 0; t < 4; ++t) {
        if (m_tab != t) RenderTab(t);
    }
    sdlDisable(GL_TEXTURE_2D);
    m_strips[6].Render();
    sdlEnable(GL_TEXTURE_2D);
    RenderTab(m_tab);
    sdlBindTexture(GL_TEXTURE_2D, 4);
    m_strips[5].Render();
    m_strips[4].Render();
    m_strips[8].Render();
    m_strips[10].Render();
    m_strips[9].Render();
    std::vector<QuadStrip>& v = m_panels[m_tab];
    const unsigned count = static_cast<unsigned>(v.size());
    for (unsigned u = 0; u < count; ++u) {
        if (m_tab == 3) {
            if (u == 42) {
                sdlBindTexture(GL_TEXTURE_2D, 1);
            } else if (static_cast<unsigned>(m_abilityPanel) == u) {
                sdlBindTexture(GL_TEXTURE_2D, 4);
            } else if (static_cast<unsigned>(m_abilityPanel) + 1 == u) {
                if (!m_abilityIcon) break;
                sdlBindTexture(GL_TEXTURE_2D, 1);
            }
        } else if (m_tab == 2) {
            if (!m_perkIcon && u == count - 1) break;
        } else if (m_tab == 1 && static_cast<unsigned>(m_raceIcons) + 1 == u) {
            sdlBindTexture(GL_TEXTURE_2D, 1);
        }
        v[u].Render();
    }
    const int first = g->m_classes[g->m_currentClass].m_class == CLASS_ADVENTURER ? 1 : 0;
    const int last = m_tab == 0 ? static_cast<int>(m_levelLabels.size()) : 1;
    for (int i = first; i < last; ++i) m_levelLabels[static_cast<size_t>(i)].Render();
    for (int i = 0; i < m_textLabelCount; ++i) m_textLabels[static_cast<size_t>(i)].Render();
    if (m_tab == 1) {
        for (int i = 0; i < m_raceLabelCount; ++i) m_raceLabels[static_cast<size_t>(i)].Render();
    } else if (m_tab == 0) {
        for (Label& l : m_classLabels) l.Render();
    }
    if (m_a80 >= 0 || m_aa8 >= 0) {
        sdlBindTexture(GL_TEXTURE_2D, 4);
        m_grabbedIcon.Render();
        if (m_aa8 >= 0) sdlBindTexture(GL_TEXTURE_2D, 1);
        m_grabbedSmallIcon.Render();
    }
}
