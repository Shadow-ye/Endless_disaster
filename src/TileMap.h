#pragma once

#include "Types.h"

enum class Tile : uint8_t { Grass, Dirt, Water, Rock, Bush };

class TileMap {
public:
    explicit TileMap(uint32_t seed = 1);

    uint32_t seed() const { return seed_; }
    Tile at(int x, int y) const;
    bool walkable(int x, int y) const;
    bool blocks(int x, int y, int pass) const;
    bool blockedAt(float x, float y, float radius, int pass) const;

private:
    uint32_t seed_;
};
