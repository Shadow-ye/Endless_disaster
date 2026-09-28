#pragma once

#include "Types.h"

#include <cstdint>
#include <vector>

enum class Tile : uint8_t { Grass, Dirt, Water, Rock, Bush, MazeWall, MazeFloor, Plaza };

class TileMap {
public:
    explicit TileMap(uint32_t seed = 1);

    uint32_t seed() const { return seed_; }
    Tile at(int x, int y) const;
    bool walkable(int x, int y) const;
    bool blocks(int x, int y, int pass) const;
    bool blockedAt(float x, float y, float radius, int pass) const;
    bool inRuin(int x, int y) const;
    void setRuin(int originX, int originY, int width, int height, const std::vector<uint8_t>& walls);
    void clearRuin();

private:
    uint32_t seed_;
    bool ruinActive_ = false;
    int ruinX_ = 0;
    int ruinY_ = 0;
    int ruinW_ = 0;
    int ruinH_ = 0;
    std::vector<uint8_t> ruinWall_;
};
