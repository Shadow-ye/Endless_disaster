#pragma once

#include "Audio.h"
#include "Codex.h"
#include "MazeRuin.h"
#include "PathField.h"
#include "ShallowPool.h"
#include "TileMap.h"
#include "Types.h"

#include <QJsonObject>
#include <QString>
#include <array>
#include <vector>

enum class ActorState { Idle, Run, Attack, Dodge, Hurt, Dead };
enum class MonsterKind { Slime, Skeleton, Mushroom, Flyer, Caster, Killbot, Eye, SlimeBoss };

// 拟态形态 ↔ 怪物类型（MimicForm::Hero 是玩家自己的尸体，没有对应的怪物）
inline MonsterKind mimicMonsterKind(MimicForm form) {
    switch (form) {
    case MimicForm::Skeleton:
        return MonsterKind::Skeleton;
    case MimicForm::Mushroom:
        return MonsterKind::Mushroom;
    case MimicForm::Flyer:
        return MonsterKind::Flyer;
    case MimicForm::Caster:
        return MonsterKind::Caster;
    case MimicForm::Killbot:
        return MonsterKind::Killbot;
    case MimicForm::Eye:
        return MonsterKind::Eye;
    case MimicForm::SlimeBoss:
        return MonsterKind::SlimeBoss;
    default:
        return MonsterKind::Slime;
    }
}

inline MimicForm mimicFormOf(MonsterKind kind) {
    switch (kind) {
    case MonsterKind::Skeleton:
        return MimicForm::Skeleton;
    case MonsterKind::Mushroom:
        return MimicForm::Mushroom;
    case MonsterKind::Flyer:
        return MimicForm::Flyer;
    case MonsterKind::Caster:
        return MimicForm::Caster;
    case MonsterKind::Killbot:
        return MimicForm::Killbot;
    case MonsterKind::Eye:
        return MimicForm::Eye;
    case MonsterKind::SlimeBoss:
        return MimicForm::SlimeBoss;
    default:
        return MimicForm::Slime;
    }
}

// 史莱姆之躯拟态期间临时获得的属性加成；全部是乘算倍率，1 表示无加成
struct MimicBonus {
    float maxHp = 1.f;
    float armor = 1.f;
    float poise = 1.f;
    float speed = 1.f;
    float cooldown = 1.f;  // <1 表示冷却缩短
    float atkSpeed = 1.f;
    float damage = 1.f;
    float manaRegen = 1.f;  // 回蓝速度倍率
    int crit = 0;  // 暴击率百分点
};
enum class HeroClass { Warrior, Sword, Mage, Robot };

// 「继续前进」配音按角色性别选男声 / 女声：女剑客与女魔法师用女声，战士与机甲人用男声
inline bool femaleHero(HeroClass hero) {
    return hero == HeroClass::Sword || hero == HeroClass::Mage;
}
enum class EndReason { None, Death, Settle };
// 意识回归符咒：None = 未触发，Offered = 已弹出选择，Declined = 玩家拒绝回归
enum class ReviveState { None, Offered, Declined };

inline constexpr int kRobotMagazine = 30;

// 近战扇形：判定用 dot > kMeleeConeDot，特效按同一张角画，保证画面和作用范围一致
inline constexpr float kMeleeConeDot = 0.35f;
inline constexpr float kMeleeConeHalf = 1.213f;  // std::acos(0.35f)

// 万葬两段的节奏：第一段法阵铺开后延后出伤，法阵结束换六芒星再出第二段
inline constexpr float kBurialStage1Life = 0.75f;
inline constexpr float kBurialStage1Hit = 0.25f;
inline constexpr float kBurialStage2Life = 0.65f;
inline constexpr float kBurialStage2Hit = 0.2f;

// 抹杀法阵：第 i 个（0 起）的半径 = kEraseRingBase × (i + 1)，最后一个刚好铺满整屏。
// squash 是俯视透视的竖向压扁率，绘制层据此把圆盘画成带立体感的地面法阵
inline constexpr float kEraseRingBase = 62.f;
inline constexpr float kEraseRingSquash = 0.42f;

// I am atomic 三段节奏：蓄力 → 闪白 → 引爆；引爆一开始就抹除画面内的怪
// 蓄力 4 秒是对着配音来的：整段 5 秒，前 4 秒吟唱，最后一秒是引爆
inline constexpr float kAtomicChargeLife = 4.f;
inline constexpr float kAtomicFlashLife = 0.12f;
inline constexpr float kAtomicBlastLife = 1.2f;
inline constexpr float kAtomicBlastHit = 0.02f;
// 「画面内」按视野矩形判定：以玩家为中心，取一屏的半宽半高
inline constexpr float kAtomicHalfW = 240.f;
inline constexpr float kAtomicHalfH = 135.f;
// 引爆把地表烧成沙地的半径（格），以及顺带炸塌迷宫墙的作用半径
inline constexpr int kAtomicScorchTiles = 7;
inline constexpr float kAtomicWallBreak = 112.f;
// 蓄力时那圈紫色网格的扫描半径：要盖住整个视野（480×270），
// 扫描带会一圈圈由内向外反复推出去，直到蓄力结束
inline constexpr float kAtomicGridRange = 360.f;
// 吟唱字幕的时间轴：I 亮 1 秒、空 1 秒、am 亮 1 秒、再空 1 秒（正好铺满 4 秒蓄力），
// 引爆瞬间甩出 atomic，停 2 秒
inline constexpr float kAtomicChantWord = 1.f;
inline constexpr float kAtomicChantGap = 1.f;
inline constexpr float kAtomicChantFinal = 2.f;

