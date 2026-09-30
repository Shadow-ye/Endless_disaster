#pragma once

#include "Types.h"

#include <cstdint>
#include <vector>

// 一座迷宫遗迹：外墙只有一个入口，生成树保证进中央广场只有一条通路。
class MazeRuin {
public:
    static constexpr int kSize = 33;
    // 15×15（原 7×7 的约 4.6 倍）。边长必须是奇数，门口才会落在广场外一圈，只留一个缺口。
    static constexpr int kPlaza0 = 9;
    static constexpr int kPlaza1 = 23;
    static constexpr int kCenter = 16;
    // 迷宫外围被临时清成空地的宽度（见 TileMap::at）。迷宫消失时这块地形会恢复原貌。
    static constexpr int kClearMargin = 4;

    enum class Phase { None, Live, Leave, Wait };

    bool active = false;
    Phase phase = Phase::None;
    int originX = 0;
    int originY = 0;
    uint32_t seed = 1;
    bool bossDead = false;
    bool enteredPlaza = false;
    float cooldown = 0.f;
    float arrive = 0.f;
    int entranceX = 1;
    int entranceY = 0;
    int doorX = 12;
    int doorY = 16;
    int attackStep = 0;
    float attackCd = 1.2f;
    float pulseR = -1.f;
    bool pulseHit = false;
    std::vector<uint8_t> wall;
    std::vector<uint8_t> route;

    void clear();
    bool generate(int originTileX, int originTileY, uint32_t mazeSeed);
    bool audit() const;

    bool contains(int tileX, int tileY) const;
    // 是否落在迷宫的影响区内（矩形本体 + 外围清空带），离开这里地形才会恢复原貌
    bool inClearZone(int tileX, int tileY) const;
    bool isWallAt(int tileX, int tileY) const;
    bool inPlaza(int tileX, int tileY) const;
    bool onRoute(int localX, int localY) const;
    static bool isPlazaRock(int localX, int localY);

    float centerX() const;
    float centerY() const;
    float entranceWorldX() const;
    float entranceWorldY() const;

    const std::vector<uint8_t>& walls() const { return wall; }
};
