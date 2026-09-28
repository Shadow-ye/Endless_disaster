#include "TileMap.h"

#include "MazeRuin.h"

TileMap::TileMap(uint32_t seed) : seed_(seed) {}

void TileMap::setRuin(int originX, int originY, int width, int height, const std::vector<uint8_t>& walls) {
    ruinActive_ = width > 0 && height > 0 && int(walls.size()) == width * height;
    ruinX_ = originX;
    ruinY_ = originY;
    ruinW_ = width;
    ruinH_ = height;
    ruinWall_ = walls;
}

void TileMap::clearRuin() {
    ruinActive_ = false;
    ruinWall_.clear();
    ruinW_ = 0;
    ruinH_ = 0;
}

bool TileMap::inRuin(int x, int y) const {
    if (!ruinActive_) {
        return false;
    }
    const int lx = x - ruinX_;
    const int ly = y - ruinY_;
    return lx >= 0 && ly >= 0 && lx < ruinW_ && ly < ruinH_;
}

Tile TileMap::at(int x, int y) const {
    if (inRuin(x, y)) {
        const int lx = x - ruinX_;
        const int ly = y - ruinY_;
        if (ruinWall_[ly * ruinW_ + lx] != 0) {
            return Tile::MazeWall;
        }
        if (lx >= MazeRuin::kPlaza0 && lx <= MazeRuin::kPlaza1 && ly >= MazeRuin::kPlaza0 && ly <= MazeRuin::kPlaza1) {
            if (MazeRuin::isPlazaRock(lx, ly)) {
                return Tile::Rock;
            }
            return Tile::Plaza;
        }
        return Tile::MazeFloor;
    }
    // 迷宫外圈清成空地，避免岩石、灌木和水堵住或挡住唯一入口。
    if (ruinActive_) {
        constexpr int kMargin = 4;
        const int lx = x - ruinX_;
        const int ly = y - ruinY_;
        if (lx >= -kMargin && ly >= -kMargin && lx < ruinW_ + kMargin && ly < ruinH_ + kMargin) {
            return Tile::Grass;
        }
    }
    if (x * x + y * y <= 64) {
        return Tile::Grass;
    }
    const int lx = x & 3;
    const int ly = y & 3;
    const uint32_t pond = mixHash(seed_ ^ 0x51u ^ mixHash(uint32_t(x >> 2)) ^ mixHash(uint32_t(y >> 2) * 0x9e3779b1u));
    if (pond % 100u < 12u && lx < 2 && ly < 2) {
        return Tile::Water;
    }
    const uint32_t clump = mixHash(seed_ ^ 0x99u ^ mixHash(uint32_t(x >> 1)) ^ mixHash(uint32_t(y >> 1) * 0x85ebca6bu));
    if (clump % 100u < 7u) {
        return (clump & 1u) ? Tile::Rock : Tile::Bush;
    }
    const uint32_t h = mixHash(seed_ ^ mixHash(uint32_t(x) * 0x85ebca6bu) ^ mixHash(uint32_t(y)));
    if (h % 100u < 14u) {
        return Tile::Dirt;
    }
    return Tile::Grass;
}

bool TileMap::walkable(int x, int y) const {
    return !blocks(x, y, 0);
}

bool TileMap::blocks(int x, int y, int pass) const {
    const Tile tile = at(x, y);
    // 迷宫墙任何通行等级都过不去，包括跳跃、飞行和飞行怪。
    if (tile == Tile::MazeWall) {
        return true;
    }
    if (pass >= 2) {
        return false;
    }
    if (tile == Tile::Water) {
        return true;
    }
    if (tile == Tile::Rock || tile == Tile::Bush) {
        return pass == 0;
    }
    return false;
}

bool TileMap::blockedAt(float x, float y, float radius, int pass) const {
    const float points[5][2] = {
        {x, y},
        {x + radius, y},
        {x - radius, y},
        {x, y + radius},
        {x, y - radius},
    };
    for (const auto& p : points) {
        if (blocks(tileOf(p[0]), tileOf(p[1]), pass)) {
            return true;
        }
    }
    return false;
}