// 隐藏强化普攻「次元斩」：战士 / 女剑客带上突刺后，长按闪避 0.5 秒开始判定（配音起、BGM 让位）；
// 10 秒内打完 3 次突刺 + 3 次普攻 + 1 次跳跃算判定成功，此后从长按闪避那一刻起算 20 秒内，
// 下一次普攻或重击打出次元斩。超时 / 差一招都算判定失败。
inline constexpr float kDimHoldTime = 0.5f;
inline constexpr float kDimComboWindow = 10.f;
inline constexpr float kDimDeadline = 20.f;
inline constexpr int kDimThrustNeed = 3;
inline constexpr int kDimLightNeed = 3;
inline constexpr int kDimJumpNeed = 1;
// 次元斩只吃体力
inline constexpr float kDimStaminaCost = 30.f;
// 次元斩本体：角色沿六芒星边移动边斩，六芒星六个顶点各一斩（每斩中心都不同），
// 斩完回原位停顿一拍，再一起结算伤害，最后光刃与滤镜一起收掉。
inline constexpr int kDimBladeCount = 6;          // 六芒星六斩
inline constexpr float kDimGap = 0.55f;           // 每一斩的间隔（要慢）
inline constexpr float kDimStarRadius = 44.f;     // 六芒星半径
inline constexpr float kDimDashTime = float(kDimBladeCount) * kDimGap;  // 移动覆盖整个斩击阶段
inline constexpr float kDimReturnTime = 0.3f;     // 斩完回原位
inline constexpr float kDimPauseTime = 0.6f;      // 回原位后停顿（停顿结束才结算伤害）
inline constexpr float kDimFadeTime = 0.5f;       // 光刃一起消失 / 滤镜从四周回收
inline constexpr float kDimSettleTime = kDimDashTime + kDimReturnTime + kDimPauseTime;
inline constexpr float kDimSlashLife = kDimSettleTime + kDimFadeTime;
inline constexpr float kDimFilterExpand = 0.35f;  // 蓝滤镜由中心铺开用多久
inline constexpr float kDimBladeAppear = 0.14f;   // 单道刃「斩出去」用多久
// 斩完之后配音再多留一秒（界面那边收掉配音后，还要再空一拍才把 BGM 渐入回来）
inline constexpr float kDimVoiceTail = 1.f;
// 单道刃的判定：以刃线为中心的细长条，长度足够横贯整屏
inline constexpr float kDimBladeRange = 260.f;
inline constexpr float kDimBladeWidth = 26.f;
inline constexpr float kDimBladeDamage = 26.f;
inline constexpr float kDimBladePoise = 16.f;
// 时缓强度（其余单位的时间缩放）与尾段回正常速的时长
inline constexpr float kDimSlowScale = 0.22f;
inline constexpr float kDimSlowTail = 0.45f;

// 次元斩的一斩：中心落在六芒星顶点上（每斩各不相同），刃线方向自成一路
struct DimBlade {
    float x = 0.f;
    float y = 0.f;
    float dx = 1.f;
    float dy = 0.f;
    float age = 0.f;  // 出现后过了多久，用来画「这一斩推开」的展开
};

struct Item {
    int slot = 0;
    int kind = 0;
    int power = 0;
    // 可堆叠物品的叠加个数；同名物品在背包里只占一个条目
    int count = 1;
    QString name;
};

struct Player {
    float x = 8.f;
    float y = 8.f;
    float hp = 100.f;
    float maxHp = 100.f;
    float mp = 80.f;
    float maxMp = 80.f;
    float stamina = 100.f;
    float maxStamina = 100.f;
    float shield = 25.f;
    float maxShield = 25.f;
    float armor = 10.f;
    // 理智（SAN）：激活 boss 后开始流逝，10 分钟耗尽即死（不可复活）；击败 boss 恢复正常
    float san = 100.f;
    float maxSan = 100.f;
    bool sanWeak = false;
    float facingX = 1.f;
    float facingY = 0.f;
    bool flip = false;
    int level = 1;
    float xp = 0.f;
    float animT = 0.f;
    float attackT = 0.f;
    bool heavy = false;
    // 本次攻击是近战挥击（战士 / 剑客普攻、机甲人「肘击」技能键）：命中判定走身前扇形
    bool meleeSwing = false;
    float heavyCharge = 0.f;
    int ammo = kRobotMagazine;
    // 长按换弹后到松开左键之前不再蓄力，松开也不开火
    bool reloadLatch = false;
    int attackId = 0;
    float dodgeT = 0.f;
    float dodgeCd = 0.f;
    float dodgeX = 1.f;
    float dodgeY = 0.f;
    float jumpT = 0.f;
    float guardT = 0.f;
    float invuln = 0.f;
    float hurtT = 0.f;
    float cdGuard = 0.f;
    float cdHeal = 0.f;
    float cdD = 0.f;
    float cdF = 0.f;
    float cdC = 0.f;
    float cdV = 0.f;
    int lockId = 0;
    float damageDealt = 0.f;
    float distanceMoved = 0.f;
    int dodgeCount = 0;
    int skillCasts = 0;
    HeroClass hero = HeroClass::Warrior;
    int skillD = kSkillSpin;
    int skillF = kSkillSwordQi;
    int skillC = kSkillThrust;
    int skillV = -1;
    int stacksQi = 3;
    int stacksThrust = 3;
    float cdQiStack = 0.f;
    float cdThrustStack = 0.f;
    bool flying = false;
    bool seekOn = false;
    // 法师「雷罚」：开启期间锁在雷雨天、落雷加密、每秒扣蓝
    bool thunderPunish = false;
    // 万葬：1 = 第一段法阵，2 = 第二段六芒星；burialNext 是本段出伤的剩余时间
    float burialT = 0.f;
    float burialR = 96.f;
    int burialStage = 0;
    float burialNext = 0.f;
    // I am atomic：1 = 蓄力，2 = 闪白，3 = 引爆；atomicNext 是引爆出伤的剩余时间
    float atomicT = 0.f;
    int atomicStage = 0;
    float atomicNext = 0.f;
    // 这一发 I am atomic 实际定下的冷却时长，只给 UI 画冷却环当分母用
    float atomicCd = 0.f;
    // 抹杀：1 = 前摇中；eraseT 是剩余前摇时间（绘制层据此逐个铺开法阵）
    int eraseStage = 0;
    float eraseT = 0.f;
    float mirrorT = 0.f;
    float mirrorAbsorbed = 0.f;
    float mirrorCap = 40.f;
    float mirrorMaxHit = 28.f;
    float berserkT = 0.f;
    float overloadT = 0.f;
    float fieldT = 0.f;
    float fieldTick = 0.f;
    float medkitT = 0.f;
    int weaponAtk = 0;
    float speedBonus = 0.f;
    int critBonus = 0;
    bool talentMight = false;
    bool talentStride = false;
    bool talentLight = false;
    bool talentMastery = false;
    bool talentGuide = false;
    bool talentUnderdog = false;
    // 用意识回归符咒复活后带上的本轮诅咒：余光时不时盯上你，刷出精英怪
    bool cursed = false;
    // 史莱姆之躯：使用史莱姆核心复活后永久生效（一次使用整局生效）
    bool slimeBody = false;
    // 当前幻化的外形；史莱姆之躯默认是自己的史莱姆形象
    MimicForm mimic = MimicForm::Slime;
    // 已吞噬的形态（位掩码，见 mimicBit）；自己的尸体与史莱姆本体默认已吞噬，开局就带 Hero 与 Slime 位
    uint32_t devouredMask = mimicBit(MimicForm::Hero) | mimicBit(MimicForm::Slime);
    // 史莱姆之躯下累计吞噬的怪物数，达到 kGluttonyDevours 进化出天赋【暴食】
    int devourCount = 0;
    bool talentGluttony = false;
    // 拟态技能的冷却
    float cdMimicSkill = 0.f;
    // 飞虫形态的短时滞空剩余时间
    float mimicFlyT = 0.f;
    // 蘑菇形态跳砸 / 腐化史莱姆形态冲撞的进行中计时
    float mimicSlamT = 0.f;
    float mimicChargeT = 0.f;
    int worldKills = 0;
    int underdogKills = 0;
    ActorState state = ActorState::Idle;
};

