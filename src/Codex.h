#pragma once

#include <QString>

struct SkillText {
    QString name;
    QString detail;
};

// 战士 / 剑客
inline constexpr int kSkillSpin = 0;
inline constexpr int kSkillSwordQi = 1;
inline constexpr int kSkillThrust = 2;
inline constexpr int kSkillBerserk = 3;
// 法师
inline constexpr int kSkillMageBolt = 10;
inline constexpr int kSkillNova = 11;
inline constexpr int kSkillFlight = 12;
inline constexpr int kSkillBurial = 13;
inline constexpr int kSkillMirror = 14;
inline constexpr int kSkillMageHeal = 15;

inline constexpr int kWarriorSkillPool[] = {
    kSkillSpin, kSkillSwordQi, kSkillThrust, kSkillBerserk};

inline constexpr int kMageSkillPool[] = {
    kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial, kSkillMirror, kSkillMageHeal};

inline bool isWarriorSkill(int id) {
    for (int skill : kWarriorSkillPool) {
        if (skill == id) {
            return true;
        }
    }
    return false;
}

inline bool isMageSkill(int id) {
    for (int skill : kMageSkillPool) {
        if (skill == id) {
            return true;
        }
    }
    return false;
}

inline SkillText skillText(int id) {
    switch (id) {
    case kSkillSpin:
        return {"回旋斩", "攻击身边 42 距离内敌人。基础伤害 16，破韧 12。消耗 16 MP，冷却 2.8 秒。"};
    case kSkillSwordQi:
        return {"剑气", "向前方斩出月牙剑气，距离 112。可叠层，最高 3 层；释放无间隔。每层冷却 1.5 秒。消耗 10 MP。"};
    case kSkillThrust:
        return {"突刺", "朝面向突进并伤害前方。可叠层，最高 3 层；释放无间隔。每层冷却 1.0 秒。消耗 8 MP。"};
    case kSkillBerserk:
        return {"狂化", "攻击力翻倍，攻速增加 100%，持续 3 秒。消耗 18 MP，冷却 8 秒。"};
    case kSkillMageBolt:
        return {"飞弹", "朝鼠标方向射出法弹。基础伤害 20。消耗 14 MP，冷却 1.6 秒。"};
    case kSkillNova:
        return {"新星", "攻击身边 64 距离内敌人。基础伤害 12，破韧 8。消耗 22 MP，冷却 4.2 秒。"};
    case kSkillFlight:
        return {"飞行", "开关飞行。优先消耗 STA，再消耗 MP。耗尽后落地。"};
    case kSkillBurial:
        return {"万葬", "以自身为中心大范围法阵，重创范围内敌人。消耗 40 MP，冷却 9 秒。"};
    case kSkillMirror:
        return {"逆反之盾", "吸收伤害并把每次伤害的 5% 反弹给攻击者。持续 10 秒，单次过高或累计达上限也会破碎。上限随等级提高。消耗 24 MP，冷却 15 秒。"};
    case kSkillMageHeal:
        return {"治疗术", "回复最大生命值的 15%。消耗 22 MP，冷却 20 秒。"};
    default:
        return {"未知", ""};
    }
}

inline SkillText guardSkillText() {
    return {"防御", "持续 0.75 秒，受到的伤害变为 35%。消耗 12 MP，冷却 3.6 秒。"};
}

inline SkillText healSkillText() {
    return {"恢复", "回复 22 HP 与 20 STA。消耗 20 MP，冷却 5.5 秒。"};
}

inline SkillText lightAttackText() {
    return {"轻击", "点按左键。近战基础伤害 11；法师发射飞弹伤害 12。造成击退。近战可劈砍敌方飞弹。"};
}

inline SkillText heavyAttackText() {
    return {"重击", "按住左键 0.42 秒后放出。战士/剑客：小剑气（距离 56，基础伤害 24）；法师：激光（穿透，基础伤害 22）。消耗 30 STA。"};
}

inline QString talentMightDetail() {
    return "造成的伤害变为 1.2 倍。累计造成 250 点伤害后获得。";
}

inline QString talentStrideDetail() {
    return "移动速度 +18。累计移动 900 距离后获得。";
}

inline QString talentLightDetail() {
    return "闪避消耗的 STA 从 24 降到 14。累计闪避 6 次后获得。";
}

inline QString talentMasteryDetail() {
    return "战斗熟练度：技能冷却 -25%，攻击速度 +25%。累计释放技能 12 次后获得。";
}
