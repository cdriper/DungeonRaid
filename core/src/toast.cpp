#include "toast.h"

#include "dr_gl.h"
#include "game.h"

namespace {
constexpr float kFadeIn = 0.2f;    // seconds
constexpr float kStay = 1.8f;
constexpr float kFadeOut = 0.4f;
constexpr float kTextScale = 0.09f;
constexpr float kPadX = 0.04f;     // screen units (the screen is 1 x 1.5)
constexpr float kHeight = 0.09f;
constexpr float kY = 1.25f;        // the box's middle
const GLcolour4 kBoxColour{20, 20, 35, 225};
}  // namespace

Toast::Toast(Game* game) : m_text(&game->m_fonts[0]) {
    m_text.m_spacing = 2.0f;
    m_text.m_scale = GLfloat2{kTextScale, kTextScale};
}

void Toast::Show(const std::string& text) {
    m_text.SetText(text.c_str(), -1);
    const float w = m_text.m_width * kTextScale;
    m_box.SetSize(GLfloat2{w + 2.0f * kPadX, kHeight});
    m_box.m_position = GLfloat2{0.5f, kY};
    m_text.m_position = GLfloat2{0.5f - w * 0.5f, kY + kTextScale * 0.22f};   // y: the baseline
    m_time = 0.0f;
    m_active = true;
    SetAlpha(0);
}

void Toast::Queue(const std::string& text) {
    if (m_active) {
        m_queue.push_back(text);
    } else {
        Show(text);
    }
}

void Toast::Process(float dt) {
    if (!m_active) return;
    m_time += dt;
    float a;
    if (m_time < kFadeIn) {
        a = m_time / kFadeIn;
    } else if (m_time < kFadeIn + kStay) {
        a = 1.0f;
    } else if (m_time < kFadeIn + kStay + kFadeOut) {
        a = 1.0f - (m_time - kFadeIn - kStay) / kFadeOut;
    } else {
        m_active = false;
        if (!m_queue.empty()) {   // the next queued message
            const std::string next = m_queue.front();
            m_queue.pop_front();
            Show(next);
        }
        return;
    }
    SetAlpha(static_cast<uint8_t>(a * 255.0f));
}

void Toast::SetAlpha(uint8_t alpha) {
    GLcolour4 c = kBoxColour;
    c.a = static_cast<uint8_t>(c.a * alpha / 0xff);
    m_box.SetColour(c);
    m_text.m_colour.a = alpha;
    m_text.UpdateColour();
}

void Toast::Render() {
    if (!m_active) return;
    sdlLoadIdentity();
    sdlDisable(GL_TEXTURE_2D);
    m_box.Render();
    sdlEnable(GL_TEXTURE_2D);
    m_text.Render();
}