struct Monster {
    int id = 0;
    MonsterKind kind = MonsterKind::Slime;
    int level = 1;
    float x = 0.f;
    float y = 0.f;
    float hp = 1.f;
    float maxHp = 1.f;
    float shield = 0.f;
    float maxShield = 0.f;
    float poise = 1.f;
    float maxPoise = 1.f;
    float facingX = -1.f;
    float facingY = 0.f;
    bool flip = false;
    float animT = 0.f;
    float hurtT = 0.f;
    float stunT = 0.f;
    float defenseT = 0.f;
    float attackT = 0.f;
    bool attackApplied = false;
    float contactCd = 0.f;
    float lungeT = 0.f;
    int lastHitBy = 0;
    bool scored = false;
    // 诅咒召来的精英：血量翻倍，死亡积分翻倍
    bool elite = false;
    ActorState state = ActorState::Idle;
};

struct FloatText {
    float x = 0.f;
    float y = 0.f;
    float life = 0.f;
    float vy = -28.f;
    float amount = 0.f;
    bool crit = false;
};

enum class AttackFxKind { Ring, Cone, Crescent, Dash, Pulse, Mirror, Blink, Laser, Spin, Slash, Burst, Pillar, Qi, Lunge, Mushroom, Grid };

struct AttackFx {
    AttackFxKind kind = AttackFxKind::Ring;
    float x = 0.f;
    float y = 0.f;
    float fx = 1.f;
    float fy = 0.f;
    float life = 0.3f;
    float maxLife = 0.3f;
    float radius = 40.f;
    float halfAngle = 0.7f;
    // 0xRRGGBB；0 表示用该类型的默认色
    uint32_t color = 0;
};

// 只给绘制层生成粒子用，不参与任何判定
enum class VfxKind { Hit, Kill, Explode, Heal, LevelUp, Rage, AtomicCharge, AtomicFlash, AtomicBlast, Corrosion, Rise, Hail, Thunder };

// 天气系统：局内周期性轮换的六种天灾，退场后有一段平静期。
// Clear 是平静期（也是「天气系统」关掉后的状态），粒子和滤镜都不画。
enum class WeatherKind { Clear = 0, Rain, Thunder, Blizzard, Hail, Sandstorm, Typhoon };

inline QString weatherName(WeatherKind kind) {
    switch (kind) {
    case WeatherKind::Rain:
        return QStringLiteral("雨天");
    case WeatherKind::Thunder:
        return QStringLiteral("雷雨天");
    case WeatherKind::Blizzard:
        return QStringLiteral("暴风雪");
    case WeatherKind::Hail:
        return QStringLiteral("冰雹");
    case WeatherKind::Sandstorm:
        return QStringLiteral("沙尘暴");
    case WeatherKind::Typhoon:
        return QStringLiteral("台风");
    default:
        return QStringLiteral("晴");
    }
}

// 天气的一句话效果说明，给暂停菜单与说明页用
inline QString weatherDetail(WeatherKind kind) {
    switch (kind) {
    case WeatherKind::Rain:
        return QStringLiteral("细雨打湿地面，视野微微发暗。");
    case WeatherKind::Thunder:
        return QStringLiteral("密集雨幕里不时落下闪电：白闪过后，落点附近的怪物挨一记雷击。");
    case WeatherKind::Blizzard:
        return QStringLiteral("狂风卷着雪片扫过，白茫茫一片看不清远处。");
    case WeatherKind::Hail:
        return QStringLiteral("冰雹砸在地上四处迸溅，被砸中的怪物持续掉一点血。");
    case WeatherKind::Sandstorm:
        return QStringLiteral("黄沙贴着地面横卷，视野被沙幕压低。");
    case WeatherKind::Typhoon:
        return QStringLiteral("台风过境：暴雨斜扫、飞叶乱舞，风一阵紧过一阵。");
    default:
        return QStringLiteral("风平浪静。");
    }
}

// 一句话短说明：法师「改天」的选择面板上每个按钮的第二行
inline QString weatherBrief(WeatherKind kind) {
    switch (kind) {
    case WeatherKind::Rain:
        return QStringLiteral("细雨压暗视野");
    case WeatherKind::Thunder:
        return QStringLiteral("雨幕里落下雷击");
    case WeatherKind::Blizzard:
        return QStringLiteral("白茫一片看不清");
    case WeatherKind::Hail:
        return QStringLiteral("冰雹砸中掉血");
    case WeatherKind::Sandstorm:
        return QStringLiteral("沙幕贴着地横卷");
    case WeatherKind::Typhoon:
        return QStringLiteral("暴雨斜扫飞叶乱舞");
    default:
        return QStringLiteral("风平浪静");
    }
}

