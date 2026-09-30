#pragma once

#include <cstdint>
#include <cmath>

inline constexpr int kTile = 16;
inline constexpr int kFrame = 64;
inline constexpr int kViewW = 480;
inline constexpr int kViewH = 270;
inline constexpr float kSimDt = 1.f / 60.f;

inline int tileOf(float v) {
    return static_cast<int>(std::floor(v / float(kTile)));
}

inline uint32_t mixHash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// 4-dir sheet rows: 0 down, 1 left, 2 right, 3 up.
// 8-dir Puny rows (clockwise from south): 0 S, 1 SE, 2 E, 3 NE, 4 N, 5 NW, 6 W, 7 SW.
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
    bool bEdge = false;
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
        bEdge = false;
        gEdge = false;
        ctrlEdge = false;
    }

    void clearHeld() {
        w = a = s = d = shift = lmb = rmb = false;
        moveX = moveY = 0.f;
        clearEdges();
    }
};
