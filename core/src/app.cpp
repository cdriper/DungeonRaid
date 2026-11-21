#include "app.h"

#include "game.h"

void App::render() { m_game->Render(); }
void App::process() { m_game->Process(); }

void App::done() {
    delete m_game;
    m_game = nullptr;
}

// The original assigns the new Game without deleting an earlier one (a second init leaks it).
void App::init(float viewWidth, float viewHeight) { m_game = new Game(viewWidth, viewHeight); }