// 天气的时间轴：首次出现前的等待、单次持续、退场后的平静期、强度渐入渐出
inline constexpr float kWeatherFirstDelay = 22.f;
inline constexpr float kWeatherRamp = 3.5f;
inline constexpr float kWeatherLifeMin = 30.f;
inline constexpr float kWeatherLifeMax = 55.f;
inline constexpr float kWeatherCalmMin = 9.f;
inline constexpr float kWeatherCalmMax = 17.f;
// 落雷：只劈玩家附近的怪，伤害随怪物等级微涨；劈完地面留一记闪光与烟尘
inline constexpr float kLightningRange = 150.f;
inline constexpr float kLightningRadius = 34.f;
inline constexpr float kLightningLife = 0.34f;
// 同时劈下的雷最多几道（法师「雷罚」一次三道，普通雷雨天一道）
inline constexpr int kMaxLightningBolts = 3;

// 正在劈下的一道雷：世界坐标 + 年龄，绘制层按年龄淡出
struct LightningBolt {
    bool active = false;
    float x = 0.f;
    float y = 0.f;
    float age = 0.f;
};
// 冰雹碎击：视野内的怪物被砸中掉一点血
inline constexpr float kHailRange = 130.f;

struct VfxEvent {
    VfxKind kind = VfxKind::Hit;
    float x = 0.f;
    float y = 0.f;
    float radius = 0.f;
    bool crit = false;
    MonsterKind monster = MonsterKind::Slime;
};

struct Bolt {
    float x = 0.f;
    float y = 0.f;
    float vx = 0.f;
    float vy = 0.f;
    float life = 0.f;
    float age = 0.f;
    float damage = 0.f;
    bool hostile = false;
    bool crit = false;
    bool mage = false;
    bool robot = false;
    // 腐蚀水弹：绿色弹体，飞行中拖出腐蚀尾迹（画法与普通敌弹区分开）
    bool corrosion = false;
    // >0 时为爆破弹：命中、撞墙或寿命耗尽时按此半径范围伤害
    float blast = 0.f;
    // 仅绘制时上移；判定仍按脚底平面，避免枪口高度让弹道与目标错开
    float lift = 0.f;
    static constexpr int kTrail = 8;
    float trailX[kTrail]{};
    float trailY[kTrail]{};
    int trailLen = 0;
    float trailAcc = 0.f;
};

// 蜂群无人机：坐标在地面平面，飞行高度只影响绘制
struct Drone {
    float x = 0.f;
    float y = 0.f;
    float vx = 0.f;
    float vy = 0.f;
    float life = 0.f;
    float age = 0.f;
    int slot = 0;
    float damage = 0.f;
    bool crit = false;
};

struct Drop {
    float x = 0.f;
    float y = 0.f;
    Item item;
};

// —— 传送平台
// 玩家跑出迷宫遗迹 / 浅水 boss 房一定距离后，会在玩家附近、画面之外随机刷出一块 3×3 的
// 符文平台，并标出它通往哪里；站上去起跳即传送过去，省得在开阔地图上长距离跋涉。
inline constexpr int kTeleportPadTiles = 3;
// 目的地离玩家超过这么多格才给传送平台；已经快到跟前就没必要再放了
inline constexpr float kTeleportPadMinDist = 34.f;

enum class TeleportTarget { Ruin, Pool, Count };

inline QString teleportTargetName(TeleportTarget target) {
    return target == TeleportTarget::Pool ? QStringLiteral("浅水区域") : QStringLiteral("迷宫遗迹");
}

struct TeleportPad {
    bool active = false;
    TeleportTarget target = TeleportTarget::Ruin;
    // 平台左上角所在格，占 kTeleportPadTiles × kTeleportPadTiles
    int tileX = 0;
    int tileY = 0;
    // 目的地（世界像素）：生成时定下，之后不再跟着玩家跑
    float destX = 0.f;
    float destY = 0.f;

    float centerX() const { return (float(tileX) + kTeleportPadTiles * 0.5f) * float(kTile); }
    float centerY() const { return (float(tileY) + kTeleportPadTiles * 0.5f) * float(kTile); }
};

class Session {
public:
    void newGame(uint32_t seed, uint32_t runId, HeroClass hero = HeroClass::Warrior,
        int skillD = 0, int skillF = 1, int skillC = 2, int skillV = -1, bool guideAtStart = false);
    // 开局准备的分步入口：按 reset → spawns → ruin 依次调用与 newGame() 完全等价。
    // 加载页借此把开局的重活摊到多帧执行，避免进入游戏时整帧卡顿。
    void newGameStepReset(uint32_t seed, uint32_t runId, HeroClass hero = HeroClass::Warrior,
        int skillD = 0, int skillF = 1, int skillC = 2, int skillV = -1, bool guideAtStart = false);
    void newGameStepSpawns();
    // 生成迷宫遗迹；找不到合适位置时返回 false（本局地图上没有遗迹）
    bool newGameStepRuin();
    bool loadFrom(const QJsonObject& game);
    QJsonObject toJson() const;

    void update(float dt, const InputState& input, float mouseX, float mouseY);
    void settle();

    bool paused() const { return paused_; }
    void setPaused(bool paused) { paused_ = paused; }

    bool ended() const { return reason_ != EndReason::None; }
    EndReason reason() const { return reason_; }
    uint32_t runId() const { return runId_; }
    uint32_t seed() const { return map_.seed(); }
    float time() const { return time_; }
    int score() const { return score_; }
    QString pullNotice();

