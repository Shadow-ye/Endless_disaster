#pragma once

#include "Types.h"

#include <unordered_map>

class TileMap;

// One breadth-first search from the player. Each reached tile stores the
// previous tile, which is one step closer to the player. Monsters walk that
// link, so the path is the search from player to monster, reversed.
// 中文：以玩家为源点做一次广度优先搜索。每个被访问到的瓦片记录它的「上一层瓦片」，
// 那一格恰好比当前格更靠近玩家一格；怪物沿着这条父指针走一步即可。
// 所以实际前进方向，就是把「玩家 → 怪物」的搜索过程反过来走。
//
// 三条设计要点：
//   1. 单源共享：全场怪物共用这一张表，怪物数量不影响搜索次数；
//   2. 低频重建：只在玩家跨瓦片或地形变化时重建（见 Session::update / commitBrokenWalls）；
//   3. 硬上界：搜索限制在曼哈顿距离 42 格内，时间与内存都不随存活时长增长。
class PathField {
public:
    // 重建整张流场。
    // map           : 地形查询入口（流场按 pass=0 的通行语义建图）
    // playerTileX/Y : 玩家所在瓦片，作为 BFS 源点（也就是这张场的「根」）
    // 复杂度 O(E)，E = 42 格菱形内实际可走的格数。
    void rebuild(const TileMap& map, int playerTileX, int playerTileY);

    // 查询 (x, y) 的下一步该走哪一格：成功则把父格坐标写入 outX/outY 并返回 true。
    // 返回 false 的两种情况：
    //   1. 该格不在场里（超出 42 格 / 被障碍隔断 / 与流场的通行等级不符）；
    //   2. (x, y) 就是玩家脚下（根的自环哨兵），表示已经走到终点。
    // 调用方必须处理 false，否则怪物会在这些位置站住不动（见 Session 的降级链）。
    bool nextTile(int x, int y, int& outX, int& outY) const;

private:
    // 二维瓦片坐标 → 单个 64 位键：高 32 位放 x、低 32 位放 y。
    // 先转 uint32_t 再移位，避免负坐标左移的未定义行为。
    static int64_t keyOf(int x, int y);
    // keyOf 的逆运算：把 64 位键拆回瓦片坐标（经 uint32_t 截断，负数也能正确还原）。
    static void unpack(int64_t key, int& x, int& y);

    // 瓦片键 → 父瓦片键。世界无边界、地形稀疏，不能开二维数组，
    // 所以用哈希表只为半径 42 内真正可达的格子分配内存。
    // 根节点存成自环（parent_[根] = 根），作为「已到玩家脚下」的终止哨兵。
    std::unordered_map<int64_t, int64_t> parent_;
    // 记录本次重建的源点瓦片。目前只写不读（场本身以 parent_ 为准），
    // 保留它是为了调试时能确认「这张场是以哪一格为根建的」。
    int originX_ = 0;
    int originY_ = 0;
};
