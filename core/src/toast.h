// Toast (port-only): a short message in a dark box near the bottom of the screen, over everything;
// fades in, stays, fades out. A new message (Show) replaces the one showing; a queued one (Queue)
// waits for it. Game::m_toast; processed
// every frame (Game::Process), drawn last (Game::Render).
#pragma once

#include <deque>
#include <string>

#include "label.h"
#include "quad.h"

class Game;

class Toast {
public:
    explicit Toast(Game* game);
    Toast(const Toast&) = delete;
    Toast& operator=(const Toast&) = delete;

    void Show(const std::string& text);   // the fonts must be loaded
    void Queue(const std::string& text);  // shown after the toasts before it (achievements: several at once)
    void Process(float dt);
    void Render();
    const std::string& Text() const { return m_text.m_text; }   // the message showing (or last shown)

    bool m_active = false;

private:
    void SetAlpha(uint8_t alpha);

    Quad m_box;      // untextured
    Label m_text;
    float m_time = 0.0f;   // since Show
    std::deque<std::string> m_queue;   // Queue's messages waiting for the one showing
};