    const Player& player() const { return player_; }
    const std::vector<Monster>& monsters() const { return monsters_; }
    const std::vector<Bolt>& bolts() const { return bolts_; }
    const std::vector<Drone>& drones() const { return drones_; }
    // 蜂群锁定的目标：起手时钉住它，准星画在它身上；目标死了 / 无人机散尽后返回 nullptr
    const Monster* droneLockTarget() const;
    // 锁定准星的进度：0 = 刚起手正在收拢，数值越大越接近「已咬住」（绘制层自己钳制）
    float droneLockAge() const { return droneLockAge_; }
    // 目标丢失后的余辉 0..1：准星停在最后的位置上散掉
    float droneLockFade() const { return droneLockFade_; }
    // 准星位置：锁定时跟随目标，丢失后就停在最后的位置
    void droneLockPos(float& x, float& y) const { x = droneLockX_; y = droneLockY_; }
    const std::vector<Drop>& drops() const { return drops_; }
    const std::vector<FloatText>& floatTexts() const { return floats_; }
    const std::vector<AttackFx>& attackFx() const { return attackFx_; }
    const TileMap& map() const { return map_; }
    const MazeRuin& ruin() const { return ruin_; }
    const ShallowPool& pool() const { return pool_; }
    // 场上现存的传送平台（至多每个目的地一个），界面用它画世界层平台与画面外指示
    const std::vector<TeleportPad>& teleportPads() const { return pads_; }
    // 本次浅水 boss 战里踩雷累计削减掉的理智上限
    float sanCapCut() const { return pool_.sanCut; }
    float plazaRed() const { return plazaRed_; }
    // I am atomic 的画面紫色滤镜：蓄力时涨起来，余波里退掉
    float atomicViolet() const { return atomicViolet_; }
    // —— 隐藏强化普攻「次元斩」
    // 战士 / 女剑客且技能槽里带了突刺才解锁（隐藏技能的前提）；界面据此决定要不要写这段连招说明
    bool dimensionUnlocked() const;
    // 判定进行中（长按闪避已触发，正在 10 秒连招窗口里）
    bool dimensionJudging() const { return dimOpen_; }
    // 连招已完成，等着用普攻 / 重击打出次元斩
    bool dimensionReady() const { return dimReady_; }
    float dimensionWindowLeft() const { return dimWindowT_; }
    float dimensionDeadlineLeft() const { return kDimDeadline - dimSinceDodge_ > 0.f ? kDimDeadline - dimSinceDodge_ : 0.f; }
    int dimensionThrustCount() const { return dimThrust_; }
    int dimensionLightCount() const { return dimLight_; }
    int dimensionJumpCount() const { return dimJump_; }
    // 次元斩的浅蓝滤镜圈：由中心向外展开，最后从四周回收
    float dimensionFilterReveal() const { return dimFilterR_; }
    // 次元斩正在斩（画面要沿刃线错位、除玩家外都时缓、相机锁在起手点）
    bool dimensionSlashing() const { return dimSlashT_ > 0.f; }
    // 当前还留在画面上的每一斩：中心各不相同，直到整场结束时一起消失
    const std::vector<DimBlade>& dimensionBlades() const { return dimBlades_; }
    // 一斩「推开」用多久；以及全场的整体强度（末尾一起消失）
    float dimensionBladeAppear() const { return kDimBladeAppear; }
    float dimensionBladeFade() const { return dimBladeFade_; }
    // 次元斩期间相机锁在起手处，角色才能在画面里沿六芒星跑
    void dimensionCameraAnchor(float& x, float& y) const { x = dimCamX_; y = dimCamY_; }
    // 六芒星上的第 index 个落点：0 是起手点，1..6 是六个顶点（界面用它画出跑动轨迹）
    void dimensionNodeAt(int index, float& x, float& y) const { dimensionNode(index, x, y); }
    // 长按闪避触发了有效判定（界面据此起配音、压掉 BGM）
    bool consumeDimensionVoiceStart();
    // 判定失败 / 死亡 / 超时：界面据此停配音、立刻恢复 BGM
    bool consumeDimensionVoiceStop();
    // 次元斩正常打完（再留一秒）：界面据此停配音，隔一拍后让 BGM 渐入
    bool consumeDimensionVoiceFinish();
    // 角色头顶的吟唱字幕，一个词一个词往外蹦；空串表示这会儿不显示
    QString atomicChant() const;
    void cameraShake(float& sx, float& sy) const;
    float cooldownMul() const { return cdMul(); }

    void queueSfx(SfxId id);
    std::vector<SfxId> drainSfx();
    std::vector<VfxEvent> drainVfx();
    bool consumeRecoverBgm();
    bool consumeVoidPrompt();

    // 意识回归符咒：死亡时由 Session 触发，交给界面弹选择框
    bool consumeRevivePrompt();
    bool acceptRevive();
    void declineRevive();
    // 转化为史莱姆之躯（默认史莱姆形象，自己的尸体默认已吞噬）
    void transformIntoSlimeBody();
    // 史莱姆之躯下击杀怪物即吞噬：记录形态、推进【暴食】进度
    void devour(MonsterKind kind);
    // 左 Ctrl / 触屏按钮：按当前拟态形态放一次技能
    void castMimicSkill();

    // 背包内容：只读，给界面的背包面板按获得顺序列出物品
    const std::vector<Item>& bag() const { return bag_; }
    int itemCount(int kind) const;
    int talismanCount() const { return itemCount(kItemReturnTalisman); }
    int slimeCoreCount() const { return itemCount(kItemSlimeCore); }
    // 诅咒「存在被克苏鲁余光注意！」：本轮永久，偶尔刷出双倍血量的精英怪
    bool cursed() const { return player_.cursed; }

    // 史莱姆之躯：使用史莱姆核心复活后永久生效
    bool slimeBody() const { return player_.slimeBody; }
    MimicForm mimicForm() const { return player_.mimic; }
    bool devoured(MimicForm form) const { return (player_.devouredMask & mimicBit(form)) != 0u; }
    int devourCount() const { return player_.devourCount; }
    bool talentGluttony() const { return player_.talentGluttony; }
    // 当前形态带来的临时属性加成（未转化 / 原本的躯体时全是 1）
    MimicBonus mimicBonus() const;
    float mimicSkillCooldown() const { return player_.cdMimicSkill; }
    float mimicSkillCooldownMax() const;
    // 图鉴里点选形态；未吞噬的形态不会生效
    void setMimicForm(MimicForm form);
    // 史莱姆核心只能搭着意识回归符咒一起用：两个都持有才算可用；
    // 已经是史莱姆之躯后核心不再生效，复活选项里也不该再出现
    bool canUseSlimeCore() const { return !player_.slimeBody && talismanCount() > 0 && slimeCoreCount() > 0; }
    bool acceptReviveWithSlimeCore();
    // 结算时符咒折算出来的积分，只在结算面板上用
    int talismanBonus() const { return talismanBonus_; }

