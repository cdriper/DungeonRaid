#include "game_state.h"

GameState::GameState(int capacity)
    : m_capacity(capacity), m_buffer(new uint8_t[capacity]), m_size(0), m_changed(false), m_readPos(0) {}

GameState::~GameState() {
    delete[] m_buffer;
}

void GameState::WriteChar(uint8_t value) {
    m_buffer[m_size] = value;
    ++m_size;
    m_changed = true;
}

void GameState::WriteInteger(int value) {
    uint32_t v = static_cast<uint32_t>(value);
    WriteChar(static_cast<uint8_t>(v));
    WriteChar(static_cast<uint8_t>(v >> 8));
    WriteChar(static_cast<uint8_t>(v >> 16));
    WriteChar(static_cast<uint8_t>(v >> 24));
    m_changed = true;
}

uint8_t GameState::ReadChar() {
    uint8_t value = m_buffer[m_readPos];
    ++m_readPos;
    return value;
}

int GameState::ReadInteger() {
    uint32_t b0 = ReadChar();
    uint32_t b1 = ReadChar();
    uint32_t b2 = ReadChar();
    uint32_t b3 = ReadChar();
    return static_cast<int>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}
