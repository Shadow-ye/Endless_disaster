#pragma once

#include "Types.h"

#include <cstdint>

// 浅水 boss 房：一块圆形浅水区（通行与平地一样，只对生物略微减速），
// 旁边地面上刷新一局扫雷棋盘。玩家跳跃揭示方块，排完雷后「巨型腐化史莱姆」从水中浮出。
class ShallowPool {
public:
    static constexpr int kBoardSize = 5;
    static constexpr int kMines = 5;
    // 圆形浅水区的半径（格）。直径 29 格，够当一个 boss 竞技场
    static constexpr int kRadiusTiles = 14;
    // 浅水区外圈同样清成空地的宽度（格）：周围障碍物一律抹掉，只留可走的平地，
    // 同时这块余量也是 boss 房消失前要求玩家先走出去的距离
    static constexpr int kClearMargin = 4;
    // 棋盘四周额外清空这么多格：树木/岩石的贴图会往上、往左溢出十几像素，
    // 贴着棋盘长就会把格子数字盖住，所以棋盘这块矩形要单独再清一圈
    static constexpr int kBoardMargin = 2;
    // boss 从水里浮出所用的时间
    static constexpr float kRiseTime = 1.7f;

    // 巨型腐化史莱姆的攻击参数：判定与「攻击范围预警指示」共用同一套数，
    // 保证地上画出来的范围就是真正会打到的范围。
    static constexpr float kBossRadius = 22.f;
    // 冲撞：预警 0.55 秒（地上画一条冲刺走廊）→ 冲出 0.5 秒
    static constexpr float kWarnCharge = 0.55f;
    static constexpr float kChargeTime = 0.5f;
    static constexpr float kChargeSpeed = 330.f;
    // 弹跳砸击：预警 0.6 秒（地上画落点圈）→ 滞空 0.62 秒
    static constexpr float kWarnLeap = 0.6f;
    static constexpr float kLeapTime = 0.62f;
    static constexpr float kLeapRadius = 52.f;
    // 腐蚀水弹：预警 0.5 秒（地上画 5 条射线）→ 发射
    static constexpr float kWarnShoot = 0.5f;
    static constexpr int kShootShots = 5;
    static constexpr float kShootSpread = 0.22f;
    // 预警射线的长度，只用于绘制
    static constexpr float kShootHintRange = 190.f;

    enum class Phase { None, Live, Leave, Wait };

    bool active = false;
    Phase phase = Phase::None;
    int cx = 0;
    int cy = 0;
    int radius = kRadiusTiles;
    // 棋盘左上角所在格
    int boardX = 0;
    int boardY = 0;
    uint32_t seed = 1;
    bool bossDead = false;
    // 玩家是否已经踏进过浅水区（用于播一次玩法提示）
    bool entered = false;
    // 排雷是否完成（安全格全部揭示，或所有雷都被踩过）
    bool cleared = false;
    float cooldown = 0.f;
    // boss 浮出动画的剩余时间；> 0 表示还在浮出，等于 0 表示已完全登场
    float arrive = 0.f;
    // 本场战斗里踩雷削减的 SAN 上限累计量
    float sanCut = 0.f;
    // boss 攻击节奏
    int attackStep = 0;
    float attackCd = 1.4f;
    // 冲撞：前摇预警 → 冲刺（预警期间charge方向持续朝玩家，结束时锁定）
    float warningT = 0.f;
    float chargeT = 0.f;
    float chargeX = 0.f;
    float chargeY = 0.f;
    // 弹跳砸击：落点预警 → 腾空
    float leapWarnT = 0.f;
    float leapT = 0.f;
    float leapFromX = 0.f;
    float leapFromY = 0.f;
    float leapToX = 0.f;
    float leapToY = 0.f;
    // 腐蚀水弹：射线预警 → 发射
    float shootWarnT = 0.f;
    float shootAim = 0.f;
    // 腐蚀粘液落痕节流
    float trailT = 0.f;
    // 棋盘：雷位、已揭示的安全格、已被踩过的雷
    uint32_t mineMask = 0;
    uint32_t revealedMask = 0;
    uint32_t triggeredMask = 0;

    void clear();
    bool generate(int centerX, int centerY, int boardOriginX, int boardOriginY, uint32_t poolSeed);

    bool contains(int tileX, int tileY) const;      // 圆形浅水区内
    bool inClearZone(int tileX, int tileY) const;   // 区域 + 余量
    bool onBoard(int tileX, int tileY) const;
    bool boardLocal(int tileX, int tileY, int& lx, int& ly) const;

    bool isMine(int lx, int ly) const;
    bool isRevealed(int lx, int ly) const;
    bool isTriggered(int lx, int ly) const;
    int adjacentMines(int lx, int ly) const;
    int safeTotal() const { return kBoardSize * kBoardSize - kMines; }
    int revealedCount() const;
    int triggeredCount() const;
    bool boardComplete() const;

    // 揭示一格：0 = 无事发生，1 = 揭开安全格（含 0 格连锁翻开），2 = 首次踩中雷
    int reveal(int tileX, int tileY);
    // 把指定的雷挪到别处（只用于开局第一格保证安全）
    void relocateMine(int fromX, int fromY);

    float centerX() const;
    float centerY() const;
};
