#include "PathField.h"

#include "TileMap.h"

#include <queue>

int64_t PathField::keyOf(int x, int y) {
    return (int64_t(uint32_t(x)) << 32) | uint32_t(y);
}

void PathField::unpack(int64_t key, int& x, int& y) {
    x = int(uint32_t(key >> 32));
    y = int(uint32_t(key & 0xffffffffu));
}

void PathField::rebuild(const TileMap& map, int playerTileX, int playerTileY) {
    parent_.clear();
    originX_ = playerTileX;
    originY_ = playerTileY;

    const int64_t start = keyOf(playerTileX, playerTileY);
    parent_[start] = start;
    std::queue<int64_t> pending;
    pending.push(start);

    constexpr int kMaxSteps = 42;
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (!pending.empty()) {
        int cx = 0;
        int cy = 0;
        unpack(pending.front(), cx, cy);
        pending.pop();
        const int steps = std::abs(cx - playerTileX) + std::abs(cy - playerTileY);
        if (steps >= kMaxSteps) {
            continue;
        }
        for (const auto& d : dirs) {
            const int nx = cx + d[0];
            const int ny = cy + d[1];
            if (!map.walkable(nx, ny)) {
                continue;
            }
            const int64_t nk = keyOf(nx, ny);
            if (parent_.find(nk) != parent_.end()) {
                continue;
            }
            parent_[nk] = keyOf(cx, cy);
            pending.push(nk);
        }
    }
}

bool PathField::nextTile(int x, int y, int& outX, int& outY) const {
    const auto it = parent_.find(keyOf(x, y));
    if (it == parent_.end()) {
        return false;
    }
    unpack(it->second, outX, outY);
    return !(outX == x && outY == y);
}
