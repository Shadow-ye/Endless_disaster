#include "TileMap.h"

TileMap::TileMap(uint32_t seed) : seed_(seed) {}

Tile TileMap::at(int x, int y) const {
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
    if (pass >= 2) {
        return false;
    }
    const Tile tile = at(x, y);
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