    // 理智（SAN）系统：boss 激活后流逝，归零即死（不可复活）
    bool sanActive() const { return sanActive_; }
    float san() const { return player_.san; }
    float maxSan() const { return player_.maxSan; }
    bool sanWeak() const { return player_.sanWeak; }
    // 玩家理论理智上限（不含踩雷削减）：UI 用它显示上限被削掉了多少
    float sanCapBase() const { return sanCapBase_ > 0.f ? sanCapBase_ : player_.maxSan; }
    // 距理智耗尽还剩多少秒。流速固定是「基准上限 / 600」，
    // 所以被踩雷削掉的上限会实打实地缩短这个倒计时。
    float sanRemaining() const {
        const float drain = sanCapBase() / 600.f;
        return drain <= 0.f ? 0.f : player_.san / drain;
    }

    // —— 天气系统
    // 当前天气（Clear 表示平静期 / 天气系统已关闭）
    WeatherKind weather() const { return weather_; }
    // 天气强度：刚起时从 0 渐入到 1，退场时渐出，绘制层按它缩放粒子数与滤镜浓度
    float weatherIntensity() const { return weatherIntensity_; }
    // 天气是否正在场上（含渐入渐出），界面据此决定要不要显示天气状态条
    bool weatherOn() const { return weather_ != WeatherKind::Clear || weatherIntensity_ > 0.02f; }
    // 当前天气剩余秒数；平静期返回距离下一次天气的秒数
    float weatherRemain() const { return std::max(0.f, weatherT_); }
    bool weatherEnabled() const { return weatherEnabled_; }
    // 设置里的「天气系统」开关：关掉立刻转入平静期，也不再轮换
    void setWeatherEnabled(bool on);
    // —— 法师技能「改天」：释放后由画面层弹出天气选择面板
    // 是否要弹面板；取走一次就清掉，画面层每帧轮询
    bool consumeWeatherPickRequest();
    // 面板是否处于选择中（取消、换局、关掉天气系统都会结束）
    bool weatherPickOpen() const { return weatherPickOpen_; }
    // 选好某种天气：这时才扣 MP、进冷却并切换天气；MP 不足则失败且不消耗
    bool chooseWeather(WeatherKind kind);
    // 放弃选择：不扣 MP，也不进冷却
    void cancelWeatherPick();
    // 落雷的一次白闪（0~1），画面据此叠一层白光
    float lightningFlash() const { return lightningFlash_; }
    // 正在劈下的雷（最多 kMaxLightningBolts 道），界面逐道画成折线闪电
    const std::array<LightningBolt, kMaxLightningBolts>& lightningBolts() const { return lightningBolts_; }
    // 法师「雷罚」是否开着：开启期间落雷加密、一次三道，并持续耗蓝
    bool thunderPunish() const { return player_.thunderPunish; }

