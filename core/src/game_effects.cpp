// Game: the pools of the visual effects (floating labels, particles). See game.h.
//
// The pools grow by one when full and are reused up to their counts. The original copies a
// freshly constructed (mostly uninitialised) stack object into a new slot, so the parts a label
// does not use (vertices and glyphs beyond its glyph count) hold stale bytes there; the port's new
// slots are zeroed.
#include "game.h"

// The label of a bonus token: type 0 at the tile, coloured by the resource type.
void Game::AddSimpleLabel(Tile* tile, TileType type) {
    if (m_simpleLabelCount == static_cast<int>(m_simpleLabels.size())) m_simpleLabels.push_back(SimpleLabel());
    m_simpleLabels[m_simpleLabelCount].Initialise(0, tile->m_position, 0, 1.0f);
    m_simpleLabels[m_simpleLabelCount].SetColour(type);
    m_simpleLabelCount += 1;
}

void Game::AddSimpleLabel(int type, const GLfloat2* position, int value, int colour, float scale) {
    if (m_simpleLabelCount == static_cast<int>(m_simpleLabels.size())) m_simpleLabels.push_back(SimpleLabel());
    m_simpleLabels[m_simpleLabelCount].Initialise(type, *position, value, scale);
    m_simpleLabels[m_simpleLabelCount].SetColour(colour);
    m_simpleLabelCount += 1;
}

// A burst of particles from a tile (ParticleType: 0 its pieces, 1..3 sparks at a value label).
void Game::AddParticles(Tile* tile, int type) {
    if (m_particleCount == static_cast<int>(m_particles.size())) m_particles.push_back(Particles());
    m_particles[m_particleCount].InitialiseFromTile(tile, type);
    m_particleCount += 1;
}
