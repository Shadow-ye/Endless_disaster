#include "ShallowPool.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

int popCount(uint32_t v) {
    int n = 0;
    while (v) {
        v &= v - 1u;
        ++n;
    }
    return n;
}

}  // namespace

void ShallowPool::clear() {
    active = false;
    phase = Phase::None;
    cx = 0;
    cy = 0;
    radius = kRadiusTiles;
    boardX = 0;
    boardY = 0;
    seed = 1;
    bossDead = false;
    entered = false;
    cleared = false;
    cooldown = 0.f;
    arrive = 0.f;
    sanCut = 0.f;
    attackStep = 0;
    attackCd = 1.4f;
    warningT = 0.f;
    chargeT = 0.f;
    chargeX = 0.f;
    chargeY = 0.f;
    leapWarnT = 0.f;
    leapT = 0.f;
    leapFromX = 0.f;
    leapFromY = 0.f;
    leapToX = 0.f;
    leapToY = 0.f;
    shootWarnT = 0.f;
    shootAim = 0.f;
    trailT = 0.f;
    mineMask = 0;
    revealedMask = 0;
    triggeredMask = 0;
}

bool ShallowPool::generate(int centerX, int centerY, int boardOriginX, int boardOriginY, uint32_t poolSeed) {
    clear();
    cx = centerX;
    cy = centerY;
    boardX = boardOriginX;
    boardY = boardOriginY;
    seed = poolSeed == 0 ? 1u : poolSeed;

    // 用种子洗牌 25 个格子，前 kMines 个当雷。同一颗种子一定得到同一张棋盘。
    uint32_t rng = seed;
    auto next = [&]() {
        rng = mixHash(rng + 0x9e3779b9u);
        return rng;
    };
    std::vector<int> cells(kBoardSize * kBoardSize);
    for (int i = 0; i < int(cells.size()); ++i) {
        cells[size_t(i)] = i;
    }
    for (int i = int(cells.size()) - 1; i > 0; --i) {
        const int j = int(next() % uint32_t(i + 1));
        std::swap(cells[size_t(i)], cells[size_t(j)]);
    }
    for (int i = 0; i < kMines && i < int(cells.size()); ++i) {
        mineMask |= 1u << cells[size_t(i)];
    }
    active = true;
    return true;
}

bool ShallowPool::contains(int tileX, int tileY) const {
    const int dx = tileX - cx;
    const int dy = tileY - cy;
    return dx * dx + dy * dy <= radius * radius;
}

bool ShallowPool::inClearZone(int tileX, int tileY) const {
    const int dx = tileX - cx;
    const int dy = tileY - cy;
    const int r = radius + kClearMargin;
    if (dx * dx + dy * dy <= r * r) {
        return true;
    }
    // 棋盘那一片矩形（含额外清空边）也算场地，否则玩家站在棋盘边缘时 boss 房会提前沉掉
    return tileX >= boardX - kBoardMargin && tileY >= boardY - kBoardMargin
        && tileX < boardX + kBoardSize + kBoardMargin && tileY < boardY + kBoardSize + kBoardMargin;
}

bool ShallowPool::boardLocal(int tileX, int tileY, int& lx, int& ly) const {
    lx = tileX - boardX;
    ly = tileY - boardY;
    return lx >= 0 && ly >= 0 && lx < kBoardSize && ly < kBoardSize;
}

bool ShallowPool::onBoard(int tileX, int tileY) const {
    int lx = 0;
    int ly = 0;
    return boardLocal(tileX, tileY, lx, ly);
}

bool ShallowPool::isMine(int lx, int ly) const {
    if (lx < 0 || ly < 0 || lx >= kBoardSize || ly >= kBoardSize) {
        return false;
    }
    return (mineMask & (1u << (ly * kBoardSize + lx))) != 0u;
}

