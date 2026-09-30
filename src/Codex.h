#pragma once

#include "Types.h"

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
inline constexpr int kSkillAtomic = 4;
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
inline constexpr int kSkillMelee = 28;
inline constexpr int kSkillSeek = 30;

// 机甲人「近战」：一次挥击消耗的体力与技能键冷却
inline constexpr float kMeleeStaminaCost = 8.f;
inline constexpr float kMeleeCooldown = 0.9f;

// 战士「I am atomic」：一次倾泻当前全部 MP，冷却按这一发烧掉的 MP 占上限的比例减免
inline constexpr float kAtomicMinMp = 30.f;      // 释放门槛，低于此值放不出来
inline constexpr float kAtomicCdMax = 30.f;      // 刚好只够门槛时的冷却
inline constexpr float kAtomicCdMin = 12.f;      // 满 MP 释放时的冷却

inline constexpr int kWarriorSkillPool[] = {
    kSkillSpin, kSkillSwordQi, kSkillThrust, kSkillBerserk, kSkillAtomic};

inline constexpr int kMageSkillPool[] = {
    kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial, kSkillMirror, kSkillMageHeal};

inline constexpr int kRobotSkillPool[] = {
    kSkillScatter, kSkillMissile, kSkillBoost, kSkillOverload, kSkillSwarm, kSkillMagField, kSkillMedkit, kSkillJetpack,
    kSkillMelee};

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
    case kSkillAtomic:
        return {"I am atomic",
            "核级引爆：倾泻当前全部 MP，直接抹除画面内所有怪物，包括克苏鲁之眼。至少需要 "
                + QString::number(int(kAtomicMinMp))
                + " MP 才能释放；烧掉的 MP 越多冷却越短，满 MP 时 "
                + QString::number(int(kAtomicCdMin)) + " 秒，只够门槛时 "
                + QString::number(int(kAtomicCdMax)) + " 秒。"};
    case kSkillMageBolt:
        return {"飞弹", "朝鼠标方向射出法弹。基础伤害 20。消耗 14 MP，冷却 1.6 秒。"};
    case kSkillNova:
        return {"新星", "攻击身边 64 距离内敌人。基础伤害 12，破韧 8。消耗 22 MP，冷却 4.2 秒。"};
    case kSkillFlight:
        return {"飞行", "开关飞行。优先消耗 STA，再消耗 MP。耗尽后落地。"};
    case kSkillBurial:
        return {"万葬", "以自身为中心大范围法阵，伤害分两段：法阵铺开 0.25 秒后第一段，第一段特效结束时六芒星落下第二段。消耗 40 MP，冷却 9 秒。"};
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
        return {"蜂群", "身边召唤 6 架小型无人机，成群撞向离自己最近的怪物并爆炸，半径 26 内基础伤害 11 并击退。无人机会被怪物子弹打爆，撞上迷宫墙壁也会爆炸，12 秒后自毁。消耗 22 MP，冷却 7 秒。"};
    case kSkillMelee:
        return {"肘击", "按下本技能键用肘部撞击一次：特效与战士普攻一致，攻击身前 34 距离内扇形范围的敌人，基础伤害 11，并能劈掉敌方飞弹。每次消耗 8 STA，不消耗 MP，也不消耗弹药，冷却 0.9 秒。普攻不受影响，仍是点射。"};
    case kSkillSeek:
        return {"寻路", "按 G 开关（安卓点右上角「寻路」）。雷达标出迷宫遗迹方向；进入迷宫会自动显示迷宫地图，开启后地图上才画出进出中央广场的唯一路线。不消耗 MP。由天赋「世界指引」获得。"};
    default:
        return {"未知", ""};
    }
}

inline SkillText guardSkillText() {
    return {"防御", "持续 0.75 秒，受到的伤害变为 35%。消耗 12 MP，冷却 3.6 秒。"};
}

// 史莱姆之躯把「恢复」改成按比例回复
inline SkillText healSkillText(bool slimeBody = false) {
    if (slimeBody) {
        return {"恢复", "史莱姆之躯：回复 20% 最大生命与 20% 最大体力。消耗 20 MP，冷却 5.5 秒。"};
    }
    return {"恢复", "回复 22 HP 与 20 STA。消耗 20 MP，冷却 5.5 秒。"};
}

