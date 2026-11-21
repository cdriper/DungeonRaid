// Token (derives from Quad): a collected resource flying to the HUD (or to a skill button, a thief,
// the player's health). Layout: re/structs/classes.json "Token" (0xa0 bytes in the original).
// Complete except the destructor's details (nothing to free).
#pragma once

#include "dr_types.h"
#include "quad.h"

class Interface;
class Tile;

class Token : public Quad {
public:
    explicit Token(Interface* ui);                                  // 0x00065388
    ~Token() override;                                              // 0x00065334

    // A token that starts as a copy of `tile` and flies to the target of `type` (tokenType 3:
    // enemy damage, flies to the health bar). position: start position instead of the tile's.
    void InitialiseFromTile(Tile* tile, TileType type, const GLfloat2* position, int tokenType);   // 0x000653d0
    // One frame of the flight: turns from the start direction towards the target while m_progress
    // grows to 1, then accelerates straight at it; dies when the step would pass the target.
    void Process(float dt);                                         // 0x00065200

    bool m_alive = false;          // Quad+0x72 in the original (a spare byte after the render flags)
    Interface* m_interface;        // 0x74
    TileType m_type = TILE_POTION; // 0x78: the resource it delivers
    int m_kind = 0;                // 0x7c: 0 normal, 1 mana (to a skill button), 2 acid, 3 enemy damage
    GLfloat2 m_direction{};        // 0x80: initial direction (length 0.1)
    float m_speed = 0.0f;          // 0x88
    float m_progress = 0.0f;       // 0x8c: 0..1 turning from m_direction to the target
    float m_acceleration = 0.0f;   // 0x90
    float m_distance = 0.0f;       // 0x94: distance to the target at the start (scales the token)
    int m_value = 0;               // 0x98: mana: skill index; thrown skull: its health (damage)
    Tile* m_target = nullptr;      // 0x9c: a tile to fly to instead (Thief)
};
