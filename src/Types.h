#pragma once

#include <cstdint>
#include <cmath>

inline constexpr int kTile = 16;            // 一个瓦片 16 px：寻路 / 迷宫 / 扫雷等网格算法的离散单位
inline constexpr int kFrame = 64;           // 精灵帧边长（像素）
inline constexpr int kViewW = 480;          // 逻辑视口宽（像素）
inline constexpr int kViewH = 270;          // 逻辑视口高（像素）
inline constexpr float kSimDt = 1.f / 60.f; // 固定模拟步长：1/60 s

// 世界坐标 → 瓦片坐标：向下取整，负数也会正确落到左侧 / 上侧那一格。
inline int tileOf(float v) {
    return static_cast<int>(std::floor(v / float(kTile)));
}

// 32 位整数混合（murmur 风格）。全项目唯一的「随机源」，同时扮演两个角色：
//   1. 无状态噪声：mixHash(seed ^ ... ^ 坐标) 直接当「坐标 → 伪随机值」用；
//   2. 状态式 PRNG：下一状态 = mixHash(状态 + 常数)，同种子必得同序列。
inline uint32_t mixHash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// 4-dir sheet rows: 0 down, 1 left, 2 right, 3 up.
// 中文：4 方向图集的行序为 0 下、1 左、2 右、3 上。
// 8-dir Puny rows (clockwise from south): 0 S, 1 SE, 2 E, 3 NE, 4 N, 5 NW, 6 W, 7 SW.
// 中文：8 方向 Puny 图集的行序（从正南起顺时针）为 0 南、1 东南、2 东、3 东北、
//       4 北、5 西北、6 西、7 西南。
// 输入是朝向向量，输出是图集行号。
// 4 方向用「绝对分量谁大」判定；8 方向用 atan2 除以 45° 再四舍五入到 0..7。
inline int facingDir(float fx, float fy, int dirs = 4) {
    if (dirs >= 8) {
        float a = std::atan2(fx, fy);
        if (a < 0.f) {
            a += 6.28318530718f;
        }
        return static_cast<int>(std::round(a / 0.78539816339f)) % 8;
    }
    if (std::abs(fx) >= std::abs(fy)) {
        return fx < 0.f ? 1 : 2;
    }
    return fy < 0.f ? 3 : 0;
}

// 史莱姆之躯能幻化的外形：默认是自己的尸体（原本的躯体），其余是已吞噬的怪物
enum class MimicForm : int {
    Hero = 0,
    Slime,
    Skeleton,
    Mushroom,
    Flyer,
    Caster,
    Killbot,
    Eye,
    SlimeBoss,
    Count,
};

inline uint32_t mimicBit(MimicForm form) {
    return 1u << uint32_t(form);
}

struct InputState {
    bool w = false;
    bool a = false;
    bool s = false;
    bool d = false;
    bool shift = false;
    bool lmb = false;
    bool rmb = false;
    bool lmbEdge = false;
    bool lmbUp = false;
    bool rmbEdge = false;
    bool shiftEdge = false;
    bool escEdge = false;
    bool spaceEdge = false;
    bool qEdge = false;
    bool eEdge = false;
    bool rEdge = false;
    bool fEdge = false;
    bool cEdge = false;
    bool vEdge = false;
    bool gEdge = false;
    // 史莱姆之躯的拟态技能（左 Ctrl / 触屏按钮）
    bool ctrlEdge = false;
    // 虚拟摇杆：方向 × 力度，长度 0~1
    float moveX = 0.f;
    float moveY = 0.f;

    void clearEdges() {
        lmbEdge = false;
        lmbUp = false;
        rmbEdge = false;
        shiftEdge = false;
        escEdge = false;
        spaceEdge = false;
        qEdge = false;
        eEdge = false;
        rEdge = false;
        fEdge = false;
        cEdge = false;
        vEdge = false;
        gEdge = false;
        ctrlEdge = false;
    }

    void clearHeld() {
        w = a = s = d = shift = lmb = rmb = false;
        moveX = moveY = 0.f;
        clearEdges();
    }
};
