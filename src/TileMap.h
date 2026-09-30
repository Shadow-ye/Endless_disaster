#pragma once

#include "Types.h"

#include <cstdint>
#include <unordered_set>
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

    // 核爆把地表烧成沙地：记下烧过的格子，at() 对这些格子改口成 Dirt
    void scorchCircle(int cx, int cy, int tiles);
    void scorchAt(int x, int y);
    bool scorched(int x, int y) const;
    const std::vector<int64_t>& scorchedKeys() const { return scorchKeys_; }
    // scorchedKeys() 里的 key 拆回格子坐标；位布局只在这里和 tileKey 里出现
    static void decodeKey(int64_t key, int& x, int& y);

private:
    static int64_t tileKey(int x, int y) { return (int64_t(x) << 32) ^ int64_t(uint32_t(y)); }

    uint32_t seed_;
    std::unordered_set<int64_t> scorch_;
    std::vector<int64_t> scorchKeys_;
    bool ruinActive_ = false;
    int ruinX_ = 0;
    int ruinY_ = 0;
    int ruinW_ = 0;
    int ruinH_ = 0;
    std::vector<uint8_t> ruinWall_;
};
