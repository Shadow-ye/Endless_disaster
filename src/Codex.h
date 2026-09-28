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
// 机甲人
inline constexpr int kSkillScatter = 20;
inline constexpr int kSkillMissile = 21;
inline constexpr int kSkillBoost = 22;
inline constexpr int kSkillOverload = 23;
inline constexpr int kSkillSwarm = 24;
inline constexpr int kSkillMagField = 25;
inline constexpr int kSkillMedkit = 26;
inline constexpr int kSkillJetpack = 27;
inline constexpr int kSkillSeek = 30;

inline constexpr int kWarriorSkillPool[] = {
    kSkillSpin, kSkillSwordQi, kSkillThrust, kSkillBerserk};

inline constexpr int kMageSkillPool[] = {
    kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial, kSkillMirror, kSkillMageHeal};

inline constexpr int kRobotSkillPool[] = {
    kSkillScatter, kSkillMissile, kSkillBoost, kSkillOverload, kSkillSwarm, kSkillMagField, kSkillMedkit, kSkillJetpack};

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

inline bool isRobotSkill(int id) {
    for (int skill : kRobotSkillPool) {
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
    case kSkillScatter:
        return {"散射", "朝面向扇形射出 5 发子弹，每发基础伤害 9。消耗 14 MP，冷却 2.4 秒。"};
    case kSkillMissile:
        return {"爆破弹", "射出榴弹，命中、撞墙或飞到尽头时爆炸，半径 44 内基础伤害 26，破韧 16。消耗 20 MP，冷却 4.5 秒。"};
    case kSkillBoost:
        return {"推进", "朝面向喷射突进，起步冲击波伤害身边 36 距离内敌人，基础伤害 10。突进中无敌。消耗 12 MP，冷却 3 秒。"};
    case kSkillOverload:
        return {"过载", "持续 10 秒：普攻与技能伤害 +40%，移速 +25%，射速 +60%，但其他技能耗蓝 +50%。消耗 18 MP，冷却 20 秒。"};
    case kSkillMagField:
        return {"磁力场", "展开磁力场 6 秒：受到的伤害只剩 5%，每 0.4 秒对贴身（半径 30）的怪物造成基础伤害 8 并轻微推开。消耗 20 MP，冷却 10 秒。"};
    case kSkillMedkit:
        return {"战术医疗包", "3 秒内持续回复 24% 最大生命。消耗 16 MP，冷却 16 秒。"};
    case kSkillJetpack:
        return {"喷气背包", "开关喷气背包，离地飞行，可越过岩石和灌木。每秒消耗 22，优先扣 STA，再扣 MP，耗尽后落地；飞行中 STA 不回复。"};
    case kSkillSwarm:
        return {"蜂群", "身边召唤 6 架小型无人机，成群撞向离自己最近的怪物并爆炸，半径 26 内基础伤害 11 并击退。无人机会被怪物子弹打爆，12 秒后自毁。消耗 22 MP，冷却 7 秒。"};
    case kSkillSeek:
        return {"寻路", "按 G 开关。雷达标出迷宫遗迹方向；身在迷宫中时，另一份地图实时画出进出中央广场的唯一路线。不消耗 MP。由天赋「世界指引」获得。"};
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
    return {"轻击", "点按左键。近战基础伤害 11；法师发射飞弹伤害 12；机甲人射出子弹伤害 10（射速快，弹匣 30 发）。造成击退。近战可劈砍敌方飞弹。"};
}

inline SkillText heavyAttackText() {
    return {"重击", "按住左键 0.42 秒后放出。战士/剑客：小剑气（距离 56，基础伤害 24）；法师：激光（穿透，基础伤害 22）。消耗 30 STA。机甲人没有重击，改为换弹：一次补满 30 发，不耗 STA。"};
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

inline QString talentGuideDetail() {
    return "击杀 20 个入侵世界的怪物后获得额外技能寻路，按 G 开关。";
}
