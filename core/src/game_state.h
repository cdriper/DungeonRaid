// GameState: a byte buffer that the game serialises its state into and reads it back from.
// The Java shell saves/loads these buffers (via the GetChar/PutChar/... helpers that SWIG inlined
// into the JNI stubs). Integers are little endian, 4 bytes.
#pragma once

#include <cstdint>

class GameState {
public:
    explicit GameState(int capacity);   // 0x00047420
    ~GameState();                       // 0x00047400
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    void WriteChar(uint8_t value);      // 0x00047378
    void WriteInteger(int value);       // 0x0004738c
    uint8_t ReadChar();                 // 0x000473bc
    int ReadInteger();                  // 0x000473cc

    // Inlined by SWIG into the JNI stubs (Java_..._GameState_1*):
    int GetStateSize() const { return m_size; }
    uint8_t GetChar(int index) const { return m_buffer[index]; }
    void PutChar(int index, uint8_t value) { m_buffer[index] = value; }
    bool HasStateChanged() const { return m_changed; }
    void ClearStateChanged() { m_changed = false; }
    void ResetStates() {
        m_size = 0;
        m_readPos = 0;
    }

    // Inlined by Game (SerialiseState/SerialiseClasses: size = 0, changed = 1; DeserialiseState:
    // read position = 0).
    void ResetForWriting() {
        m_size = 0;
        m_changed = true;
    }
    void ResetForReading() { m_readPos = 0; }

    // Port additions (no bounds checks in the original either).
    int GetCapacity() const { return m_capacity; }
    const uint8_t* GetBuffer() const { return m_buffer; }
    int GetReadPosition() const { return m_readPos; }

    // Original layout (32-bit): capacity +0, buffer +4, size (write position) +8, changed +0xc,
    // read position +0x10. The port keeps the members, not the exact offsets.
private:
    int m_capacity;
    uint8_t* m_buffer;
    int m_size;
    bool m_changed;
    int m_readPos;
};
