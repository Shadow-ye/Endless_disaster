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

void TileMap::setShallowPool(int cx, int cy, int radius, int margin, int boardX, int boardY, int boardSize, int boardMargin) {
    shallowActive_ = radius > 0;
    shallowX_ = cx;
    shallowY_ = cy;
    shallowR_ = radius;
    shallowMargin_ = std::max(0, margin);
    boardX_ = boardX;
    boardY_ = boardY;
    boardSize_ = boardSize;
    boardMargin_ = std::max(0, boardMargin);
}

void TileMap::clearShallowPool() {
    shallowActive_ = false;
    shallowR_ = 0;
    shallowMargin_ = 0;
    boardSize_ = 0;
    boardMargin_ = 0;
}

bool TileMap::inShallowPool(int x, int y) const {
    if (!shallowActive_) {
        return false;
    }
    const int dx = x - shallowX_;
    const int dy = y - shallowY_;
    return dx * dx + dy * dy <= shallowR_ * shallowR_;
}

bool TileMap::inShallowClearZone(int x, int y) const {
    if (!shallowActive_) {
        return false;
    }
    const int dx = x - shallowX_;
    const int dy = y - shallowY_;
    const int r = shallowR_ + shallowMargin_;
    if (dx * dx + dy * dy <= r * r) {
        return true;
    }
    // 棋盘整块矩形也要清干净，树/岩石的贴图会向上溢出十几像素盖住格子数字
    if (boardSize_ > 0) {
        if (x >= boardX_ - boardMargin_ && y >= boardY_ - boardMargin_
            && x < boardX_ + boardSize_ + boardMargin_ && y < boardY_ + boardSize_ + boardMargin_) {
            return true;
        }
    }
    return false;
}

bool TileMap::onShallowBoard(int x, int y) const {
    if (!shallowActive_ || boardSize_ <= 0) {
        return false;
    }
    const int lx = x - boardX_;
    const int ly = y - boardY_;
    return lx >= 0 && ly >= 0 && lx < boardSize_ && ly < boardSize_;
}

void TileMap::scorchAt(int x, int y) {
    const int64_t key = tileKey(x, y);
    if (scorch_.insert(key).second) {
        scorchKeys_.push_back(key);
    }
}

void TileMap::scorchCircle(int cx, int cy, int tiles) {
    if (tiles <= 0) {
        return;
    }
    const int r2 = tiles * tiles;
    for (int ty = cy - tiles; ty <= cy + tiles; ++ty) {
        for (int tx = cx - tiles; tx <= cx + tiles; ++tx) {
            const int dx = tx - cx;
            const int dy = ty - cy;
            if (dx * dx + dy * dy > r2) {
                continue;
            }
            scorchAt(tx, ty);
        }
    }
}

bool TileMap::scorched(int x, int y) const {
    return scorch_.find(tileKey(x, y)) != scorch_.end();
}

void TileMap::decodeKey(int64_t key, int& x, int& y) {
    x = int32_t(key >> 32);
    y = int32_t(uint32_t(key & 0xffffffffLL));
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
        // 迷宫地板也能被烧成沙地；中央广场的刻纹和墙各自保留原样
        if (scorched(x, y)) {
            return Tile::Dirt;
        }
        return Tile::MazeFloor;
    }
    // 浅水 boss 房：圆形水面盖住原地形，棋盘格强制成可走地面
    if (inShallowPool(x, y)) {
        return Tile::Shallow;
    }
    if (onShallowBoard(x, y)) {
        return Tile::Grass;
    }
    // 先算出原本该是什么地形，再按浅水区清空带决定要不要把障碍物抹平
    const Tile base = baseTile(x, y);
    if (inShallowClearZone(x, y) && (base == Tile::Rock || base == Tile::Bush || base == Tile::Water)) {
        return Tile::Grass;
    }
    return base;
}

// 不含任何 boss 房覆盖层的地形：烧焦 → 迷宫清空带 → 程序化地表
Tile TileMap::baseTile(int x, int y) const {
    if (scorched(x, y)) {
        return Tile::Dirt;
    }
    // 迷宫外圈清成空地，避免岩石、灌木和水堵住或挡住唯一入口。
    if (ruinActive_) {
        constexpr int kMargin = MazeRuin::kClearMargin;
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
    // 浅水与平地通行完全一致，只靠 Session 侧的速度系数略微减速
    if (tile == Tile::Shallow) {
        return false;
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
