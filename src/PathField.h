#pragma once

#include "Types.h"

#include <unordered_map>

class TileMap;

// One breadth-first search from the player. Each reached tile stores the
// previous tile, which is one step closer to the player. Monsters walk that
// link, so the path is the search from player to monster, reversed.
class PathField {
public:
    void rebuild(const TileMap& map, int playerTileX, int playerTileY);

    bool nextTile(int x, int y, int& outX, int& outY) const;

private:
    static int64_t keyOf(int x, int y);
    static void unpack(int64_t key, int& x, int& y);

    std::unordered_map<int64_t, int64_t> parent_;
    int originX_ = 0;
    int originY_ = 0;
};
