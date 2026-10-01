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
// 蓄力时那圈紫色网格的扫描半径
inline constexpr float kAtomicGridRange = 190.f;
// 吟唱字幕的时间轴：I 亮 1 秒、空 1 秒、am 亮 1 秒、再空 1 秒（正好铺满 4 秒蓄力），
// 引爆瞬间甩出 atomic，停 2 秒
inline constexpr float kAtomicChantWord = 1.f;
inline constexpr float kAtomicChantGap = 1.f;
inline constexpr float kAtomicChantFinal = 2.f;

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
    // 已吞噬的形态（位掩码，见 mimicBit）；自己的尸体默认已吞噬，所以开局就带 Hero 位
    uint32_t devouredMask = mimicBit(MimicForm::Hero);
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
enum class VfxKind { Hit, Kill, Explode, Heal, LevelUp, Rage, AtomicCharge, AtomicFlash, AtomicBlast, Corrosion, Rise };

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

// 腐蚀粘液：boss 经过处留下的地形，踩上去持续微量掉血（护盾 / 飞行 / 跳跃可免疫）。
// 只在地面上停留 kLife 秒，到点自行消失。
struct SlimeSpot {
    static constexpr float kLife = 3.f;
    float x = 0.f;
    float y = 0.f;
    float radius = 15.f;
    // 已经存在了多久；绘制时用 age / kLife 做呼吸与渐隐
    float age = 0.f;
};

class Session {
public:
    void newGame(uint32_t seed, uint32_t runId, HeroClass hero = HeroClass::Warrior,
        int skillD = 0, int skillF = 1, int skillC = 2, int skillV = -1, bool guideAtStart = false);
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
    const std::vector<Drop>& drops() const { return drops_; }
    const std::vector<FloatText>& floatTexts() const { return floats_; }
    const std::vector<AttackFx>& attackFx() const { return attackFx_; }
    const TileMap& map() const { return map_; }
    const MazeRuin& ruin() const { return ruin_; }
    const ShallowPool& pool() const { return pool_; }
    const std::vector<SlimeSpot>& slimeSpots() const { return slime_; }
    // 本次浅水 boss 战里踩雷累计削减掉的理智上限
    float sanCapCut() const { return pool_.sanCut; }
    float plazaRed() const { return plazaRed_; }
    // I am atomic 的画面紫色滤镜：蓄力时涨起来，余波里退掉
    float atomicViolet() const { return atomicViolet_; }
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
    void castAtomic(float& cooldown);
    void atomicBlast();
    void updateAtomic(float dt);
    void castMirrorShield(float& cooldown);
    void castMageHeal(float& cooldown);
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
    // 腐蚀粘液：boss 经过处留痕，对踩上去的玩家持续微量掉血
    void updateCorrosion(float dt);
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

    TileMap map_{1};
    MazeRuin ruin_;
    ShallowPool pool_;
    PathField paths_;
    Player player_;
    std::vector<Monster> monsters_;
    std::vector<Bolt> bolts_;
    std::vector<Drone> drones_;
    std::vector<Drop> drops_;
    std::vector<SlimeSpot> slime_;
    std::vector<FloatText> floats_;
    std::vector<AttackFx> attackFx_;
    std::vector<Item> bag_;
    Item equipped_[3]{};
    float baseMaxHp_ = 100.f;
    float baseMaxMp_ = 80.f;
    float baseArmor_ = 10.f;
    int nextId_ = 1;
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
    bool voidPrompt_ = false;
    int eyeDefeats_ = 0;
    // 玩家理论理智上限（不含踩雷削减）；effective 上限是 player_.maxSan
    float sanCapBase_ = 100.f;
    // 腐蚀粘液的持续伤害节流
    float corrosionTick_ = 0.f;
    int wallStrikeId_ = -1;
    // 理智系统会话状态：sanActive_ 一旦 boss 激活便锁定为真，击败或死亡才解除
    bool sanActive_ = false;
    // sanKill_ 为真时本次死亡由理智崩溃造成，结算时禁止意识回归符咒复活
    bool sanKill_ = false;
    ReviveState reviveState_ = ReviveState::None;
    bool revivePrompt_ = false;
    int talismanBonus_ = 0;
    uint32_t nextRand();
};