bool ShallowPool::isRevealed(int lx, int ly) const {
    if (lx < 0 || ly < 0 || lx >= kBoardSize || ly >= kBoardSize) {
        return false;
    }
    return (revealedMask & (1u << (ly * kBoardSize + lx))) != 0u;
}

bool ShallowPool::isTriggered(int lx, int ly) const {
    if (lx < 0 || ly < 0 || lx >= kBoardSize || ly >= kBoardSize) {
        return false;
    }
    return (triggeredMask & (1u << (ly * kBoardSize + lx))) != 0u;
}

int ShallowPool::adjacentMines(int lx, int ly) const {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            if (isMine(lx + dx, ly + dy)) {
                count += 1;
            }
        }
    }
    return count;
}

int ShallowPool::revealedCount() const {
    return popCount(revealedMask);
}

int ShallowPool::triggeredCount() const {
    return popCount(triggeredMask);
}

bool ShallowPool::boardComplete() const {
    return revealedCount() >= safeTotal() || triggeredCount() >= kMines;
}

void ShallowPool::relocateMine(int fromX, int fromY) {
    const int from = fromY * kBoardSize + fromX;
    const uint32_t fromBit = 1u << from;
    if ((mineMask & fromBit) == 0u) {
        return;
    }
    // 优先挪到 3x3 邻域之外的第一个空位：这样开局第一格翻开就能连锁掀开一片
    int chosen = -1;
    for (int i = 0; i < kBoardSize * kBoardSize && chosen < 0; ++i) {
        if (mineMask & (1u << i)) {
            continue;
        }
        const int cx = i % kBoardSize;
        const int cy = i / kBoardSize;
        if (std::abs(cx - fromX) <= 1 && std::abs(cy - fromY) <= 1) {
            continue;
        }
        chosen = i;
    }
    if (chosen < 0) {
        for (int i = 0; i < kBoardSize * kBoardSize; ++i) {
            if (i != from && (mineMask & (1u << i)) == 0u) {
                chosen = i;
                break;
            }
        }
    }
    if (chosen < 0) {
        return;
    }
    mineMask &= ~fromBit;
    mineMask |= 1u << chosen;
}

int ShallowPool::reveal(int tileX, int tileY) {
    int lx = 0;
    int ly = 0;
    if (!boardLocal(tileX, tileY, lx, ly)) {
        return 0;
    }
    uint32_t bit = 1u << (ly * kBoardSize + lx);
    if (mineMask & bit) {
        if (triggeredMask & bit) {
            return 0;  // 同一颗雷只触发一次
        }
        if (revealedMask == 0 && triggeredMask == 0) {
            // 经典扫雷的「第一下必定安全」：开局踩到的雷先挪走
            relocateMine(lx, ly);
            bit = 1u << (ly * kBoardSize + lx);
        } else {
            triggeredMask |= bit;
            return 2;
        }
    }
    if (revealedMask & bit) {
        return 0;
    }
    // 经典扫雷：翻开一格；相邻雷数为 0 时从这一格向外连锁翻开，
    // 一路翻到有数字的格子为止（数字格本身翻开、不再往外扩）。
    std::vector<int> stack;
    stack.push_back(ly * kBoardSize + lx);
    while (!stack.empty()) {
        const int idx = stack.back();
        stack.pop_back();
        const uint32_t idxBit = 1u << idx;
        if ((revealedMask & idxBit) || (mineMask & idxBit)) {
            continue;
        }
        revealedMask |= idxBit;
        const int cx = idx % kBoardSize;
        const int cy = idx / kBoardSize;
        if (adjacentMines(cx, cy) != 0) {
            continue;
        }
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const int nx = cx + dx;
                const int ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= kBoardSize || ny >= kBoardSize) {
                    continue;
                }
                stack.push_back(ny * kBoardSize + nx);
            }
        }
    }
    return 1;
}

float ShallowPool::centerX() const {
    return (float(cx) + 0.5f) * float(kTile);
}

float ShallowPool::centerY() const {
    return (float(cy) + 0.5f) * float(kTile);
}