    int xpToNext() const;

private:
    void addItem(int kind, int count = 1);
    bool consumeItem(int kind, int count = 1);
    void convertTalismansToScore();
    void finishRun(EndReason reason);
    void revivePlayer();
    void gainXp(int amount);
    void note(const QString& text);
    void trackHpForBgm();
    void checkTalents();
    float rollDamage(float base, bool* critOut = nullptr);
    void hurtPlayer(float damage, Monster* source = nullptr);
    void hurtMonster(Monster& monster, float damage, float poiseDamage, bool crit = false, float knockback = 0.f);
    void pushFloat(float x, float y, float amount, bool crit);
    void pushFx(AttackFxKind kind, float radius, float halfAngle = 0.7f, float life = 0.35f, uint32_t color = 0);
    // 战士/剑客普攻与机甲人「肘击」共用同一道挥砍特效
    void pushSlashFx();
    void queueVfx(VfxKind kind, float x, float y, float radius = 0.f, bool crit = false, MonsterKind monster = MonsterKind::Slime);
    void updateFloats(float dt);
    void updateAttackFx(float dt);
    float scaledMonsterDamage(const Monster& monster, float base) const;
    void fireMageBolt(bool heavy);
    void fireMageLaser();
    void castHeavySwordQi();
    void slashHostileBolts();
    bool canBreakMazeWalls() const;
    bool breakMazeWallTile(int tileX, int tileY);
    void commitBrokenWalls(bool broken);
    void breakMazeWallsCone(float range, float minDot);
    void breakMazeWallsRadius(float x, float y, float radius);
    void breakMazeWallsBeam(float range, float halfWidth);
    void applyEyeLevel();
    // 分轴移动：先试 X 轴、再试 Y 轴，被拒绝的只有撞墙的那个分量，另一个仍生效，
    // 于是斜向撞墙时会沿墙滑行而不是卡死。moved 非空时累加实际位移（用于判断「有没有动」）。
    void tryMove(float& x, float& y, float vx, float vy, float dt, float radius, int pass, float* moved);
    int playerPass() const;
    // 脚下这格不可走（岩石 / 灌木 / 水）时，给出朝玩家更近一格的可走邻格
    bool nearestWalkableTile(int tileX, int tileY, int& outX, int& outY) const;
    void updatePlayer(float dt, const InputState& input, float mouseX, float mouseY);
    void updateMonsters(float dt);
    // 画面外：跨过岩石和灌木直追，被水或迷宫墙挡住时再绕行，并按离画面的距离加速
    bool chaseOffscreen(Monster& monster, float dt);
    void updateBolts(float dt);
    void castSlot(int skill, float& cooldown);
    void castSpin();
    void castBolt();
    void castStrike();
    void castNova();
    void castWave();
    void castSwordQi();
    void castThrustStack();
    void castBurial(float& cooldown);
    void burialBlast(int stage);
    void updateBurial(float dt);
    // 抹杀：起手扣 50% 最大 MP → 5 秒前摇（不能动）→ 随机抹杀当前一半敌人；
    // 前摇被打断只保留 10% 的消耗（退回 40%）
    void castErase(float& cooldown);
    void updateErase(float dt);
    void eraseBlast();
    void interruptErase();
    void castAtomic(float& cooldown);
    void atomicBlast();
    void updateAtomic(float dt);
    // —— 隐藏强化普攻「次元斩」
    // 判定与出招：长按闪避计时、10 秒连招窗口、20 秒期限、沿六芒星逐斩
    void updateDimension(float dt, const InputState& input);
    // 次元斩的时间线：六芒星位移 + 逐斩展开 + 回位停顿 + 一起结算 + 滤镜回收
    void updateDimensionSlash(float elapsed, float dt);
    void startDimensionJudge();
    void failDimensionJudge();
    void clearDimension();
    bool dimensionComboReady() const;
    void castDimensionSlash();
    // 六芒星第 index 个落点（0 是起手点，1..6 是六个顶点）
    void dimensionNode(int index, float& x, float& y) const;
    // 一斩：落在六芒星第 index 段末尾的顶点上
    void dimensionSlash(int index);
    // 六斩一起结算伤害
    void dimensionSettle();
    // 除玩家外的世界时间缩放（次元斩期间 <1）
    float dimensionSlowScale() const;
    void castMirrorShield(float& cooldown);
    void castMageHeal(float& cooldown);
    // 改天：只置起「等画面层弹面板」的状态，不扣 MP、不进冷却
    void requestWeatherPick(float& cooldown);
    // 雷罚：开关雷雨天、每秒扣蓝；结束时转雨天并把天气交还给正常轮换
    void toggleThunderPunish();
    void updateThunderPunish(float dt);
    void endThunderPunish(bool toRain);
    void castBerserk(float& cooldown, const QString& name);
    void castOverload(float& cooldown);
    void castMagField(float& cooldown);
    void castMedkit(float& cooldown);
    void updateRobotBuffs(float dt);
    // 机甲人带了「肘击」：技能键挥出一次近战挥击；普攻不受影响，仍是点射
    bool robotMelee() const;
    void swingMelee();
    void castMelee(float& cooldown);
    float skillCostMul() const { return player_.overloadT > 0.f ? 1.5f : 1.f; }
    void fireRobotShot(float angleOffset, float baseDamage);
    void castScatter();
    void castMissile();
    void castBoost();
    void explodeBolt(const Bolt& bolt);
    void castSwarm();
    void updateDrones(float dt);
    // 蜂群锁定用的目标筛选：玩家周围 range 内最近的活怪
    const Monster* nearestMonster(float range) const;
    // 钉住一个目标：重置准星动画，并在换目标时播锁定音
    void acquireDroneLock(const Monster& target);
    // 每帧推进准星：跟着目标走，目标没了就退回余辉
    void updateDroneLock(float dt);
    void explodeDrone(const Drone& drone, bool harmful);
    void updateKillbot(Monster& monster, float dt, float dist);
    bool rangedHero() const { return player_.hero == HeroClass::Mage || player_.hero == HeroClass::Robot; }
    void breakMirrorShield(const QString& reason);
    void toggleFlight();
    void updateFlight(float dt);
    void updateStackRegen(float dt);
    float cdMul() const;
    float atkSpeedMul() const;
    void recomputeGear();
    void spawn(float dt);
    void spawnMonster(MonsterKind kind, float x, float y);
    void spawnElite();
    bool findSpawn(float& x, float& y);
    bool spawnRuin();
    void dismissRuin();
    void syncRuinMap();
    void updateRuin(float dt);
    // 理智流逝：boss 激活后按 10 分钟线性清零，归零即死（不可复活）
    void updateSan(float dt);
    // 天气：调度轮换 + 强度渐入渐出 + 雷雨天落雷 / 冰雹碎击（都只砸怪，不伤玩家）
    void updateWeather(float dt);
    void rollWeather();
    // 落雷：默认一道；雷罚一次三道。每道各自挑目标、各自结算伤害
    void strikeLightning(int count = 1);
    // 只是把一道雷放进画面槽位（最多三道），伤害与闪光由 strikeLightning 负责
    void spawnLightningBolt(float x, float y);
    // 清空所有正在劈的雷（换天气 / 重开一局）
    void clearLightningBolts();
    void hailStrike();
    void spawnEye(float hp = -1.f, float shield = -1.f, int level = -1);
    void updateEye(Monster& monster, float dt);
    void onEyeDefeated();
    // 浅水 boss 房：圆形浅水区 + 旁边的排雷小游戏 + 巨型腐化史莱姆
    void updatePool(float dt);
    bool spawnPool();
    void dismissPool();
    void syncPoolMap();
    void spawnSlimeBoss(float hp = -1.f, float shield = -1.f, int level = -1);
    void updateSlimeBoss(Monster& monster, float dt);
    void onSlimeBossDefeated();
    void restorePool(const QJsonObject& game);
    // 跳跃揭示棋盘格：安全格点亮数字，踩雷受伤并削减本场理智上限
    void revealBoardTile(int tileX, int tileY);
    void applySlimeBossLevel();
    // 腐蚀地面：boss 拖痕 / 拟态冲撞 / 腐蚀弹落点都会把格子改成腐蚀地形，
    // 踩上去的玩家（史莱姆之躯、滞空、有护盾时除外）与怪物（史莱姆一族、飞行的除外）
    // 持续微量掉血
    void updateCorrosion(float dt);
    void hurtPlayerByCorrosion();
    void addCorrosion(float x, float y);
    void slimeBossClampToPool(Monster& monster) const;
    void slimeBossContact(Monster& monster, float dist, float damage);
    void slimeBossTrail(Monster& monster, float dt);
    // 踩雷削减 / 击败恢复理智上限
    void applySanCut();
    void restoreSanCap();
    // 浅水减速：史莱姆与飞行单位免疫
    float shallowMoveMul(float x, float y, bool immune) const;
    void triggerShake();
    void restoreRuin(const QJsonObject& game);
    Monster* findMonster(int id);
    // —— 传送平台
    // 目的地还在场上（遗迹立着 / 浅水区开着）
    bool teleportTargetLive(TeleportTarget target) const;
    // 目的地世界坐标：遗迹取入口那一格，浅水区取外圈清空带上朝玩家这一侧
    bool teleportDestination(TeleportTarget target, float& x, float& y) const;
    // 在玩家附近、画面之外随机找一块整块可站的 3×3 空地放平台
    bool placeTeleportPad(TeleportPad& pad, TeleportTarget target);
    // 把落点吸附到最近的可站格（先就地，再按环形向外找）
    bool snapToWalkable(float& x, float& y, int radiusTiles) const;
    // 每帧维护平台：目的地没了就撤、玩家走远了就重新刷、踩上去起跳就传送
    void updateTeleportPads(float dt);
    void useTeleportPad(TeleportPad& pad);

