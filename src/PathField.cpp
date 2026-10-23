// 怪物寻路：以玩家为源点的反向 BFS 流场（flow field）。
//
// 不变量：每个被写入 parent_ 的格子，其层号（到玩家的最少步数）都比父格大 1，
// 因此「朝父格中心走一步」永不偏离最短路；走到根（玩家脚下）时 nextTile 返回 false。
#include "PathField.h"

#include "TileMap.h"

#include <queue>

// 瓦片坐标 → 64 位键：高 32 位 x、低 32 位 y。
// 先转 uint32_t 再移位，负坐标也不会触发「负数左移」的未定义行为。
int64_t PathField::keyOf(int x, int y) {
    return (int64_t(uint32_t(x)) << 32) | uint32_t(y);
}

// 64 位键 → 瓦片坐标。用 uint32_t 截断再转回 int，补码语义下负坐标可正确还原。
void PathField::unpack(int64_t key, int& x, int& y) {
    x = int(uint32_t(key >> 32));
    y = int(uint32_t(key & 0xffffffffu));
}

// 重建整张流场：从玩家瓦片出发做一次 BFS，把「上一层」写成父指针。
// 单次成本与 42 格内的可走格数成正比（实测约 3.3k 格 / 0.45 ms，详见 algorithm/detail.md）。
void PathField::rebuild(const TileMap& map, int playerTileX, int playerTileY) {
    // clear() 会保留哈希桶数组：每 10 帧左右重建一次，不会反复扩容。
    parent_.clear();
    originX_ = playerTileX;
    originY_ = playerTileY;

    // 源点入表，父指针指向自己 —— 这就是「已到玩家脚下」的终止哨兵。
    const int64_t start = keyOf(playerTileX, playerTileY);
    parent_[start] = start;
    // 必须是 FIFO 队列：换成栈会退化成 DFS，父指针就不再指向最短路。
    std::queue<int64_t> pending;
    pending.push(start);

    // 曼哈顿距离 42 格 ≈ 672 px ≈ 1.4 个屏幕宽：覆盖画面内外一圈，
    // 同时给场规模一个硬上界（≤ 2R²+2R+1 = 3613 格）。
    constexpr int kMaxSteps = 42;
    // 4 邻域：网格边权恒为 1，所以 BFS 求出的层号就是最短步数。
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (!pending.empty()) {
        int cx = 0;
        int cy = 0;
        unpack(pending.front(), cx, cy);
        pending.pop();
        // 剪枝在出队时做：层号到界的格子只保留父指针，不再继续外扩。
        // 因此场是一个「半径 42 的实心菱形」，边界之外的格子查不到，由调用方兜底。
        const int steps = std::abs(cx - playerTileX) + std::abs(cy - playerTileY);
        if (steps >= kMaxSteps) {
            continue;
        }
        for (const auto& d : dirs) {
            const int nx = cx + d[0];
            const int ny = cy + d[1];
            // 流场固定按 pass=0 建图（水 / 岩石 / 灌木 / 迷宫墙都算阻挡）。
            // 飞行、跳跃怪物的通行差异不在这里体现，交给执行层单独处理。
            if (!map.walkable(nx, ny)) {
                continue;
            }
            // 已访问过就跳过：BFS 首次发现某格时层号已是最小值，
            // 不需要 Dijkstra 那样「松弛 + 更新」的开销。
            const int64_t nk = keyOf(nx, ny);
            if (parent_.find(nk) != parent_.end()) {
                continue;
            }
            // 父指针指向当前格 —— 它恰好比新格更靠近玩家一层。
            parent_[nk] = keyOf(cx, cy);
            pending.push(nk);
        }
    }
}

// 查一步：返回 false 表示「没有下一步」，调用方必须走降级分支。
bool PathField::nextTile(int x, int y, int& outX, int& outY) const {
    // 纯查询，不改任何状态：可以被任意多条 AI 分支按任意顺序调用。
    const auto it = parent_.find(keyOf(x, y));
    if (it == parent_.end()) {
        return false;
    }
    unpack(it->second, outX, outY);
    // 父格就是自己 = 根（玩家脚下）：返回 false，让调用方切换到攻击逻辑。
    return !(outX == x && outY == y);
}
