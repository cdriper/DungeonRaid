// Game, slice 4f: the UI part (Render, BackPressed, SetupRenderer, the main menu's entry points
// NewGame/ContinueGame/StartChallenge/StartTutorial, fonts). Start and the constructor are in
// game.cpp. Control flow of Render and SetupRenderer checked in the disassembly (the decompiler
// garbles Render's screen branches).
#include <cmath>

#include "dr_gl.h"
#include "game.h"

// ================================================================ renderer

// A 2:3 viewport as large as fits, centred: wider views get bars left and right, taller ones above
// and below (also NaN ratios: bge is false for unordered). An exact 2:3 view (320 x 480) gives the
// same viewport either way (320 / 0.6666667f rounds to 480). Projection: x 0..1, y 0..1.5 downwards.
// Port: on the tall canvas the viewport is 9:16 and the projection y 0..kTallCanvasHeight (Render
// moves it for the centred screens); the horizontal layout's canvas is kWideW x kWideH (Render sets the
// projection of each pass).
void Game::SetupRenderer() {
    const float aspect = CanvasAspect();   // the original: 0.6666667f (0x3f2aaaab)
    int x, y, w, h;
    if (s_viewWidth / s_viewHeight >= aspect) {
        h = static_cast<int>(s_viewHeight);
        y = 0;
        w = static_cast<int>(static_cast<float>(h) * aspect);
        x = static_cast<int>((s_viewWidth - static_cast<float>(w)) * 0.5f);
    } else {
        w = static_cast<int>(s_viewWidth);
        x = 0;
        h = static_cast<int>(static_cast<float>(w) / aspect);
        y = static_cast<int>((s_viewHeight - static_cast<float>(h)) * 0.5f);
    }
    m_viewportX = x;
    m_viewportY = y;
    m_viewportWidth = w;
    m_viewportHeight = h;
    sdlViewport(x, y, w, h);
    sdlMatrixMode(GL_PROJECTION);
    sdlLoadIdentity();
    sdlOrthof(0.0f, 1.0f, m_tallCanvas ? kTallCanvasHeight : 1.5f, 0.0f, -10.0f, 10.0f);
    m_projectionOffsetY = IsWide() ? -1.0f : 0.0f;   // (wide: Render sets the projections)
    sdlMatrixMode(GL_MODELVIEW);
    sdlEnable(GL_TEXTURE_2D);
    sdlEnable(GL_BLEND);
    sdlBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    sdlEnableClientState(GL_VERTEX_ARRAY);
    sdlEnableClientState(GL_COLOR_ARRAY);
    sdlEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

// Port-only (not in the original, whose view never changes size after the Game is made): desktop
// hosts call it when the window is resized. The view size is only used by SetupRenderer, so this
// changes the viewport and GL state, not the game state.
void Game::SetViewSize(float viewWidth, float viewHeight) {
    s_viewWidth = viewWidth;
    s_viewHeight = viewHeight;
    SetupRenderer();
}

// ================================================================ port: the layouts

float Game::CanvasAspect() const {
    if (IsWide()) return kWideW / kWideH;
    return m_tallCanvas ? 0.5625f : 0.6666667f;
}

void Game::CanvasSize(float& w, float& h) const {
    if (IsWide()) {
        w = kWideW;
        h = kWideH;
    } else {
        w = 1.0f;
        h = m_tallCanvas ? kTallCanvasHeight : 1.5f;
    }
}

void Game::SetFullscreen(bool on) {
    m_fullscreenChanged = m_fullscreenChanged || on != m_fullscreen;
    m_fullscreen = on;
}

// What is over the board (port): the one place that lists it, for the layouts, the info panel and
// the host's back key. A new menu or screen over the board goes here.
bool Game::HudMenuOpen() const {
    return m_interface.m_foldoutMenu.m_active || m_interface.m_gameMenu.m_active || m_interface.m_statsMenu.m_active;
}

bool Game::ScreenOverBoard() const {
    return m_settingsScreen.m_active || m_achievementsScreen.m_active || m_helpMenu.m_active || m_unlockMenu.m_active ||
           m_gameOverScreen.m_active;
}

void Game::SetProjection(float left, float right, float bottom, float top) {
    sdlMatrixMode(GL_PROJECTION);
    sdlLoadIdentity();
    sdlOrthof(left, right, bottom, top, -10.0f, 10.0f);
    sdlMatrixMode(GL_MODELVIEW);
}

// x: 0..1 across the viewport; y: viewport widths / 1.5 (Host::ToViewport), so y * 1.5 is in
// viewport widths. The original: (x, y * 1.5). The tall canvas: the screen's offset taken off.
// Wide (the canvas is kWideW units wide): LAYOUT_SPLIT maps the left column to the board (also
// past its edges, as a drag goes on), the right one to the skills bar, the HUD or the info panel
// (y >= 1.5), whichever card the point is in or nearest to; LAYOUT_FIT undoes the centred screen.
void Game::MapPointer(float x, float y, float& gx, float& gy) const {
    if (m_layout == LAYOUT_SPLIT) {
        const float cx = x * kWideW, cy = y * 1.5f * kWideW;
        if (cx < kWideColW) {
            gx = cx - kFrameL;
            gy = cy - kFrameT + kWideBoardTop;
            return;
        }
        gx = cx - kWideColW - kFrameL;
        if (cy < kFrameT + kWideSkillsH) {
            gy = cy - kFrameT;   // the skills bar
        } else if (cy < kWideCardH) {
            gy = cy - kFrameT - kWideSkillsH + kSplitY;   // the HUD
        } else {
            gy = cy - kWideCardH - kFrameT + 1.5f;   // the info panel
        }
        return;
    }
    if (m_layout == LAYOUT_FIT) {
        gx = m_fitLeft + x * m_fitWidth;
        gy = y * 1.5f * kWideW * (1.5f / kWideH);
        return;
    }
    gx = x;
    gy = y * 1.5f - m_screenOffsetY;
}

// The viewport for the canvas rectangle (x, y, w, h) (canvas units, y down).
void Game::SetCanvasViewport(float x, float y, float w, float h) {
    const float sx = static_cast<float>(m_viewportWidth) / kWideW, sy = static_cast<float>(m_viewportHeight) / kWideH;
    const int px = m_viewportX + static_cast<int>(x * sx + 0.5f);
    const int pw = m_viewportX + static_cast<int>((x + w) * sx + 0.5f) - px;
    const int top = static_cast<int>(y * sy + 0.5f), bottom = static_cast<int>((y + h) * sy + 0.5f);
    sdlViewport(px, m_viewportY + m_viewportHeight - bottom, pw, bottom - top);   // (GL: from the bottom)
}

// The view for drawing the original screen's band y gy0..gy1 (all of its width) into the canvas at
// (cx, cy), at the same scale; the viewport cuts everything else.
void Game::SetPartView(float cx, float cy, float gy0, float gy1) {
    SetCanvasViewport(cx, cy, 1.0f, gy1 - gy0);
    SetProjection(0.0f, 1.0f, gy1, gy0);
    sdlLoadIdentity();
}

// The pause menu's frame (GameMenu's pieces: 13-pixel left and top, 15-pixel right and bottom
// sides, texture 1) around the outer rectangle (x, y, w, h), in canvas units; the middle stays open.
// Each piece samples half a texel inside its atlas rectangle: drawn scaled with linear filtering,
// it would otherwise blend in the atlas's neighbouring pixels (seams at the corners). The
// bottom-right piece is 13 pixels tall (no transparent rows below its border, unlike the bottom
// edge), so it is top-aligned with the bottom side, as GameMenu places it.
void Game::RenderFrame(float x, float y, float w, float h) {
    const float px = 1.0f / 320.0f, uv = 1.0f / 1024.0f;
    auto piece = [&](float u, float v, float uw, float vh, float qx, float qy, float qw, float qh) {
        m_framePiece.SetUV(GLfloat2{(u + 0.5f) * uv, (v + 0.5f) * uv},
                           GLfloat2{(u + uw - 0.5f) * uv, (v + vh - 0.5f) * uv});
        m_framePiece.SetSize(GLfloat2{qw, qh});
        m_framePiece.m_position = GLfloat2{qx + qw * 0.5f, qy + qh * 0.5f};
        sdlLoadIdentity();
        m_framePiece.Render();
    };
    const float l = 13.0f * px, r = 15.0f * px, t = 13.0f * px, b = 15.0f * px;
    sdlBindTexture(GL_TEXTURE_2D, 1);
    piece(498.0f, 498.0f, 13.0f, 13.0f, x, y, l, t);                                 // top left
    piece(479.0f, 472.0f, 1.0f, 13.0f, x + l, y, w - l - r, t);                       // top
    piece(380.0f, 415.0f, 15.0f, 13.0f, x + w - r, y, r, t);                          // top right
    piece(478.0f, 487.0f, 13.0f, 1.0f, x, y + t, l, h - t - b);                       // left
    piece(380.0f, 430.0f, 15.0f, 1.0f, x + w - r, y + t, r, h - t - b);               // right
    piece(498.0f, 482.0f, 13.0f, 15.0f, x, y + h - b, l, b);                          // bottom left
    piece(483.0f, 470.0f, 1.0f, 15.0f, x + l, y + h - b, w - l - r, b);               // bottom
    piece(380.0f, 433.0f, 15.0f, 13.0f, x + w - r, y + h - b, r, 13.0f * px);         // bottom right
}

// ================================================================ fonts

void Game::SetFonts(const Font& font, const Font& menuFont, const Font& countFont) {
    m_fonts[0] = font;
    m_fonts[1] = menuFont;
    m_fonts[1].UseSmallCaps(0.75f);
    m_fonts[1].m_spacing = -0.225f;    // 0xbe666666
    m_fonts[2] = countFont;
    m_fonts[2].UseSmallCaps(0.75f);
    m_fonts[2].m_spacing = -0.05f;     // 0xbd4ccccd
}

void Game::CreateFont(const char* font, const char* menuFont, const char* countFont) {
    m_fonts[0].LoadFont(font, TEX_FONT);
    m_fonts[1].LoadFont(menuFont, TEX_FONT_MENU);
    m_fonts[1].UseSmallCaps(0.75f);
    m_fonts[1].m_spacing = -0.225f;
    m_fonts[2].LoadFont(countFont, TEX_COUNT);
    m_fonts[2].UseSmallCaps(0.75f);
    m_fonts[2].m_spacing = -0.05f;
}

// ================================================================ main menu entry points

void Game::NewGame() {
    m_challenge = false;
    InitNewGame();
    m_nameScreen.Show(m_name, false);
    m_mainMenu.SetActive(false);
    SetHelpState(false);
    m_helpMenu.ClearHelp();
    m_interface.Process(0.0f, false);
}

void Game::StartChallenge() {
    m_challenge = true;
    InitNewGame();
    m_nameScreen.Show(m_name, true);
    m_mainMenu.SetActive(false);
    SetHelpState(false);
    m_helpMenu.ClearHelp();
    m_interface.Process(0.0f, false);
}

// The tutorial is a normal game on Easy with help on and every help page reset.
void Game::StartTutorial() {
    m_challenge = false;
    InitNewGame();
    m_difficulty = DIFF_EASY;
    m_nameScreen.Show(m_name, false);
    m_mainMenu.SetActive(false);
    SetHelpState(true);
    m_helpMenu.ResetHelp();
    m_interface.Process(0.0f, false);
}

void Game::ContinueGame() {
    if (m_saveState.GetStateSize() == 0) return;
    InitNewGame();
    DeserialiseState();
    CalculateTilePositions();
    m_mainMenu.FadeOut();
    m_interface.Process(0.0f, false);
}

void Game::ShowBonusReceived() {
    m_unlockMenu.ShowPromo("Ability unlocked: Hack!",
                           "You've unlocked the Hack ability! Hack creates a column of swords when used, and "
                           "collects the whole board if used on the same turn as Slash.");
    m_mainMenu.m_bonusVisible = false;
}

// ================================================================ back key

// Drops the queued help texts and closes help, then goes back one level: class screen -> name
// screen, mode screen -> name screen, name screen -> main menu, score screen / game over -> main
// menu; during a game (no menu open) the main menu fades in. The intro screen ignores it.
void Game::BackPressed() {
    if (m_settingsScreen.m_active) {   // port: back closes the settings first
        m_settingsScreen.Back();   // the import dialog first
        return;
    }
    if (m_achievementsScreen.m_active) {   // port
        m_achievementsScreen.FadeOut();
        return;
    }
    m_helpMenu.m_queue.clear();
    m_helpMenu.ClearHelp();
    m_helpMenu.FadeOut();
    if (m_nameScreen.m_active) {
        if (m_nameScreen.m_classScreen.m_active) {
            FadeOutClassScreen();
        } else if (m_nameScreen.m_modeScreen.m_active) {
            m_nameScreen.m_modeScreen.FadeOut();
        } else {
            m_mainMenu.Show();
            m_mainMenu.SetActive(true);
            m_nameScreen.FadeOut();
        }
        return;
    }
    if (m_introScreen.m_active) return;
    if (m_scoreScreen.m_active) {
        m_mainMenu.Show();
        m_mainMenu.SetActive(true);
        m_scoreScreen.FadeOut();
    } else if (m_gameOverScreen.m_active) {
        m_mainMenu.Show();
        m_mainMenu.SetActive(true);
        m_gameOverScreen.m_active = false;
    } else if (!m_interface.m_foldoutMenu.m_active && !m_interface.m_gameMenu.m_active &&
               !m_interface.m_statsMenu.m_active && !m_mainMenu.m_active) {
        m_mainMenu.FadeIn();
    }
}

// Port (desktop Esc on the board, instead of the original back's return to the title screen): an
// open help page closes first; then the pause menu opens, or closes when open (the stats menu
// closes too). The fold-out menus (a choice to make) and the unlock page stay. It opens only when
// the HUD's menu button works: between turns, no chain.
void Game::TogglePauseMenu() {
    if (m_helpMenu.m_active) {
        m_helpMenu.m_queue.clear();
        m_helpMenu.ClearHelp();
        m_helpMenu.FadeOut();
        return;
    }
    if (m_interface.m_foldoutMenu.m_active || m_unlockMenu.m_active) return;
    GameMenu& pause = m_interface.m_gameMenu;
    if (pause.m_active) {
        if (!pause.m_closing) {   // as its Help button closes it
            QueueSound(SOUND_CLICK);
            pause.m_fadeTime = 0.2f;
            pause.m_closing = true;
        }
        return;
    }
    StatsMenu& stats = m_interface.m_statsMenu;
    if (stats.m_active) {
        if (!stats.m_closing) {   // as a press outside it closes it
            QueueSound(SOUND_CLICK);
            stats.m_closing = true;
            stats.m_fadeTime = 0.2f;
        }
        return;
    }
    if ((m_inputReady & 0xff) == 0 || m_touchState != NOT_TOUCHING || !m_chain.empty()) return;
    QueueSound(SOUND_CLICK);
    pause.Show();
}

// ================================================================ frame

namespace {
void BindTexture(unsigned texture) { sdlBindTexture(GL_TEXTURE_2D, texture); }
}  // namespace

// The menus drawn over the board or the name screen: the main menu and name screen unless the
// intro has started closing (m_closing), then the intro itself.
void Game::Render() {
    sdlClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    sdlClear(GL_COLOR_BUFFER_BIT);
    if (IsWide()) {   // port: the horizontal layout
        m_screenOffsetY = 0.0f;
        m_projectionOffsetY = -1.0f;
        // The original screen (1 x 1.5) scaled to the canvas's height, centred on the canvas: where
        // the other screens and the menus over the board are drawn (game x 0..1 shows on canvas x
        // (kWideW - s) / 2 .. (kWideW + s) / 2, s = kWideH / 1.5).
        const float s = kWideH / 1.5f;
        m_fitWidth = kWideW / s;
        m_fitLeft = (1.0f - m_fitWidth) * 0.5f;
        // the board also stays split under a screen fading over it (a game starting or being left),
        // so it does not jump from the centred original layout when the fade ends
        const bool board = IsBoardScreen() || BoardUnderFade();
        if (board) {
            // the board stays split under any menu. Each part in its own viewport, which cuts the
            // rest (also the original's quads that reach past the screen, as the 2 x 2 attack
            // shade): the board on the left; on the right the skills bar, the HUD under it, the
            // info panel (y 1.5 ..) under them; then the frames; then the gliding tokens and labels;
            // then the menus
            m_layout = OverlayOpen() ? LAYOUT_FIT : LAYOUT_SPLIT;   // (where the touches go)
            const float rx = kWideColW + kFrameL;
            m_splitPass = true;
            SetPartView(kFrameL, kFrameT, kWideBoardTop, kSplitY);
            RenderBoardScreen();
            SetPartView(rx, kFrameT, 0.0f, kWideSkillsH);
            RenderBoardScreen();
            SetPartView(rx, kFrameT + kWideSkillsH, kSplitY, 1.5f);
            RenderBoardScreen();
            m_splitPass = false;
            SetPartView(rx, kWideCardH + kFrameT, 1.5f, 1.5f + kWidePanelH);
            m_infoPanel.Render();   // (only the panel is there: built and laid out once a frame)
            sdlViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
            SetProjection(0.0f, kWideW, kWideH, 0.0f);
            RenderFrame(0.0f, 0.0f, kWideColW, kWideH);
            RenderFrame(kWideColW, 0.0f, kWideColW, kWideCardH);
            RenderFrame(kWideColW, kWideCardH, kWideColW, kWideH - kWideCardH);
            sdlLoadIdentity();
            RenderFlying();
        } else {
            m_layout = LAYOUT_FIT;   // the other screens
        }
        // the menus (or the other screens) in the centred screen only (a 2:3 part of the viewport,
        // so the original's oversized shades stay inside it); the window's sides get the same
        // shades (colour and alpha as they fade), so the dimming is even across the window
        const float vw = static_cast<float>(m_viewportWidth);
        const int screenX = m_viewportX + static_cast<int>(vw * (kWideW - s) * 0.5f / kWideW + 0.5f);
        const int screenW = static_cast<int>(vw * s / kWideW + 0.5f);
        if (board) RenderSideShades(screenX, screenW);
        sdlViewport(screenX, m_viewportY, screenW, m_viewportHeight);
        SetProjection(0.0f, 1.0f, 1.5f, 0.0f);
        sdlLoadIdentity();
        if (board) {
            RenderOverlays();
        } else {
            RenderScreens();
        }
        sdlViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
        return;
    }
    m_layout = LAYOUT_VERTICAL;
    if (m_tallCanvas && !IsBoardScreen()) {
        // port: a title / intro screen fading over the board: the board and the info panel already
        // (or still) where the board screen has them, so nothing jumps when the fade ends; the
        // fading screen over them, centred, as below
        const bool fade = BoardUnderFade();
        if (fade) {
            SetBoardProjection();
            sdlLoadIdentity();
            RenderBoardScreen();
            m_infoPanel.Render();
        }
        // port: the other screens, centred, drawn into their 1 x 1.5 part of the viewport only: the
        // original draws things past its screen (backgrounds reaching above it, the name screen's
        // keyboard parked below it), which the taller canvas would show
        m_screenOffsetY = kPanelHeight * 0.5f;
        m_projectionOffsetY = -1.0f;   // (the board screen sets its projection again)
        const float vh = static_cast<float>(m_viewportHeight);
        const int bottom = static_cast<int>(vh * m_screenOffsetY / kTallCanvasHeight + 0.5f);
        const int height = static_cast<int>(vh * 1.5f / kTallCanvasHeight + 0.5f);
        if (fade) RenderSideShades(m_viewportY + bottom, height, true);   // the fade also over the strips
        sdlViewport(m_viewportX, m_viewportY + bottom, m_viewportWidth, height);
        SetProjection(0.0f, 1.0f, 1.5f, 0.0f);
        sdlLoadIdentity();
        if (fade) {
            RenderOverlays();   // (the board is drawn)
        } else {
            RenderScreens();
        }
        sdlViewport(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight);
        return;
    }
    if (m_tallCanvas) {
        SetBoardProjection();
    } else {
        m_screenOffsetY = 0.0f;   // (Info panel None: the original screen, also after the tall canvas)
    }
    sdlLoadIdentity();
    RenderScreens();
}

// Port: the board below the info panel (or above it), m_screenOffsetY down the tall canvas.
void Game::SetBoardProjection() {
    m_screenOffsetY = m_panelAtBottom ? 0.0f : kPanelHeight;
    if (m_projectionOffsetY != m_screenOffsetY) {
        m_projectionOffsetY = m_screenOffsetY;
        sdlMatrixMode(GL_PROJECTION);
        sdlLoadIdentity();
        sdlOrthof(0.0f, 1.0f, kTallCanvasHeight - m_screenOffsetY, -m_screenOffsetY, -10.0f, 10.0f);
        sdlMatrixMode(GL_MODELVIEW);
    }
}

// The screen being shown: the score page, the name/class screens, else the board and what is over
// it. (Port: split out of Render.)
void Game::RenderScreens() {
    if (m_scoreScreen.m_active) {
        if (m_scoreScreen.m_fadeTime > 0.0f) m_mainMenu.Render();   // sliding in over the main menu
        m_scoreScreen.Render();
        if (m_gameOverScreen.m_active) m_gameOverScreen.Render();
        m_toast.Render();   // port
        return;
    }
    if (m_nameScreen.m_active) {
        if (!m_introScreen.m_closing) {
            m_mainMenu.Render();
            m_nameScreen.Render();
        }
        if (m_introScreen.m_active) m_introScreen.Render();
        if (m_unlockMenu.m_active) m_unlockMenu.Render();
        if (m_helpMenu.m_active) m_helpMenu.Render();
        return;
    }
    RenderBoardScreen();
    RenderOverlays();
}

// The board screen: tiles, HUD, attack banners, arrow, tokens, effects, labels. (Port: split out of
// Render; on the horizontal layout it runs once per card with m_splitPass set: the tokens and the
// flying labels then wait for RenderFlying. The info panel is drawn by RenderOverlays, or by Render
// in its own card.)
void Game::RenderBoardScreen() {
    BindTexture(1);
    for (auto& row : m_board) {
        for (Tile& tile : row) tile.Render();
    }
    for (int i = 0; i < m_runeMarkerCount; ++i) m_runeMarkers[i].Render();
    BindTexture(2);
    for (auto& row : m_board) {
        for (Tile& tile : row) tile.RenderText();
    }
    BindTexture(1);
    for (auto& row : m_board) {
        for (Tile& tile : row) {
            tile.RenderOverlay();
            tile.RenderCross();
        }
    }
    if (static_cast<unsigned>(m_attackPhase - 1) < 7) {   // phases 1..7
        sdlDisable(GL_TEXTURE_2D);
        if (m_attackDimmingEnabled) m_attackShade.Render();
        sdlEnable(GL_TEXTURE_2D);
    }
    BindTexture(1);
    m_interface.Render();
    if (m_flareCount > 0) {
        BindTexture(1);
        for (int i = 0; i < m_flareCount; ++i) m_flares[i].Render();
    }
    if (static_cast<unsigned>(m_attackPhase - 3) < 5) {   // phases 3..7: the banners
        BindTexture(1);
        m_attackBanner.Render();
        m_counterBanner.Render();
        if (m_attackPhase < 7 && !m_counterActive && !m_splitPass) {
            m_incomingDamageLabel.Render();
            if (m_incomingPoison > 0) m_incomingPoisonLabel.Render();
        }
    }
    // port: with the mouse the arrow also runs from the last tile to the pointer while dragging
    // (cosmetic; the chain itself still moves tile by tile in ProcessTileSelection)
    bool live = m_touchIsMouse && m_touchState == TOUCHING && !m_chain.empty() && m_attackPhase == 0;
    bool drawArrow = m_chain.size() > 1;
    if (live) {
        drawArrow = m_arrow.UpdateLive(m_chain, GLfloat2{m_touchX, m_touchY});
        m_arrowLive = true;
    } else if (m_arrowLive) {   // back to the original arrow
        m_arrowLive = false;
        if (m_chain.size() > 1) UpdateArrow();
    }
    if (drawArrow) {
        BindTexture(1);
        m_arrow.Render();
    }
    if (live) m_arrow.RenderNodes(m_chain);   // port: a node on each selected tile
    if (m_tokenCount > 0 && !m_splitPass) {
        sdlLoadIdentity();
        BindTexture(1);
        for (int i = 0; i < m_tokenCount; ++i) {
            if (m_tokens[i].m_alive) m_tokens[i].Render();
        }
    }
    if (m_particleCount > 0) {
        sdlLoadIdentity();
        BindTexture(1);
        for (int i = 0; i < m_particleCount; ++i) {
            if (m_particles[i].m_active) m_particles[i].Render();
        }
    }
    if (m_crossLabelCount > 0) {
        sdlLoadIdentity();
        BindTexture(1);
        for (int i = 0; i < m_crossLabelCount; ++i) m_crossLabels[i].Render();
    }
    if (m_simpleLabelCount > 0) {
        sdlLoadIdentity();
        BindTexture(1);
        for (int i = 0; i < m_simpleLabelCount; ++i) {
            if (m_simpleLabels[i].m_active) m_simpleLabels[i].Render();
        }
    }
    if (m_shieldShatterActive) {
        sdlLoadIdentity();
        BindTexture(1);
        m_shieldShatterLeft.Render();
        m_shieldShatterRight.Render();
    }
    if (m_chainDamage > 0 || m_simpleLabelB904.m_active) {
        sdlLoadIdentity();
        BindTexture(1);
        m_simpleLabelB904.Render();
    }
    if (m_lifeLeechHeal > 0 && !m_splitPass) {
        sdlLoadIdentity();
        BindTexture(1);
        m_simpleLabelC388.Render();
    }
    if (m_chainBurnDamage > 0 && !m_splitPass) {
        sdlLoadIdentity();
        BindTexture(1);
        m_simpleLabel14344.Render();
    }
    if (m_spikyDamage > 0 && !m_splitPass) {
        sdlLoadIdentity();
        BindTexture(1);
        m_simpleLabel146b8.Render();
    }
}

// What is drawn over the board: the HUD's menus (fold-out, game, stats) and the tooltip, the main
// menu or intro, the unlock and help pages, game over, the settings, the toast. (Port: split out of
// Render; the horizontal layout draws it in the left column, over the split board.)
void Game::RenderOverlays() {
    if (m_interface.m_foldoutMenu.m_active || m_interface.m_gameMenu.m_active || m_interface.m_statsMenu.m_active) {
        sdlLoadIdentity();
        BindTexture(1);
        m_interface.RenderMenu();
    }
    // port: on the tall canvas the info panel replaces the board's tooltip bubbles (special enemy,
    // skill); the level-up menu's scroll tooltip stays. Drawing only: the tooltip still runs.
    if (!m_tallCanvas || m_interface.m_foldoutMenu.m_active) m_interface.RenderTooltip();
    // port: the tall canvas's info panel, over the HUD's menus (their shades leave it undimmed),
    // under the other screens; the horizontal layout draws it in its own card (Render)
    // (under a fade Render draws it with the board, outside the centred screen)
    if (m_layout == LAYOUT_VERTICAL && IsBoardScreen()) m_infoPanel.Render();
    if (m_mainMenu.m_active || m_introScreen.m_active) {
        if (!m_introScreen.m_closing) {
            m_mainMenu.Render();
            if (m_introScreen.m_active) m_nameScreen.Render();
        }
        if (m_introScreen.m_active) m_introScreen.Render();
    }
    if (m_unlockMenu.m_active) m_unlockMenu.Render();
    if (m_helpMenu.m_active) m_helpMenu.Render();
    if (m_gameOverScreen.m_active) m_gameOverScreen.Render();
    if (m_achievementsScreen.m_active) {   // port: over the title screen
        sdlLoadIdentity();
        m_achievementsScreen.Render();
    }
    if (m_settingsScreen.m_active) {   // port: on top of everything
        sdlLoadIdentity();
        m_settingsScreen.Render();
    }
    m_toast.Render();   // port: above that
}

// The horizontal layout with a menu over the board: the window outside the centred screen
// (screenX, screenW: its part of the viewport) gets what the menus' shades put over the screen, in
// their drawing order (RenderOverlays): the fold-out, game or stats menu's, game over's, the settings'
// background. `vertical`: the tall canvas under a fade, the strips above and below the centred
// screen (screenX, screenW are then its y and height in the viewport).
void Game::RenderSideShades(int screenX, int screenW, bool vertical) {
    GLcolour4 shades[5] = {};
    int count = 0;
    if (m_interface.m_foldoutMenu.m_active) {
        shades[count++] = m_interface.m_foldoutMenu.m_shade.m_vertices[0].colour;
    } else if (m_interface.m_gameMenu.m_active) {
        shades[count++] = m_interface.m_gameMenu.m_shade.m_vertices[0].colour;
    } else if (m_interface.m_statsMenu.m_active) {
        shades[count++] = m_interface.m_statsMenu.m_shade.m_vertices[0].colour;
    }
    if (m_gameOverScreen.m_active) shades[count++] = m_gameOverScreen.m_shade.m_vertices[0].colour;
    if (m_settingsScreen.m_active) shades[count++] = m_settingsScreen.Background().m_vertices[0].colour;
    // a fading title screen (its picture's alpha) or intro (its black shade): black beside it
    if (m_mainMenu.m_active) shades[count++] = GLcolour4{0, 0, 0, m_mainMenu.m_background.m_vertices[0].colour.a};
    if (m_introScreen.m_active) shades[count++] = m_introScreen.m_shade.m_vertices[0].colour;
    if (count == 0) return;
    const int start = vertical ? m_viewportY : m_viewportX;
    const int length = vertical ? m_viewportHeight : m_viewportWidth;
    const int end = screenX + screenW;
    const int sides[2][2] = {{start, screenX - start}, {end, start + length - end}};
    m_wideShade.SetSize(GLfloat2{1.0f, 1.0f});
    m_wideShade.m_position = GLfloat2{0.5f, 0.5f};
    sdlDisable(GL_TEXTURE_2D);
    for (const auto& side : sides) {
        if (side[1] <= 0) continue;
        if (vertical) {
            sdlViewport(m_viewportX, side[0], m_viewportWidth, side[1]);
        } else {
            sdlViewport(side[0], m_viewportY, side[1], m_viewportHeight);
        }
        SetProjection(0.0f, 1.0f, 1.0f, 0.0f);
        for (int i = 0; i < count; ++i) {
            m_wideShade.SetColour(shades[i]);
            sdlLoadIdentity();
            m_wideShade.Render();
        }
    }
    sdlEnable(GL_TEXTURE_2D);
}

// LAYOUT_SPLIT, on the whole canvas (x 0..kWideW, y 0..kWideH): the tokens and the labels that fly
// to the HUD or a skill button, each drawn between where its point is in the board card and where
// it is in the card of its target (the HUD's, or the skills bar's), by how far its flight has got:
// a token by its distance left to the target (tokens to a tile stay on the board), the chain labels
// by their flight time, the attack's damage label by its flight time (from the HUD already when it
// starts there). The game's positions are put back after each draw.
void Game::RenderFlying() {
    // the board card, the skills bar and the HUD: canvas = game + offset
    const GLfloat2 board{kFrameL, kFrameT - kWideBoardTop};
    const GLfloat2 skills{kWideColW + kFrameL, kFrameT};
    const GLfloat2 hud{kWideColW + kFrameL, kFrameT + kWideSkillsH - kSplitY};
    auto glide = [&](const GLfloat2& p, float w, const GLfloat2& to) {
        w = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
        return GLfloat2{p.x + board.x + (to.x - board.x) * w, p.y + board.y + (to.y - board.y) * w};
    };
    BindTexture(1);
    for (int i = 0; i < m_tokenCount; ++i) {
        Token& t = m_tokens[i];
        if (!t.m_alive) continue;
        float w = 0.0f;
        GLfloat2 to = board;
        if (t.m_target == nullptr) {   // to a skill button or a HUD target (Token::Process's choice)
            const GLfloat2 target = t.m_kind == 1 ? Interface::GetSpecialTarget(t.m_value)
                                                  : *m_interface.GetTarget(t.m_kind == 3 ? TILE_POTION : t.m_type);
            if (target.y < kWideBoardTop) to = skills;
            if (target.y >= kSplitY) to = hud;
            if (t.m_distance > 0.0f) {
                const float dx = target.x - t.m_position.x, dy = target.y - t.m_position.y;
                w = 1.0f - std::sqrt(dx * dx + dy * dy) / t.m_distance;
            }
        }
        const GLfloat2 saved = t.m_position;
        t.m_position = glide(saved, w, to);
        sdlLoadIdentity();
        t.Render();
        t.m_position = saved;
    }
    auto label = [&](SimpleLabel& l, float w) {   // (these fly to the health bar or the armour)
        const GLfloat2 saved = l.m_position;
        l.m_position = glide(saved, w, hud);
        sdlLoadIdentity();
        l.Render();
        l.m_position = saved;
    };
    if (m_lifeLeechHeal > 0) label(m_simpleLabelC388, m_leechLabelT);
    if (m_chainBurnDamage > 0) label(m_simpleLabel14344, m_burnLabelT);
    if (m_spikyDamage > 0) label(m_simpleLabel146b8, m_spikyLabelT);
    if (static_cast<unsigned>(m_attackPhase - 3) < 4 && !m_counterActive) {   // phases 3..6
        float w = 0.0f;
        if (m_attackPhase >= 5) w = m_damageLabelStart.y >= kSplitY ? 1.0f : m_damageLabelT;
        label(m_incomingDamageLabel, w);
        if (m_incomingPoison > 0) label(m_incomingPoisonLabel, w);
    }
}

// The score page and the class screen (small wrappers of the original's).
void Game::ShowClassScreen() { m_nameScreen.m_classScreen.Initialise(); }
void Game::FadeOutClassScreen() { m_nameScreen.m_classScreen.FadeOut(); }
void Game::ShowScores() {
    m_scoreScreen.Show(m_scorePage, false);
    m_mainMenu.SetActive(false);
}
