#pragma once

#include "Audio.h"
#include "Codex.h"
#include "PathField.h"
#include "TileMap.h"
#include "Types.h"

#include <QJsonObject>
#include <QString>
#include <vector>

enum class ActorState { Idle, Run, Attack, Dodge, Hurt, Dead };
enum class MonsterKind { Slime, Skeleton, Mushroom, Flyer, Caster };
enum class HeroClass { Warrior, Sword, Mage };
enum class EndReason { None, Death, Settle };

struct Item {
    int slot = 0;
    int kind = 0;
    int power = 0;
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
    float facingX = 1.f;
    float facingY = 0.f;
    bool flip = false;
    int level = 1;
    float xp = 0.f;
    float animT = 0.f;
    float attackT = 0.f;
    bool heavy = false;
    float heavyCharge = 0.f;
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
    float burialT = 0.f;
    float burialR = 96.f;
    float mirrorT = 0.f;
    float mirrorAbsorbed = 0.f;
    float mirrorCap = 40.f;
    float mirrorMaxHit = 28.f;
    float berserkT = 0.f;
    int weaponAtk = 0;
    float speedBonus = 0.f;
    int critBonus = 0;
    bool talentMight = false;
    bool talentStride = false;
    bool talentLight = false;
    bool talentMastery = false;
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

enum class AttackFxKind { Ring, Cone, Crescent, Dash, Pulse, Mirror, Blink, Laser };

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
    static constexpr int kTrail = 8;
    float trailX[kTrail]{};
    float trailY[kTrail]{};
    int trailLen = 0;
    float trailAcc = 0.f;
};

struct Drop {
    float x = 0.f;
    float y = 0.f;
    Item item;
};

class Session {
public:
    void newGame(uint32_t seed, uint32_t runId, HeroClass hero = HeroClass::Warrior,
        int skillD = 0, int skillF = 1, int skillC = 2, int skillV = -1);
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
    const std::vector<Drop>& drops() const { return drops_; }
    const std::vector<FloatText>& floatTexts() const { return floats_; }
    const std::vector<AttackFx>& attackFx() const { return attackFx_; }
    const TileMap& map() const { return map_; }
    float cooldownMul() const { return cdMul(); }

    void queueSfx(SfxId id);
    std::vector<SfxId> drainSfx();
    bool consumeRecoverBgm();

    int xpToNext() const;

private:
    void gainXp(int amount);
    void note(const QString& text);
    void trackHpForBgm();
    void checkTalents();
    float rollDamage(float base, bool* critOut = nullptr);
    void hurtPlayer(float damage, Monster* source = nullptr);
    void hurtMonster(Monster& monster, float damage, float poiseDamage, bool crit = false, float knockback = 0.f);
    void pushFloat(float x, float y, float amount, bool crit);
    void pushFx(AttackFxKind kind, float radius, float halfAngle = 0.7f, float life = 0.35f);
    void updateFloats(float dt);
    void updateAttackFx(float dt);
    float scaledMonsterDamage(const Monster& monster, float base) const;
    void fireMageBolt(bool heavy);
    void fireMageLaser();
    void castHeavySwordQi();
    void slashHostileBolts();
    void tryMove(float& x, float& y, float vx, float vy, float dt, float radius, int pass, float* moved);
    void updatePlayer(float dt, const InputState& input, float mouseX, float mouseY);
    void updateMonsters(float dt);
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
    void castMirrorShield(float& cooldown);
    void castMageHeal(float& cooldown);
    void castBerserk(float& cooldown);
    void breakMirrorShield(const QString& reason);
    void toggleFlight();
    void updateFlight(float dt);
    void updateStackRegen(float dt);
    float cdMul() const;
    float atkSpeedMul() const;
    void recomputeGear();
    void spawn(float dt);
    void spawnMonster(MonsterKind kind, float x, float y);
    bool findSpawn(float& x, float& y);
    Monster* findMonster(int id);

    TileMap map_{1};
    PathField paths_;
    Player player_;
    std::vector<Monster> monsters_;
    std::vector<Bolt> bolts_;
    std::vector<Drop> drops_;
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
    uint32_t runId_ = 0;
    bool paused_ = false;
    EndReason reason_ = EndReason::None;
    uint32_t rng_ = 1;
    QString notice_;
    std::vector<SfxId> sfxQueue_;
    float damageSinceHeal_ = 0.f;
    bool bgmRecoverArmed_ = false;
    bool recoverBgmPending_ = false;
    float hpTrack_ = -1.f;
    uint32_t nextRand();
};