    TileMap map_{1};
    MazeRuin ruin_;
    ShallowPool pool_;
    // 怪物寻路流场：以玩家所在瓦片为源点，全场怪物共享同一张父指针表。
    // 属于纯派生状态，不进存档 —— 读档后按玩家位置重建一次即可。
    PathField paths_;
    Player player_;
    std::vector<Monster> monsters_;
    std::vector<Bolt> bolts_;
    std::vector<Drone> drones_;
    // 蜂群锁定：id 为 -1 表示没锁；age / fade 供绘制层做收拢与余辉动画
    int droneLockId_ = -1;
    float droneLockAge_ = 0.f;
    float droneLockFade_ = 0.f;
    float droneLockX_ = 0.f;
    float droneLockY_ = 0.f;
    // 换目标时锁定音的最短间隔，免得一瞬间刷出好几声
    float droneRelockCd_ = 0.f;
    std::vector<Drop> drops_;
    std::vector<FloatText> floats_;
    std::vector<AttackFx> attackFx_;
    std::vector<Item> bag_;
    Item equipped_[3]{};
    float baseMaxHp_ = 100.f;
    float baseMaxMp_ = 80.f;
    float baseArmor_ = 10.f;
    int nextId_ = 1;
    // 上一次重建流场时玩家所在的瓦片。和当前瓦片不同就说明场已过期，需要重建。
    // 初值 / 传送后置为 999999 这个「不可能坐标」哨兵，保证下一帧必定重建。
    int pathTileX_ = 999999;
    int pathTileY_ = 999999;
    float time_ = 0.f;
    int score_ = 0;
    float spawnCd_ = 1.2f;
    float flyerCd_ = 50.f;
    float eliteCd_ = 0.f;
    uint32_t runId_ = 0;
    bool paused_ = false;
    EndReason reason_ = EndReason::None;
    uint32_t rng_ = 1;
    QString notice_;
    std::vector<SfxId> sfxQueue_;
    std::vector<VfxEvent> vfxQueue_;
    float damageSinceHeal_ = 0.f;
    bool bgmRecoverArmed_ = false;
    bool recoverBgmPending_ = false;
    float hpTrack_ = -1.f;
    float shakeT_ = 0.f;
    float plazaRed_ = 0.f;
    float atomicViolet_ = 0.f;
    float atomicChantT_ = 0.f;
    // —— 隐藏强化普攻「次元斩」
    bool dimHoldArmed_ = false;  // 这次按住闪避是不是由一次成功闪避起头的
    float dimHoldT_ = 0.f;       // 闪避键已经按住多久
    bool dimOpen_ = false;       // 判定中：10 秒连招窗口
    bool dimReady_ = false;      // 连招已完成，等下一次普攻 / 重击
    float dimWindowT_ = 0.f;     // 连招窗口剩余
    float dimSinceDodge_ = 0.f;  // 距长按闪避触发过了多久（20 秒上限）
    int dimThrust_ = 0;
    int dimLight_ = 0;
    int dimJump_ = 0;
    int dimBladeOut_ = 0;        // 已经斩出的刃数
    bool dimSettled_ = false;    // 这一场是否已经结算过伤害
    float dimSlashT_ = 0.f;      // 次元斩剩余时间
    float dimSlashLife_ = 0.f;
    float dimFilterR_ = 0.f;     // 蓝滤镜圈的展开度（0 中心 → 1 铺满）
    float dimBladeFade_ = 1.f;   // 全场的整体强度，末尾一起消失
    float dimCamX_ = 0.f;        // 起手点：次元斩期间相机锁在这里
    float dimCamY_ = 0.f;
    std::vector<DimBlade> dimBlades_;
    // 配音开关：判定开始时起配音，失败时叫停，打完再留一秒后交给界面收尾
    bool dimVoiceOn_ = false;
    bool dimVoiceStart_ = false;
    bool dimVoiceStop_ = false;
    bool dimVoiceFinish_ = false;
    float dimVoiceTail_ = 0.f;
    bool voidPrompt_ = false;
    int eyeDefeats_ = 0;
    // 玩家理论理智上限（不含踩雷削减）；effective 上限是 player_.maxSan
    float sanCapBase_ = 100.f;
    // 腐蚀地面的持续伤害节流
    float corrosionTick_ = 0.f;
    int wallStrikeId_ = -1;
    // 理智系统会话状态：sanActive_ 一旦 boss 激活便锁定为真，击败或死亡才解除
    bool sanActive_ = false;
    // sanKill_ 为真时本次死亡由理智崩溃造成，结算时禁止意识回归符咒复活
    bool sanKill_ = false;
    ReviveState reviveState_ = ReviveState::None;
    bool revivePrompt_ = false;
    int talismanBonus_ = 0;
    // —— 天气系统
    // 设置里的「天气系统」开关，默认开
    bool weatherEnabled_ = true;
    WeatherKind weather_ = WeatherKind::Clear;
    // 上一次出现过的天气：轮换时避开它，免得连着两场一样
    WeatherKind lastWeather_ = WeatherKind::Clear;
    float weatherT_ = kWeatherFirstDelay;  // 当前阶段（天气 / 平静期）剩余时间
    float weatherIntensity_ = 0.f;         // 0~1，粒子与滤镜按它缩放
    float lightningCd_ = 0.f;              // 距下一次落雷
    float lightningFlash_ = 0.f;           // 白闪强度，随时间衰减
    std::array<LightningBolt, kMaxLightningBolts> lightningBolts_{};
    float hailCd_ = 0.f;  // 距下一次冰雹碎击
    // —— 法师技能「改天」：等画面层弹面板 / 面板已打开 / 这次是哪个技能键（0~3 = D/F/C/V）
    bool weatherPickRequest_ = false;
    bool weatherPickOpen_ = false;
    int weatherPickSlot_ = -1;
    // —— 传送平台
    std::vector<TeleportPad> pads_;
    // 用掉平台后，对同一目的地的再次刷新冷却（秒）
    float padCd_[static_cast<int>(TeleportTarget::Count)]{};
    uint32_t nextRand();
};
