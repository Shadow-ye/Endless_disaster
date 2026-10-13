#pragma once

#include "Types.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Corroded 只追加在末尾：存档记录的是覆盖层坐标而不是枚举值，旧档不会串味
enum class Tile : uint8_t { Grass, Dirt, Water, Rock, Bush, MazeWall, MazeFloor, Plaza, Shallow, Corroded };

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

    // 浅水 boss 房覆盖层：一块圆形浅水区（通行与平地无异）+ 正中央一块排雷棋盘。
    // 棋盘格强制成可走地面；浅水区外 margin 格内、以及棋盘外 boardMargin 格内的
    // 岩石/灌木/水塘也一并清掉，只留平地（棋盘上方的树会把格子数字盖住）。
    void setShallowPool(int cx, int cy, int radius, int margin, int boardX, int boardY, int boardSize, int boardMargin);
    void clearShallowPool();
    bool inShallowPool(int x, int y) const;
    bool inShallowClearZone(int x, int y) const;
    bool onShallowBoard(int x, int y) const;

    // 核爆把地表烧成沙地：记下烧过的格子，at() 对这些格子改口成 Dirt
    void scorchCircle(int cx, int cy, int tiles);
    void scorchAt(int x, int y);
    bool scorched(int x, int y) const;
    const std::vector<int64_t>& scorchedKeys() const { return scorchKeys_; }
    // scorchedKeys() 里的 key 拆回格子坐标；位布局只在这里和 tileKey 里出现
    static void decodeKey(int64_t key, int& x, int& y);

    // 腐蚀地面：巨型腐化史莱姆（以及拟态成史莱姆的玩家）经过处把格子变成腐蚀地形，
    // 踩上去持续微量掉血（伤害判定在 Session 侧）。只留 kCorrosionLife 秒就自行恢复，
    // 属于战场上的临时危险区，和核爆烧焦那种永久改造不同，所以不进存档。
    static constexpr float kCorrosionLife = 10.f;
    void corrodeAt(int x, int y);
    bool corroded(int x, int y) const;
    // 这一格距离恢复原状还有多少秒；不是腐蚀格时返回 0（渲染用它做临期渐隐）
    float corrosionLeft(int x, int y) const;
    void updateCorrosion(float dt);
    void clearCorrosion();

private:
    // 不含腐蚀层的地表：迷宫 / 浅水房覆盖层都已算进去，at() 在此基础上再叠腐蚀
    Tile surfaceTile(int x, int y) const;
    // 不含任何 boss 房覆盖层的地形（烧焦 / 迷宫清空带 / 程序化地表）
    Tile baseTile(int x, int y) const;
    static int64_t tileKey(int x, int y) { return (int64_t(x) << 32) ^ int64_t(uint32_t(y)); }

    uint32_t seed_;
    std::unordered_set<int64_t> scorch_;
    std::vector<int64_t> scorchKeys_;
    // 腐蚀格 → 已经腐蚀了多久（秒）；到 kCorrosionLife 就恢复原样
    std::unordered_map<int64_t, float> corrode_;
    bool ruinActive_ = false;
    int ruinX_ = 0;
    int ruinY_ = 0;
    int ruinW_ = 0;
    int ruinH_ = 0;
    std::vector<uint8_t> ruinWall_;
    bool shallowActive_ = false;
    int shallowX_ = 0;
    int shallowY_ = 0;
    int shallowR_ = 0;
    int shallowMargin_ = 0;
    int boardX_ = 0;
    int boardY_ = 0;
    int boardSize_ = 0;
    int boardMargin_ = 0;
};