inline SkillText lightAttackText() {
    return {"轻击", "点按左键。近战基础伤害 11；法师发射飞弹伤害 12；机甲人射出子弹伤害 10（射速快，弹匣 30 发，长按左键换弹）。造成击退。近战可劈砍敌方飞弹。"};
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

inline QString talentUnderdogDetail() {
    return "对等级高于自己的敌人，造成的伤害变为 1.3 倍。击杀 10 个等级超过自己的怪物后获得。";
}

inline QString talentGluttonyDetail() {
    return "【暴食】史莱姆之躯下累计吞噬 100 只怪物后获得：经验获取翻倍。";
}

// 史莱姆之躯：图鉴里每个形态的名字、左Ctrl技能与属性加成
struct MimicText {
    QString name;
    QString skillName;
    QString skill;
    QString bonus;
};

inline MimicText mimicText(MimicForm form) {
    switch (form) {
    case MimicForm::Hero:
        return {"原本的躯体", "无", "把外形换回你原本的角色，不附带额外技能。", "无属性加成。"};
    case MimicForm::Slime:
        return {"史莱姆", "腐蚀喷吐",
            "腐蚀喷吐（左 Ctrl，冷却 4 秒）：朝鼠标吐出腐蚀黏液弹，命中造成伤害并在地面留下腐蚀粘液。",
            "最大生命 +25%。"};
    case MimicForm::Skeleton:
        return {"骷髅", "骨刺突进",
            "骨刺突进（左 Ctrl，冷却 4 秒）：朝鼠标突进一段，并伤害身前扇形内的敌人。",
            "护甲 +50%。"};
    case MimicForm::Mushroom:
        return {"蘑菇", "毒孢跳砸",
            "毒孢跳砸（左 Ctrl，冷却 4 秒）：跃起后砸向鼠标方向，落点范围伤害。",
            "破韧 +30%。"};
    case MimicForm::Flyer:
        return {"飞虫", "振翅",
            "振翅（左 Ctrl，冷却 4 秒）：短时滞空飞行约 2.5 秒，可越过岩石与灌木，并免疫腐蚀粘液。",
            "移速 +25%。"};
    case MimicForm::Caster:
        return {"术士", "奥术符弹",
            "奥术符弹（左 Ctrl，冷却 4 秒）：射出穿透法弹。",
            "技能冷却 -25%。"};
    case MimicForm::Killbot:
        return {"杀手机器人", "榴弹",
            "榴弹（左 Ctrl，冷却 4 秒）：射出爆破弹，命中、撞墙或飞到尽头时范围爆炸。",
            "攻击速度 +30%。"};
    case MimicForm::Eye:
        return {"克苏鲁之眼【投影】", "血环",
            "血环（左 Ctrl，冷却 6 秒）：以自身为中心炸开一圈血环，大范围伤害。",
            "暴击率 +15%。"};
    case MimicForm::SlimeBoss:
        return {"巨型腐化史莱姆", "腐化冲撞",
            "腐化冲撞（左 Ctrl，冷却 6 秒）：朝鼠标高速冲撞，沿途撞飞敌人并留下腐蚀粘液。",
            "伤害 +40%。"};
    default:
        return {"未知形态", "无", "", ""};
    }
}

// 物品编号：Item::kind 用它，可堆叠的同类物品在背包里合并成一个条目，按 count 计数
inline constexpr int kItemNone = 0;
inline constexpr int kItemReturnTalisman = 1;
inline constexpr int kItemSlimeCore = 2;

// 意识回归符咒：结算时每张折算的积分
inline constexpr int kTalismanScore = 500;
// 史莱姆核心：结算时每个折算的积分（比符咒略低，但也能变现）
inline constexpr int kSlimeCoreScore = 400;
// 史莱姆之躯下吞噬多少只怪物进化出天赋【暴食】
inline constexpr int kGluttonyDevours = 100;

struct ItemText {
    QString name;
    QString detail;
    bool stackable = true;
};

inline ItemText itemText(int id) {
    switch (id) {
    case kItemReturnTalisman:
        return {"意识回归符咒",
            QString("克苏鲁之眼【投影】掉落，可堆叠。死亡时被动触发：可以选择确认回归（原地复活，生命恢复到 25%）或拒绝回归（直接结算）。"
                    "确认回归会带上本轮永久诅咒「存在被克苏鲁余光注意！」：此后偶尔刷出双倍血量的精英怪物。"
                    "若同时持有史莱姆核心，还能选择「使用史莱姆核心回归」，转化为史莱姆之躯（会额外消耗 1 个核心）。"
                    "结算时每张折算 %1 积分。")
                .arg(kTalismanScore),
            true};
    case kItemSlimeCore:
        return {"史莱姆核心",
            QString("巨型腐化史莱姆掉落，可堆叠。只在使用意识回归符咒复活时可选择使用（一次使用整局生效，所以也只能用一次）："
                    "消耗 1 张符咒与 1 个核心，原地复活并转化为「史莱姆之躯」——"
                    "受到伤害 -10%，「恢复」改为按比例回复；获得额外技能「拟态」（按 T，时停打开怪物图鉴），"
                    "可幻化为吞噬过的怪物，并暂时获得该形态的一个技能（左 Ctrl）与一项属性加成。"
                    "史莱姆之躯下累计吞噬 %1 只怪物可进化出天赋【暴食】（经验翻倍）。"
                    "结算时每个折算 %2 积分。")
                .arg(kGluttonyDevours)
                .arg(kSlimeCoreScore),
            true};
    default:
        return {"未知", "", false};
    }
}
