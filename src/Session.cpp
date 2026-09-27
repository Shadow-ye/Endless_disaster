#include "Session.h"

#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kPlayerRadius = 7.f;
constexpr float kMonsterRadius = 7.f;

float lengthOf(float x, float y) {
    return std::sqrt(x * x + y * y);
}

void faceToward(float& fx, float& fy, bool& flip, float dx, float dy) {
    const float d = lengthOf(dx, dy);
    if (d < 0.001f) {
        return;
    }
    fx = dx / d;
    fy = dy / d;
    if (fx < -0.2f) {
        flip = true;
    } else if (fx > 0.2f) {
        flip = false;
    }
}

void setupMonster(Monster& monster, int level) {
    monster.level = std::max(1, level);
    switch (monster.kind) {
    case MonsterKind::Skeleton:
        monster.maxHp = 30.f;
        monster.maxShield = 14.f;
        monster.maxPoise = 16.f;
        break;
    case MonsterKind::Mushroom:
        monster.maxHp = 48.f;
        monster.maxShield = 10.f;
        monster.maxPoise = 22.f;
        break;
    case MonsterKind::Flyer:
        monster.maxHp = 72.f;
        monster.maxShield = 18.f;
        monster.maxPoise = 20.f;
        break;
    case MonsterKind::Caster:
        monster.maxHp = 26.f;
        monster.maxShield = 8.f;
        monster.maxPoise = 10.f;
        break;
    case MonsterKind::Slime:
        monster.maxHp = 16.f;
        monster.maxShield = 0.f;
        monster.maxPoise = 8.f;
        break;
    }
    const float scale = 1.f + float(monster.level - 1) * 0.14f;
    monster.maxHp *= scale;
    monster.maxShield *= scale;
    monster.maxPoise *= scale;
    monster.hp = monster.maxHp;
    monster.shield = monster.maxShield;
    monster.poise = monster.maxPoise;
}

int scoreFor(MonsterKind kind, int level) {
    int base = 10;
    switch (kind) {
    case MonsterKind::Skeleton:
        base = 25;
        break;
    case MonsterKind::Mushroom:
        base = 40;
        break;
    case MonsterKind::Flyer:
        base = 80;
        break;
    case MonsterKind::Caster:
        base = 30;
        break;
    case MonsterKind::Slime:
        base = 10;
        break;
    }
    return base + (std::max(1, level) - 1) * 3;
}
}  // namespace

uint32_t Session::nextRand() {
    rng_ = mixHash(rng_ + 0x9e3779b9u);
    return rng_;
}

int Session::xpToNext() const {
    return 12 + (player_.level - 1) * 6;
}

QString Session::pullNotice() {
    QString text = notice_;
    notice_.clear();
    return text;
}

void Session::note(const QString& text) {
    notice_ = text;
}

void Session::queueSfx(SfxId id) {
    if (sfxQueue_.size() < 24) {
        sfxQueue_.push_back(id);
    }
}

std::vector<SfxId> Session::drainSfx() {
    std::vector<SfxId> out;
    out.swap(sfxQueue_);
    return out;
}

bool Session::consumeRecoverBgm() {
    if (!recoverBgmPending_) {
        return false;
    }
    recoverBgmPending_ = false;
    return true;
}

void Session::trackHpForBgm() {
    if (hpTrack_ < 0.f) {
        hpTrack_ = player_.hp;
        return;
    }
    if (player_.hp < hpTrack_ - 0.01f) {
        damageSinceHeal_ += hpTrack_ - player_.hp;
        if (damageSinceHeal_ >= player_.maxHp * 0.2f) {
            bgmRecoverArmed_ = true;
        }
    } else if (player_.hp > hpTrack_ + 0.01f) {
        if (bgmRecoverArmed_) {
            recoverBgmPending_ = true;
            bgmRecoverArmed_ = false;
            damageSinceHeal_ = 0.f;
        }
    }
    hpTrack_ = player_.hp;
}

void Session::newGame(uint32_t seed, uint32_t runId, HeroClass hero, int skillD, int skillF, int skillC, int skillV) {
    map_ = TileMap(seed);
    player_ = Player{};
    player_.x = 8.f;
    player_.y = 8.f;
    player_.hero = hero;
    if (hero == HeroClass::Mage) {
        player_.skillD = kSkillMageBolt;
        player_.skillF = kSkillNova;
        player_.skillC = kSkillFlight;
        player_.skillV = kSkillBurial;
        if (isMageSkill(skillD)) {
            player_.skillD = skillD;
        }
        if (isMageSkill(skillF)) {
            player_.skillF = skillF;
        }
        if (isMageSkill(skillC)) {
            player_.skillC = skillC;
        }
        if (isMageSkill(skillV)) {
            player_.skillV = skillV;
        }
    } else {
        player_.skillD = kSkillSpin;
        player_.skillF = kSkillSwordQi;
        player_.skillC = kSkillThrust;
        player_.skillV = -1;
        if (isWarriorSkill(skillD)) {
            player_.skillD = skillD;
        }
        if (isWarriorSkill(skillF)) {
            player_.skillF = skillF;
        }
        if (isWarriorSkill(skillC)) {
            player_.skillC = skillC;
        }
    }
    player_.stacksQi = 3;
    player_.stacksThrust = 3;
    baseMaxHp_ = 100.f;
    baseMaxMp_ = 80.f;
    baseArmor_ = 10.f;
    player_.speedBonus = 0.f;
    player_.critBonus = 0;
    if (hero == HeroClass::Warrior) {
        baseMaxHp_ = 120.f;
        baseArmor_ = 14.f;
        baseMaxMp_ = 60.f;
    } else if (hero == HeroClass::Sword) {
        baseMaxHp_ = 95.f;
        baseArmor_ = 8.f;
        baseMaxMp_ = 85.f;
        player_.speedBonus = 16.f;
        player_.critBonus = 10;
    } else {
        baseMaxHp_ = 78.f;
        baseArmor_ = 4.f;
        baseMaxMp_ = 130.f;
    }
    bag_.clear();
    drops_.clear();
    equipped_[0] = equipped_[1] = equipped_[2] = Item{};
    recomputeGear();
    player_.hp = player_.maxHp;
    player_.mp = player_.maxMp;
    player_.stamina = player_.maxStamina;
    damageSinceHeal_ = 0.f;
    bgmRecoverArmed_ = false;
    recoverBgmPending_ = false;
    hpTrack_ = player_.maxHp;
    monsters_.clear();
    bolts_.clear();
    drops_.clear();
    floats_.clear();
    attackFx_.clear();
    bag_.clear();
    nextId_ = 1;
    pathTileX_ = 999999;
    pathTileY_ = 999999;
    time_ = 0.f;
    score_ = 0;
    spawnCd_ = 0.4f;
    flyerCd_ = 45.f;
    runId_ = runId;
    paused_ = false;
    reason_ = EndReason::None;
    notice_.clear();
    rng_ = seed ^ runId ^ 0xA5A5u;
    for (int i = 0; i < 3; ++i) {
        float x = 0.f;
        float y = 0.f;
        if (findSpawn(x, y)) {
            spawnMonster(MonsterKind::Slime, x, y);
        }
    }
    paths_.rebuild(map_, tileOf(player_.x), tileOf(player_.y));
    pathTileX_ = tileOf(player_.x);
    pathTileY_ = tileOf(player_.y);
}

bool Session::loadFrom(const QJsonObject& game) {
    const uint32_t seed = uint32_t(game.value("seed").toDouble());
    const uint32_t runId = uint32_t(game.value("runId").toDouble());
    const QJsonObject savedPlayer = game.value("player").toObject();
    const HeroClass hero = HeroClass(std::clamp(savedPlayer.value("hero").toInt(), 0, 2));
    newGame(seed == 0 ? 1u : seed, runId == 0 ? 1u : runId, hero,
        savedPlayer.value("skillD").toInt(0), savedPlayer.value("skillF").toInt(1),
        savedPlayer.value("skillC").toInt(2), savedPlayer.value("skillV").toInt(-1));
    monsters_.clear();
    time_ = float(game.value("time").toDouble());
    score_ = game.value("score").toInt();
    spawnCd_ = float(game.value("spawnCd").toDouble(1.0));
    flyerCd_ = float(game.value("flyerCd").toDouble(45.0));
    const QJsonObject p = game.value("player").toObject();
    player_.x = float(p.value("x").toDouble(8));
    player_.y = float(p.value("y").toDouble(8));
    player_.hp = float(p.value("hp").toDouble(100));
    player_.maxHp = float(p.value("maxHp").toDouble(100));
    player_.mp = float(p.value("mp").toDouble(player_.maxMp));
    player_.maxMp = float(p.value("maxMp").toDouble(80));
    player_.stamina = float(p.value("stamina").toDouble(100));
    player_.shield = float(p.value("shield").toDouble(player_.maxShield));
    player_.maxShield = float(p.value("maxShield").toDouble(25));
    player_.level = std::max(1, p.value("level").toInt(1));
    player_.xp = float(p.value("xp").toDouble());
    player_.facingX = float(p.value("fx").toDouble(1));
    player_.facingY = float(p.value("fy").toDouble(0));
    player_.damageDealt = float(p.value("damageDealt").toDouble());
    player_.distanceMoved = float(p.value("distance").toDouble());
    player_.dodgeCount = p.value("dodges").toInt();
    player_.talentMight = p.value("talentMight").toBool();
    player_.talentStride = p.value("talentStride").toBool();
    player_.talentLight = p.value("talentLight").toBool();
    player_.talentMastery = p.value("talentMastery").toBool();
    player_.skillCasts = p.value("skillCasts").toInt();
    player_.stacksQi = p.value("stacksQi").toInt(3);
    player_.stacksThrust = p.value("stacksThrust").toInt(3);
    player_.skillV = p.value("skillV").toInt(player_.hero == HeroClass::Mage ? kSkillBurial : -1);
    player_.speedBonus = float(p.value("speedBonus").toDouble(player_.speedBonus));
    player_.critBonus = p.value("critBonus").toInt(player_.critBonus);
    if (p.contains("baseMaxHp")) {
        baseMaxHp_ = float(p.value("baseMaxHp").toDouble());
        baseMaxMp_ = float(p.value("baseMaxMp").toDouble());
        baseArmor_ = float(p.value("baseArmor").toDouble());
    }
    auto readItem = [](const QJsonObject& obj) {
        Item item;
        item.slot = obj.value("slot").toInt();
        item.kind = obj.value("kind").toInt();
        item.power = obj.value("power").toInt();
        item.name = obj.value("name").toString();
        return item;
    };
    const QJsonArray worn = game.value("equipped").toArray();
    for (int i = 0; i < worn.size() && i < 3; ++i) {
        equipped_[i] = readItem(worn.at(i).toObject());
    }
    bag_.clear();
    for (const QJsonValue& value : game.value("bag").toArray()) {
        bag_.push_back(readItem(value.toObject()));
    }
    recomputeGear();
    player_.state = ActorState::Idle;
    damageSinceHeal_ = 0.f;
    bgmRecoverArmed_ = false;
    recoverBgmPending_ = false;
    hpTrack_ = player_.hp;
    const QJsonArray list = game.value("monsters").toArray();
    for (const QJsonValue& value : list) {
        const QJsonObject m = value.toObject();
        Monster monster;
        monster.id = nextId_++;
        const int kind = m.value("kind").toInt();
        if (kind == 1) {
            monster.kind = MonsterKind::Skeleton;
        } else if (kind == 2) {
            monster.kind = MonsterKind::Mushroom;
        } else if (kind == 3) {
            monster.kind = MonsterKind::Flyer;
        } else if (kind == 4) {
            monster.kind = MonsterKind::Caster;
        }
        setupMonster(monster, std::max(1, m.value("level").toInt(1)));
        monster.x = float(m.value("x").toDouble());
        monster.y = float(m.value("y").toDouble());
        monster.hp = float(m.value("hp").toDouble(monster.maxHp));
        monster.shield = float(m.value("shield").toDouble(monster.maxShield));
        monster.poise = float(m.value("poise").toDouble(monster.maxPoise));
        monsters_.push_back(monster);
    }
    paths_.rebuild(map_, tileOf(player_.x), tileOf(player_.y));
    pathTileX_ = tileOf(player_.x);
    pathTileY_ = tileOf(player_.y);
    return true;
}

QJsonObject Session::toJson() const {
    QJsonObject game;
    game.insert("runId", double(runId_));
    game.insert("seed", double(map_.seed()));
    game.insert("time", time_);
    game.insert("score", score_);
    game.insert("spawnCd", spawnCd_);
    game.insert("flyerCd", flyerCd_);
    QJsonObject p;
    p.insert("x", player_.x);
    p.insert("y", player_.y);
    p.insert("hp", player_.hp);
    p.insert("maxHp", player_.maxHp);
    p.insert("mp", player_.mp);
    p.insert("maxMp", player_.maxMp);
    p.insert("stamina", player_.stamina);
    p.insert("shield", player_.shield);
    p.insert("maxShield", player_.maxShield);
    p.insert("level", player_.level);
    p.insert("xp", player_.xp);
    p.insert("fx", player_.facingX);
    p.insert("fy", player_.facingY);
    p.insert("damageDealt", player_.damageDealt);
    p.insert("distance", player_.distanceMoved);
    p.insert("dodges", player_.dodgeCount);
    p.insert("talentMight", player_.talentMight);
    p.insert("talentStride", player_.talentStride);
    p.insert("talentLight", player_.talentLight);
    p.insert("talentMastery", player_.talentMastery);
    p.insert("skillCasts", player_.skillCasts);
    p.insert("stacksQi", player_.stacksQi);
    p.insert("stacksThrust", player_.stacksThrust);
    p.insert("skillV", player_.skillV);
    p.insert("hero", int(player_.hero));
    p.insert("skillD", player_.skillD);
    p.insert("skillF", player_.skillF);
    p.insert("skillC", player_.skillC);
    p.insert("baseMaxHp", baseMaxHp_);
    p.insert("baseMaxMp", baseMaxMp_);
    p.insert("baseArmor", baseArmor_);
    p.insert("speedBonus", player_.speedBonus);
    p.insert("critBonus", player_.critBonus);
    game.insert("player", p);
    auto writeItem = [](const Item& item) {
        QJsonObject obj;
        obj.insert("slot", item.slot);
        obj.insert("kind", item.kind);
        obj.insert("power", item.power);
        obj.insert("name", item.name);
        return obj;
    };
    QJsonArray worn;
    for (const Item& item : equipped_) {
        worn.append(writeItem(item));
    }
    game.insert("equipped", worn);
    QJsonArray bag;
    for (const Item& item : bag_) {
        bag.append(writeItem(item));
    }
    game.insert("bag", bag);
    QJsonArray list;
    for (const Monster& m : monsters_) {
        if (m.state == ActorState::Dead) {
            continue;
        }
        QJsonObject obj;
        int kind = 0;
        if (m.kind == MonsterKind::Skeleton) {
            kind = 1;
        } else if (m.kind == MonsterKind::Mushroom) {
            kind = 2;
        } else if (m.kind == MonsterKind::Flyer) {
            kind = 3;
        } else if (m.kind == MonsterKind::Caster) {
            kind = 4;
        }
        obj.insert("kind", kind);
        obj.insert("level", m.level);
        obj.insert("x", m.x);
        obj.insert("y", m.y);
        obj.insert("hp", m.hp);
        obj.insert("shield", m.shield);
        obj.insert("poise", m.poise);
        list.append(obj);
    }
    game.insert("monsters", list);
    return game;
}

void Session::settle() {
    if (reason_ != EndReason::None) {
        return;
    }
    reason_ = EndReason::Settle;
    paused_ = false;
}

void Session::gainXp(int amount) {
    player_.xp += float(amount);
    while (player_.xp >= float(xpToNext())) {
        player_.xp -= float(xpToNext());
        player_.level += 1;
        baseMaxHp_ += 4.f;
        baseMaxMp_ += 4.f;
        recomputeGear();
        player_.hp = std::min(player_.maxHp, player_.hp + 4.f);
        player_.mp = std::min(player_.maxMp, player_.mp + 4.f);
        queueSfx(SfxId::Level);
        note("升级");
    }
}

void Session::checkTalents() {
    if (!player_.talentMight && player_.damageDealt >= 250.f) {
        player_.talentMight = true;
        note("天赋：重手");
    }
    if (!player_.talentStride && player_.distanceMoved >= 900.f) {
        player_.talentStride = true;
        note("天赋：远行");
    }
    if (!player_.talentLight && player_.dodgeCount >= 6) {
        player_.talentLight = true;
        note("天赋：轻身");
    }
    if (!player_.talentMastery && player_.skillCasts >= 12) {
        player_.talentMastery = true;
        note("天赋：战斗熟练度");
    }
}

float Session::cdMul() const {
    return player_.talentMastery ? 0.75f : 1.f;
}

float Session::atkSpeedMul() const {
    float mul = player_.talentMastery ? 1.25f : 1.f;
    if (player_.berserkT > 0.f) {
        mul *= 2.f;  // 攻速 +100%
    }
    return mul;
}

float Session::rollDamage(float base, bool* critOut) {
    float damage = base + float(player_.weaponAtk) + float(player_.level - 1) * 1.5f;
    if (player_.talentMight) {
        damage *= 1.2f;
    }
    if (player_.berserkT > 0.f) {
        damage *= 2.f;
    }
    if (player_.hero == HeroClass::Mage) {
        damage *= 0.9f;
    } else if (player_.hero == HeroClass::Warrior) {
        damage *= 1.1f;
    }
    bool crit = int(nextRand() % 100u) < uint32_t(12 + player_.critBonus);
    if (crit) {
        damage *= 2.0f;
    }
    if (critOut) {
        *critOut = crit;
    }
    return damage;
}

float Session::scaledMonsterDamage(const Monster& monster, float base) const {
    return base * (1.f + float(std::max(1, monster.level) - 1) * 0.1f);
}

void Session::pushFloat(float x, float y, float amount, bool crit) {
    FloatText text;
    text.x = x;
    text.y = y - 18.f;
    text.life = crit ? 1.0f : 0.7f;
    text.vy = crit ? -42.f : -30.f;
    text.amount = amount;
    text.crit = crit;
    floats_.push_back(text);
}

void Session::updateFloats(float dt) {
    for (FloatText& text : floats_) {
        text.life -= dt;
        text.y += text.vy * dt;
        text.vy += 18.f * dt;
    }
    floats_.erase(std::remove_if(floats_.begin(), floats_.end(), [](const FloatText& t) { return t.life <= 0.f; }), floats_.end());
}

void Session::hurtPlayer(float damage, Monster* source) {
    if (player_.state == ActorState::Dead || player_.state == ActorState::Dodge || player_.invuln > 0.f) {
        return;
    }
    if (player_.mirrorT > 0.f) {
        if (damage >= player_.mirrorMaxHit) {
            breakMirrorShield("逆反之盾被击碎");
        } else {
            player_.mirrorAbsorbed += damage;
            if (source && source->state != ActorState::Dead) {
                const float reflect = damage * 0.05f;
                hurtMonster(*source, reflect, 2.f, false, 4.f);
                pushFloat(source->x, source->y - 6.f, reflect, false);
            }
            if (player_.mirrorAbsorbed >= player_.mirrorCap) {
                breakMirrorShield("逆反之盾能量耗尽");
            }
            player_.invuln = 0.08f;
            return;
        }
    }
    if (player_.guardT > 0.f) {
        damage *= 0.35f;
    }
    damage *= 40.f / (40.f + player_.armor);
    if (player_.shield > 0.f) {
        const float absorbed = std::min(player_.shield, damage);
        player_.shield -= absorbed;
        damage -= absorbed;
    }
    player_.hp -= damage;
    player_.invuln = 0.4f;
    player_.hurtT = 0.14f;
    queueSfx(SfxId::Hurt);
    if (player_.hp <= 0.f) {
        player_.hp = 0.f;
        player_.state = ActorState::Dead;
        player_.animT = 0.f;
        player_.flying = false;
        player_.mirrorT = 0.f;
        queueSfx(SfxId::Death);
    }
}

void Session::pushFx(AttackFxKind kind, float radius, float halfAngle, float life) {
    AttackFx fx;
    fx.kind = kind;
    fx.x = player_.x;
    fx.y = player_.y - 8.f;
    fx.fx = player_.facingX;
    fx.fy = player_.facingY;
    fx.radius = radius;
    fx.halfAngle = halfAngle;
    fx.life = life;
    fx.maxLife = life;
    attackFx_.push_back(fx);
}

void Session::updateAttackFx(float dt) {
    for (AttackFx& fx : attackFx_) {
        fx.life -= dt;
    }
    attackFx_.erase(std::remove_if(attackFx_.begin(), attackFx_.end(), [](const AttackFx& f) { return f.life <= 0.f; }), attackFx_.end());
}

void Session::breakMirrorShield(const QString& reason) {
    if (player_.mirrorT <= 0.f) {
        return;
    }
    player_.mirrorT = 0.f;
    player_.mirrorAbsorbed = 0.f;
    pushFx(AttackFxKind::Mirror, 28.f, 0.f, 0.28f);
    note(reason);
}

void Session::hurtMonster(Monster& monster, float damage, float poiseDamage, bool crit, float knockback) {
    if (monster.state == ActorState::Dead) {
        return;
    }
    if (monster.defenseT > 0.f) {
        damage *= 0.4f;
        poiseDamage *= 0.35f;
    }
    if (monster.shield > 0.f) {
        const float absorbed = std::min(monster.shield, damage);
        monster.shield -= absorbed;
        damage -= absorbed;
    }
    monster.hp -= damage;
    monster.poise -= poiseDamage;
    player_.damageDealt += damage;
    monster.hurtT = crit ? 0.22f : 0.12f;
    monster.lastHitBy = player_.attackId;
    pushFloat(monster.x, monster.y, damage, crit);
    queueSfx(crit ? SfxId::Crit : SfxId::Hit);
    if (knockback > 0.f) {
        float kx = monster.x - player_.x;
        float ky = monster.y - player_.y;
        float kd = lengthOf(kx, ky);
        if (kd < 0.01f) {
            kx = player_.facingX;
            ky = player_.facingY;
            kd = lengthOf(kx, ky);
        }
        if (kd > 0.01f) {
            kx /= kd;
            ky /= kd;
            const int pass = monster.kind == MonsterKind::Flyer ? 2 : 0;
            tryMove(monster.x, monster.y, kx * knockback, ky * knockback, 1.f, kMonsterRadius, pass, nullptr);
        }
    }
    if (monster.poise <= 0.f) {
        monster.poise = monster.maxPoise;
        monster.stunT = 0.7f;
    }
    checkTalents();
}

void Session::fireMageBolt(bool /*heavy*/) {
    Bolt bolt;
    bolt.x = player_.x + player_.facingX * 14.f;
    bolt.y = player_.y - 10.f + player_.facingY * 14.f;
    const float speed = 230.f;
    bolt.vx = player_.facingX * speed;
    bolt.vy = player_.facingY * speed;
    bolt.life = 0.55f;
    bolt.mage = true;
    bolt.trailX[0] = bolt.x;
    bolt.trailY[0] = bolt.y;
    bolt.trailLen = 1;
    bolt.damage = rollDamage(12.f, &bolt.crit);
    bolts_.push_back(bolt);
    queueSfx(SfxId::Swing);
}

void Session::fireMageLaser() {
    constexpr float kRange = 240.f;
    constexpr float kHalfWidth = 11.f;
    player_.state = ActorState::Attack;
    player_.attackT = 0.36f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Laser, kRange, 0.f, 0.28f);
    queueSfx(SfxId::Explode);
    const float fx = player_.facingX;
    const float fy = player_.facingY;
    const float fl = lengthOf(fx, fy);
    const float nx = fl > 0.01f ? fx / fl : 1.f;
    const float ny = fl > 0.01f ? fy / fl : 0.f;
    for (Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        const float dx = monster.x - player_.x;
        const float dy = monster.y - player_.y;
        const float along = dx * nx + dy * ny;
        if (along < 0.f || along > kRange) {
            continue;
        }
        const float perp = std::abs(dx * (-ny) + dy * nx);
        if (perp <= kHalfWidth) {
            bool crit = false;
            hurtMonster(monster, rollDamage(22.f, &crit), 14.f, crit, 20.f);
        }
    }
    checkTalents();
}

void Session::castHeavySwordQi() {
    constexpr float kRange = 56.f;
    player_.state = ActorState::Attack;
    player_.attackT = 0.36f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Crescent, kRange, 0.85f, 0.28f);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        const float dx = monster.x - player_.x;
        const float dy = monster.y - player_.y;
        const float dist = lengthOf(dx, dy);
        if (dist < kRange && dist > 0.01f) {
            const float dot = (dx / dist) * player_.facingX + (dy / dist) * player_.facingY;
            if (dot > 0.35f) {
                bool crit = false;
                hurtMonster(monster, rollDamage(24.f, &crit), 18.f, crit, 28.f);
            }
        }
    }
    checkTalents();
}

void Session::slashHostileBolts() {
    if (player_.state != ActorState::Attack || player_.hero == HeroClass::Mage) {
        return;
    }
    const float reach = player_.heavy ? 42.f : 34.f;
    for (Bolt& bolt : bolts_) {
        if (!bolt.hostile || bolt.life <= 0.f) {
            continue;
        }
        const float dx = bolt.x - player_.x;
        const float dy = bolt.y - (player_.y - 8.f);
        const float dist = lengthOf(dx, dy);
        if (dist < 0.01f || dist > reach) {
            continue;
        }
        const float dot = (dx / dist) * player_.facingX + (dy / dist) * player_.facingY;
        if (dot > 0.15f) {
            bolt.life = 0.f;
        }
    }
}

void Session::tryMove(float& x, float& y, float vx, float vy, float dt, float radius, int pass, float* moved) {
    const float beforeX = x;
    const float beforeY = y;
    const float nx = x + vx * dt;
    if (!map_.blockedAt(nx, y, radius, pass)) {
        x = nx;
    }
    const float ny = y + vy * dt;
    if (!map_.blockedAt(x, ny, radius, pass)) {
        y = ny;
    }
    if (moved) {
        *moved += lengthOf(x - beforeX, y - beforeY);
    }
}

Monster* Session::findMonster(int id) {
    for (Monster& monster : monsters_) {
        if (monster.id == id && monster.state != ActorState::Dead) {
            return &monster;
        }
    }
    return nullptr;
}

void Session::castSlot(int skill, float& cooldown) {
    if (skill == kSkillSwordQi) {
        castSwordQi();
        return;
    }
    if (skill == kSkillThrust) {
        castThrustStack();
        return;
    }
    if (skill == kSkillFlight) {
        toggleFlight();
        return;
    }
    if (skill == kSkillBurial) {
        castBurial(cooldown);
        return;
    }
    if (skill == kSkillMirror) {
        castMirrorShield(cooldown);
        return;
    }
    if (skill == kSkillMageHeal) {
        castMageHeal(cooldown);
        return;
    }
    if (skill == kSkillBerserk) {
        castBerserk(cooldown);
        return;
    }
    if (cooldown > 0.f) {
        return;
    }
    float cost = 16.f;
    float wait = 2.8f;
    if (skill == kSkillMageBolt) {
        cost = 14.f;
        wait = 1.6f;
    } else if (skill == kSkillNova) {
        cost = 22.f;
        wait = 4.2f;
    } else if (skill == kSkillSpin) {
        cost = 16.f;
        wait = 2.8f;
    }
    wait *= cdMul();
    if (player_.mp < cost) {
        return;
    }
    player_.mp -= cost;
    cooldown = wait;
    player_.skillCasts += 1;
    if (skill == kSkillSpin) {
        castSpin();
    } else if (skill == kSkillMageBolt) {
        castBolt();
    } else if (skill == kSkillNova) {
        castNova();
    }
    checkTalents();
}

void Session::castSpin() {
    player_.state = ActorState::Attack;
    player_.attackT = 0.35f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Ring, 42.f, 0.f, 0.32f);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) < 42.f) {
            bool crit = false;
            const float dmg = rollDamage(16.f, &crit);
            hurtMonster(monster, dmg, 12.f, crit, 14.f);
        }
    }
}

void Session::castBolt() {
    Bolt bolt;
    bolt.x = player_.x + player_.facingX * 12.f;
    bolt.y = player_.y - 8.f + player_.facingY * 12.f;
    bolt.vx = player_.facingX * 220.f;
    bolt.vy = player_.facingY * 220.f;
    bolt.life = 0.55f;
    bolt.mage = true;
    bolt.trailX[0] = bolt.x;
    bolt.trailY[0] = bolt.y;
    bolt.trailLen = 1;
    bolt.damage = rollDamage(20.f, &bolt.crit);
    bolts_.push_back(bolt);
    pushFx(AttackFxKind::Dash, 36.f, 0.3f, 0.2f);
    queueSfx(SfxId::Skill);
}

void Session::castNova() {
    player_.state = ActorState::Attack;
    player_.attackT = 0.4f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Pulse, 64.f, 0.f, 0.4f);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) < 64.f) {
            bool crit = false;
            const float dmg = rollDamage(12.f, &crit);
            hurtMonster(monster, dmg, 8.f, crit, 12.f);
        }
    }
}

void Session::castWave() {
    castSwordQi();
}

void Session::castStrike() {
    castThrustStack();
}

void Session::castSwordQi() {
    if (player_.stacksQi <= 0 || player_.mp < 10.f) {
        return;
    }
    player_.mp -= 10.f;
    player_.stacksQi -= 1;
    if (player_.cdQiStack <= 0.f) {
        player_.cdQiStack = 1.5f * cdMul();
    }
    player_.skillCasts += 1;
    player_.state = ActorState::Attack;
    player_.attackT = 0.32f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    constexpr float kQiRange = 112.f;
    pushFx(AttackFxKind::Crescent, kQiRange, 0.95f, 0.32f);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        const float dx = monster.x - player_.x;
        const float dy = monster.y - player_.y;
        const float dist = lengthOf(dx, dy);
        if (dist < kQiRange && dist > 0.01f) {
            const float dot = (dx / dist) * player_.facingX + (dy / dist) * player_.facingY;
            if (dot > 0.35f) {
                bool crit = false;
                const float dmg = rollDamage(18.f, &crit);
                hurtMonster(monster, dmg, 12.f, crit, 16.f);
            }
        }
    }
    checkTalents();
}

void Session::castThrustStack() {
    if (player_.stacksThrust <= 0 || player_.mp < 8.f) {
        return;
    }
    player_.mp -= 8.f;
    player_.stacksThrust -= 1;
    if (player_.cdThrustStack <= 0.f) {
        player_.cdThrustStack = 1.0f * cdMul();
    }
    player_.skillCasts += 1;
    player_.state = ActorState::Dodge;
    player_.dodgeT = 0.3f;  // 位移 +50%（原 0.2）
    player_.dodgeX = player_.facingX;
    player_.dodgeY = player_.facingY;
    player_.invuln = 0.22f;
    player_.attackId += 1;
    player_.animT = 0.f;
    pushFx(AttackFxKind::Dash, 72.f, 0.35f, 0.26f);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        const float dx = monster.x - player_.x;
        const float dy = monster.y - player_.y;
        const float dist = lengthOf(dx, dy);
        if (dist < 48.f && dist > 0.01f) {
            const float dot = (dx / dist) * player_.facingX + (dy / dist) * player_.facingY;
            if (dot > 0.2f) {
                bool crit = false;
                const float dmg = rollDamage(16.f, &crit);
                hurtMonster(monster, dmg, 10.f, crit, 22.f);
            }
        }
    }
    checkTalents();
}

void Session::castBurial(float& cooldown) {
    if (cooldown > 0.f || player_.mp < 40.f) {
        return;
    }
    player_.mp -= 40.f;
    cooldown = 9.f * cdMul();
    player_.skillCasts += 1;
    player_.burialT = 1.15f;
    player_.burialR = 96.f;
    player_.state = ActorState::Attack;
    player_.attackT = 0.55f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Pulse, player_.burialR, 0.f, 0.55f);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) <= player_.burialR) {
            bool crit = false;
            const float dmg = rollDamage(38.f, &crit);
            hurtMonster(monster, dmg, 22.f, crit, 10.f);
        }
    }
    note("万葬");
    checkTalents();
}

void Session::castMirrorShield(float& cooldown) {
    if (player_.hero != HeroClass::Mage || cooldown > 0.f || player_.mp < 24.f) {
        return;
    }
    player_.mp -= 24.f;
    cooldown = 15.f * cdMul();
    player_.skillCasts += 1;
    const int lv = std::max(1, player_.level);
    player_.mirrorCap = 36.f + float(lv) * 10.f;
    player_.mirrorMaxHit = 22.f + float(lv) * 6.f;
    player_.mirrorAbsorbed = 0.f;
    player_.mirrorT = 10.f;
    pushFx(AttackFxKind::Mirror, 26.f, 0.f, 0.45f);
    queueSfx(SfxId::Skill);
    note("逆反之盾");
    checkTalents();
}

void Session::castMageHeal(float& cooldown) {
    if (player_.hero != HeroClass::Mage || cooldown > 0.f || player_.mp < 22.f) {
        return;
    }
    player_.mp -= 22.f;
    cooldown = 20.f * cdMul();
    player_.skillCasts += 1;
    const float amount = player_.maxHp * 0.15f;
    player_.hp = std::min(player_.maxHp, player_.hp + amount);
    pushFx(AttackFxKind::Pulse, 28.f, 0.f, 0.35f);
    queueSfx(SfxId::Heal);
    note(QString("治疗术 +%1").arg(int(amount)));
    checkTalents();
}

void Session::castBerserk(float& cooldown) {
    if (cooldown > 0.f || player_.mp < 18.f) {
        return;
    }
    player_.mp -= 18.f;
    cooldown = 8.f * cdMul();
    player_.skillCasts += 1;
    player_.berserkT = 3.f;
    pushFx(AttackFxKind::Pulse, 36.f, 0.f, 0.4f);
    queueSfx(SfxId::Skill);
    note("狂化");
    checkTalents();
}

void Session::toggleFlight() {
    if (player_.hero != HeroClass::Mage) {
        return;
    }
    if (player_.flying) {
        player_.flying = false;
        note("落地");
        return;
    }
    if (player_.stamina < 5.f && player_.mp < 5.f) {
        return;
    }
    player_.flying = true;
    player_.skillCasts += 1;
    note("飞行");
    checkTalents();
}

void Session::updateFlight(float dt) {
    if (!player_.flying) {
        return;
    }
    float need = 22.f * dt;
    if (player_.stamina >= need) {
        player_.stamina -= need;
    } else {
        need -= player_.stamina;
        player_.stamina = 0.f;
        if (player_.mp >= need) {
            player_.mp -= need;
        } else {
            player_.mp = 0.f;
            player_.flying = false;
            note("体力耗尽，落地");
        }
    }
}

void Session::updateStackRegen(float dt) {
    if (player_.stacksQi < 3) {
        player_.cdQiStack -= dt;
        if (player_.cdQiStack <= 0.f) {
            player_.stacksQi += 1;
            player_.cdQiStack = player_.stacksQi < 3 ? 1.5f * cdMul() : 0.f;
        }
    } else {
        player_.cdQiStack = 0.f;
    }
    if (player_.stacksThrust < 3) {
        player_.cdThrustStack -= dt;
        if (player_.cdThrustStack <= 0.f) {
            player_.stacksThrust += 1;
            player_.cdThrustStack = player_.stacksThrust < 3 ? 1.0f * cdMul() : 0.f;
        }
    } else {
        player_.cdThrustStack = 0.f;
    }
}

void Session::update(float dt, const InputState& input, float mouseX, float mouseY) {
    if (paused_ || reason_ != EndReason::None) {
        return;
    }
    if (player_.state != ActorState::Dead) {
        time_ += dt;
    }
    updatePlayer(dt, input, mouseX, mouseY);
    trackHpForBgm();
    const int tx = tileOf(player_.x);
    const int ty = tileOf(player_.y);
    if (tx != pathTileX_ || ty != pathTileY_) {
        paths_.rebuild(map_, tx, ty);
        pathTileX_ = tx;
        pathTileY_ = ty;
    }
    updateMonsters(dt);
    updateBolts(dt);
    updateFloats(dt);
    updateAttackFx(dt);
    if (player_.state != ActorState::Dead) {
        spawn(dt);
    } else if (player_.animT > 0.85f) {
        reason_ = EndReason::Death;
    }
}

void Session::updatePlayer(float dt, const InputState& input, float mouseX, float mouseY) {
    player_.animT += dt;
    player_.invuln = std::max(0.f, player_.invuln - dt);
    player_.hurtT = std::max(0.f, player_.hurtT - dt);
    player_.jumpT = std::max(0.f, player_.jumpT - dt);
    player_.guardT = std::max(0.f, player_.guardT - dt);
    player_.cdGuard = std::max(0.f, player_.cdGuard - dt);
    player_.cdHeal = std::max(0.f, player_.cdHeal - dt);
    player_.cdD = std::max(0.f, player_.cdD - dt);
    player_.cdF = std::max(0.f, player_.cdF - dt);
    player_.cdC = std::max(0.f, player_.cdC - dt);
    player_.cdV = std::max(0.f, player_.cdV - dt);
    player_.burialT = std::max(0.f, player_.burialT - dt);
    player_.berserkT = std::max(0.f, player_.berserkT - dt);
    if (player_.mirrorT > 0.f) {
        player_.mirrorT -= dt;
        if (player_.mirrorT <= 0.f) {
            breakMirrorShield("逆反之盾消散");
        }
    }
    updateStackRegen(dt);
    updateFlight(dt);
    if (player_.state != ActorState::Dodge && !player_.flying) {
        player_.stamina = std::min(player_.maxStamina, player_.stamina + 32.f * dt);
    }
    player_.mp = std::min(player_.maxMp, player_.mp + 10.f * dt);
    player_.shield = std::min(player_.maxShield, player_.shield + 3.f * dt);
    if (player_.state == ActorState::Dead) {
        return;
    }

    if (player_.state != ActorState::Dodge) {
        const float aimX = mouseX - player_.x;
        const float aimY = mouseY - player_.y;
        if (lengthOf(aimX, aimY) > 6.f) {
            faceToward(player_.facingX, player_.facingY, player_.flip, aimX, aimY);
        }
    }

    if (player_.state == ActorState::Dodge) {
        player_.dodgeT -= dt;
        const int pass = (player_.jumpT > 0.f || player_.flying) ? 1 : 0;
        tryMove(player_.x, player_.y, player_.dodgeX * 260.f, player_.dodgeY * 260.f, dt, kPlayerRadius, pass, &player_.distanceMoved);
        if (player_.dodgeT <= 0.f) {
            player_.state = ActorState::Idle;
        }
        checkTalents();
        return;
    }

    const float dodgeCost = player_.talentLight ? 14.f : 24.f;
    // 闪避无冷却，可打断普攻/技能后摇
    if ((input.rmbEdge || input.shiftEdge) && player_.stamina >= dodgeCost) {
        player_.stamina -= dodgeCost;
        player_.dodgeCount += 1;
        float dodgeX = 0.f;
        float dodgeY = 0.f;
        if (input.a) {
            dodgeX -= 1.f;
        }
        if (input.d) {
            dodgeX += 1.f;
        }
        if (input.w) {
            dodgeY -= 1.f;
        }
        if (input.s) {
            dodgeY += 1.f;
        }
        dodgeX += input.moveX;
        dodgeY += input.moveY;
        if (lengthOf(dodgeX, dodgeY) > 0.01f) {
            const float dodgeLen = lengthOf(dodgeX, dodgeY);
            player_.dodgeX = dodgeX / dodgeLen;
            player_.dodgeY = dodgeY / dodgeLen;
        } else {
            player_.dodgeX = -player_.facingX;
            player_.dodgeY = -player_.facingY;
        }
        player_.attackT = 0.f;
        player_.heavy = false;
        player_.heavyCharge = 0.f;
        player_.animT = 0.f;
        if (player_.hero == HeroClass::Mage) {
            // 法师闪现：瞬间位移，不走滑步闪避
            const float fromX = player_.x;
            const float fromY = player_.y;
            constexpr float kBlinkDist = 64.f;
            const int pass = (player_.jumpT > 0.f || player_.flying) ? 1 : 0;
            const int steps = 8;
            for (int i = 0; i < steps; ++i) {
                tryMove(player_.x, player_.y, player_.dodgeX * kBlinkDist, player_.dodgeY * kBlinkDist,
                    1.f / float(steps), kPlayerRadius, pass, &player_.distanceMoved);
            }
            AttackFx start;
            start.kind = AttackFxKind::Blink;
            start.x = fromX;
            start.y = fromY - 8.f;
            start.fx = player_.dodgeX;
            start.fy = player_.dodgeY;
            start.radius = 18.f;
            start.life = 0.22f;
            start.maxLife = 0.22f;
            attackFx_.push_back(start);
            AttackFx end = start;
            end.x = player_.x;
            end.y = player_.y - 8.f;
            end.radius = 22.f;
            attackFx_.push_back(end);
            player_.invuln = 0.2f;
            player_.state = ActorState::Idle;
            note("闪现");
            queueSfx(SfxId::Dodge);
        } else {
            player_.dodgeT = 0.16f;
            player_.invuln = 0.22f;
            player_.state = ActorState::Dodge;
            queueSfx(SfxId::Dodge);
        }
        checkTalents();
        return;
    }

    if (input.spaceEdge && player_.jumpT <= 0.f && player_.stamina >= 12.f) {
        player_.stamina -= 12.f;
        player_.jumpT = 0.34f;
    }
    if (input.qEdge) {
        if (player_.cdGuard <= 0.f && player_.mp >= 12.f) {
            player_.mp -= 12.f;
            player_.cdGuard = 3.6f * cdMul();
            player_.guardT = 0.75f;
        }
    }
    if (input.eEdge && player_.cdHeal <= 0.f && player_.mp >= 20.f) {
        player_.mp -= 20.f;
        player_.cdHeal = 5.5f * cdMul();
        player_.hp = std::min(player_.maxHp, player_.hp + 22.f);
        player_.stamina = std::min(player_.maxStamina, player_.stamina + 20.f);
        note("恢复");
        queueSfx(SfxId::Heal);
    }
    if (input.rEdge) {
        castSlot(player_.skillD, player_.cdD);
    }
    if (input.fEdge) {
        castSlot(player_.skillF, player_.cdF);
    }
    if (input.cEdge) {
        castSlot(player_.skillC, player_.cdC);
        if (player_.state == ActorState::Dodge) {
            return;
        }
    }
    if (input.vEdge && player_.hero == HeroClass::Mage && player_.skillV >= 0) {
        castSlot(player_.skillV, player_.cdV);
    }

    const float as = atkSpeedMul();
    if (player_.attackT > 0.f) {
        player_.attackT -= dt;
        slashHostileBolts();
        if (player_.attackT <= 0.f) {
            player_.state = ActorState::Idle;
            player_.heavy = false;
        }
    } else if (input.lmb) {
        player_.heavyCharge += dt * as;
        if (player_.heavyCharge >= 0.42f && player_.stamina >= 30.f) {
            player_.stamina -= 30.f;
            player_.heavyCharge = 0.f;
            if (player_.hero == HeroClass::Mage) {
                fireMageLaser();
            } else {
                castHeavySwordQi();
            }
        }
    }
    if (input.lmbUp && player_.attackT <= 0.f && player_.heavyCharge > 0.f) {
        player_.state = ActorState::Attack;
        player_.attackT = 0.36f / as;
        player_.heavy = false;
        player_.animT = 0.f;
        player_.attackId += 1;
        player_.heavyCharge = 0.f;
        if (player_.hero == HeroClass::Mage) {
            fireMageBolt(false);
        } else {
            queueSfx(SfxId::Swing);
        }
    }
    if (!input.lmb) {
        player_.heavyCharge = 0.f;
    }

    float vx = 0.f;
    float vy = 0.f;
    if (input.a) {
        vx -= 1.f;
    }
    if (input.d) {
        vx += 1.f;
    }
    if (input.w) {
        vy -= 1.f;
    }
    if (input.s) {
        vy += 1.f;
    }
    vx += input.moveX;
    vy += input.moveY;
    const float moveLen = lengthOf(vx, vy);
    const bool moving = moveLen > 0.01f;
    if (moveLen > 1.f) {
        vx /= moveLen;
        vy /= moveLen;
    }
    float speed = 96.f;
    if (player_.talentStride) {
        speed += 18.f;
    }
    speed += player_.speedBonus;
    if (player_.state == ActorState::Attack) {
        speed *= 0.45f;
    }
    const int pass = (player_.jumpT > 0.f || player_.flying) ? 1 : 0;
    if (moving) {
        tryMove(player_.x, player_.y, vx * speed, vy * speed, dt, kPlayerRadius, pass, &player_.distanceMoved);
        if (player_.state != ActorState::Attack && player_.hurtT <= 0.f && player_.guardT <= 0.f) {
            player_.state = ActorState::Run;
        }
    } else if (player_.state == ActorState::Run) {
        player_.state = ActorState::Idle;
    }
    if (player_.hurtT > 0.f && player_.state != ActorState::Attack && player_.state != ActorState::Dead) {
        player_.state = ActorState::Hurt;
    }
    checkTalents();
}

void Session::spawnMonster(MonsterKind kind, float x, float y) {
    Monster monster;
    monster.id = nextId_++;
    monster.kind = kind;
    monster.x = x;
    monster.y = y;
    const int maxLevel = player_.level + 5;
    const int level = 1 + int(nextRand() % uint32_t(std::max(1, maxLevel)));
    setupMonster(monster, level);
    monsters_.push_back(monster);
}

bool Session::findSpawn(float& x, float& y) {
    for (int attempt = 0; attempt < 24; ++attempt) {
        const float angle = float(nextRand() % 628) / 100.f;
        const float dist = 170.f + float(nextRand() % 140);
        const float sx = player_.x + std::cos(angle) * dist;
        const float sy = player_.y + std::sin(angle) * dist;
        if (map_.walkable(tileOf(sx), tileOf(sy))) {
            x = (tileOf(sx) + 0.5f) * kTile;
            y = (tileOf(sy) + 0.5f) * kTile;
            return true;
        }
    }
    return false;
}

void Session::spawn(float dt) {
    flyerCd_ -= dt;
    if (flyerCd_ <= 0.f) {
        flyerCd_ = 70.f;
        bool flying = false;
        for (const Monster& monster : monsters_) {
            if (monster.kind == MonsterKind::Flyer && monster.state != ActorState::Dead) {
                flying = true;
            }
        }
        float x = 0.f;
        float y = 0.f;
        if (!flying && findSpawn(x, y)) {
            spawnMonster(MonsterKind::Flyer, x, y);
            note("飞行头目出现");
        }
    }
    spawnCd_ -= dt;
    if (spawnCd_ > 0.f) {
        return;
    }
    spawnCd_ = std::max(0.85f, 2.6f - time_ * 0.012f);
    int alive = 0;
    for (const Monster& monster : monsters_) {
        if (monster.state != ActorState::Dead) {
            alive += 1;
        }
    }
    if (alive >= 14) {
        return;
    }
    float x = 0.f;
    float y = 0.f;
    if (!findSpawn(x, y)) {
        return;
    }
    MonsterKind kind = MonsterKind::Slime;
    const uint32_t roll = nextRand() % 100u;
    if (time_ > 20.f && roll < 16u) {
        kind = MonsterKind::Caster;
    } else if (time_ > 25.f && roll < 32u) {
        kind = MonsterKind::Mushroom;
    } else if (time_ > 15.f && roll < 58u) {
        kind = MonsterKind::Skeleton;
    }
    spawnMonster(kind, x, y);
}

void Session::updateBolts(float dt) {
    for (Bolt& bolt : bolts_) {
        bolt.life -= dt;
        bolt.age += dt;
        bolt.x += bolt.vx * dt;
        bolt.y += bolt.vy * dt;
        if (bolt.mage) {
            bolt.trailAcc += dt;
            if (bolt.trailAcc >= 0.018f) {
                bolt.trailAcc = 0.f;
                if (bolt.trailLen < Bolt::kTrail) {
                    bolt.trailX[bolt.trailLen] = bolt.x;
                    bolt.trailY[bolt.trailLen] = bolt.y;
                    bolt.trailLen += 1;
                } else {
                    for (int i = 0; i < Bolt::kTrail - 1; ++i) {
                        bolt.trailX[i] = bolt.trailX[i + 1];
                        bolt.trailY[i] = bolt.trailY[i + 1];
                    }
                    bolt.trailX[Bolt::kTrail - 1] = bolt.x;
                    bolt.trailY[Bolt::kTrail - 1] = bolt.y;
                }
            }
        }
        if (map_.blocks(tileOf(bolt.x), tileOf(bolt.y), 0)) {
            bolt.life = 0.f;
            continue;
        }
        if (bolt.hostile) {
            if (lengthOf(player_.x - bolt.x, player_.y - bolt.y) < 12.f) {
                hurtPlayer(bolt.damage); // hostile bolt — no monster source
                bolt.life = 0.f;
            }
            continue;
        }
        for (Monster& monster : monsters_) {
            if (monster.state == ActorState::Dead) {
                continue;
            }
            if (lengthOf(monster.x - bolt.x, monster.y - bolt.y) < 14.f) {
                hurtMonster(monster, bolt.damage, 6.f, bolt.crit, 16.f);
                bolt.life = 0.f;
                break;
            }
        }
    }
    bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(), [](const Bolt& bolt) { return bolt.life <= 0.f; }), bolts_.end());
}

void Session::updateMonsters(float dt) {
    for (Monster& monster : monsters_) {
        monster.animT += dt;
        monster.hurtT = std::max(0.f, monster.hurtT - dt);
        monster.stunT = std::max(0.f, monster.stunT - dt);
        monster.defenseT = std::max(0.f, monster.defenseT - dt);
        monster.contactCd = std::max(0.f, monster.contactCd - dt);
        monster.lungeT = std::max(0.f, monster.lungeT - dt);
        if (monster.state == ActorState::Dead) {
            continue;
        }
        if (monster.hp <= 0.f) {
            monster.hp = 0.f;
            monster.state = ActorState::Dead;
            monster.animT = 0.f;
            if (!monster.scored) {
                const int gained = scoreFor(monster.kind, monster.level);
                score_ += gained;
                gainXp(gained);
                monster.scored = true;
            }
            continue;
        }

        const float dx = player_.x - monster.x;
        const float dy = player_.y - monster.y;
        const float dist = lengthOf(dx, dy);
        faceToward(monster.facingX, monster.facingY, monster.flip, dx, dy);
        const int pass = monster.kind == MonsterKind::Flyer ? 2 : 0;

        if (player_.hero != HeroClass::Mage && player_.state == ActorState::Attack && !player_.heavy && player_.attackT > 0.12f && player_.attackT < 0.30f && monster.lastHitBy != player_.attackId) {
            if (dist < 34.f && dist > 0.01f) {
                const float dot = (-dx / dist) * player_.facingX + (-dy / dist) * player_.facingY;
                if (dot > 0.35f) {
                    if (monster.kind == MonsterKind::Skeleton && monster.defenseT <= 0.f && (nextRand() % 100u) < 45u) {
                        monster.defenseT = 0.35f;
                    }
                    bool crit = false;
                    hurtMonster(monster, rollDamage(11.f, &crit), 4.f, crit, 18.f);
                }
            }
        }

        if (monster.stunT > 0.f) {
            monster.state = ActorState::Hurt;
            continue;
        }

        if (monster.kind == MonsterKind::Caster) {
            if (monster.attackT > 0.f) {
                monster.attackT -= dt;
                monster.state = ActorState::Attack;
                if (monster.attackT <= 0.f) {
                    monster.state = ActorState::Idle;
                }
            } else if (dist < 150.f && dist > 70.f && player_.state != ActorState::Dead) {
                monster.attackT = 0.85f;
                monster.animT = 0.f;
                monster.state = ActorState::Attack;
                Bolt bolt;
                bolt.hostile = true;
                bolt.x = monster.x;
                bolt.y = monster.y - 8.f;
                bolt.vx = monster.facingX * 150.f;
                bolt.vy = monster.facingY * 150.f;
                bolt.life = 1.1f;
                bolt.damage = scaledMonsterDamage(monster, 9.f);
                bolts_.push_back(bolt);
            }
            float gx = player_.x;
            float gy = player_.y;
            if (dist < 64.f) {
                gx = monster.x - monster.facingX * 40.f;
                gy = monster.y - monster.facingY * 40.f;
            }
            const float mx = gx - monster.x;
            const float my = gy - monster.y;
            const float md = lengthOf(mx, my);
            if (md > 4.f && (dist > 130.f || dist < 64.f)) {
                int nx = 0;
                int ny = 0;
                const int tx = tileOf(monster.x);
                const int ty = tileOf(monster.y);
                if (dist >= 64.f && paths_.nextTile(tx, ty, nx, ny)) {
                    gx = (nx + 0.5f) * kTile;
                    gy = (ny + 0.5f) * kTile;
                }
                const float sx = gx - monster.x;
                const float sy = gy - monster.y;
                const float sd = lengthOf(sx, sy);
                if (sd > 1.f) {
                    tryMove(monster.x, monster.y, sx / sd * 34.f, sy / sd * 34.f, dt, kMonsterRadius, 0, nullptr);
                    if (monster.attackT <= 0.f) {
                        monster.state = ActorState::Run;
                    }
                }
            }
            continue;
        }

        if (monster.kind == MonsterKind::Slime && dist < 16.f && monster.contactCd <= 0.f && player_.state != ActorState::Dead) {
            hurtPlayer(scaledMonsterDamage(monster, 8.f), &monster);
            monster.contactCd = 0.55f;
        }

        if (monster.attackT > 0.f) {
            monster.attackT -= dt;
            monster.state = ActorState::Attack;
            if (!monster.attackApplied && monster.attackT <= 0.22f && monster.kind != MonsterKind::Slime && monster.kind != MonsterKind::Mushroom && monster.kind != MonsterKind::Caster) {
                monster.attackApplied = true;
                if (dist < 30.f && player_.state != ActorState::Dead) {
                    hurtPlayer(scaledMonsterDamage(monster, monster.kind == MonsterKind::Flyer ? 14.f : 11.f), &monster);
                }
            }
            if (monster.attackT <= 0.f) {
                monster.state = ActorState::Idle;
            }
            continue;
        }

        if (monster.kind == MonsterKind::Mushroom && monster.lungeT <= 0.f && dist < 96.f && dist > 28.f) {
            monster.lungeT = 0.28f;
            monster.state = ActorState::Attack;
            monster.animT = 0.f;
        }
        if (monster.lungeT > 0.f) {
            tryMove(monster.x, monster.y, monster.facingX * 150.f, monster.facingY * 150.f, dt, kMonsterRadius, 0, nullptr);
            if (dist < 16.f && monster.contactCd <= 0.f) {
                hurtPlayer(scaledMonsterDamage(monster, 13.f), &monster);
                monster.contactCd = 0.7f;
            }
            monster.state = ActorState::Attack;
            continue;
        }

        if (monster.kind != MonsterKind::Slime && monster.kind != MonsterKind::Mushroom && monster.kind != MonsterKind::Caster && dist < 28.f && player_.state != ActorState::Dead) {
            monster.state = ActorState::Attack;
            monster.attackT = 0.55f;
            monster.attackApplied = false;
            monster.animT = 0.f;
            continue;
        }
        if (monster.defenseT > 0.f) {
            monster.state = ActorState::Idle;
            continue;
        }
        if (monster.hurtT > 0.f) {
            monster.state = ActorState::Hurt;
            continue;
        }

        int nx = 0;
        int ny = 0;
        const int tx = tileOf(monster.x);
        const int ty = tileOf(monster.y);
        float gx = player_.x;
        float gy = player_.y;
        if (monster.kind != MonsterKind::Flyer && paths_.nextTile(tx, ty, nx, ny)) {
            gx = (nx + 0.5f) * kTile;
            gy = (ny + 0.5f) * kTile;
        } else if (monster.kind != MonsterKind::Flyer && dist > 20.f) {
            int bestX = tx;
            int bestY = ty;
            int best = std::abs(tx - pathTileX_) + std::abs(ty - pathTileY_);
            const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : dirs) {
                const int sx = tx + d[0];
                const int sy = ty + d[1];
                if (!map_.walkable(sx, sy)) {
                    continue;
                }
                const int score = std::abs(sx - pathTileX_) + std::abs(sy - pathTileY_);
                if (score < best) {
                    best = score;
                    bestX = sx;
                    bestY = sy;
                }
            }
            if (bestX != tx || bestY != ty) {
                gx = (bestX + 0.5f) * kTile;
                gy = (bestY + 0.5f) * kTile;
            }
        }
        const float mx = gx - monster.x;
        const float my = gy - monster.y;
        const float md = lengthOf(mx, my);
        float speed = 40.f;
        if (monster.kind == MonsterKind::Skeleton) {
            speed = 36.f;
        } else if (monster.kind == MonsterKind::Mushroom) {
            speed = 32.f;
        } else if (monster.kind == MonsterKind::Flyer) {
            speed = 48.f;
        }
        const bool closeEnough = monster.kind != MonsterKind::Flyer && monster.kind != MonsterKind::Slime && monster.kind != MonsterKind::Caster && dist < 28.f;
        if (md > 2.f && !closeEnough) {
            tryMove(monster.x, monster.y, mx / md * speed, my / md * speed, dt, kMonsterRadius, pass, nullptr);
            monster.state = ActorState::Run;
        } else if (monster.state != ActorState::Attack) {
            monster.state = ActorState::Idle;
        }
    }

    for (size_t i = 0; i < monsters_.size(); ++i) {
        for (size_t j = i + 1; j < monsters_.size(); ++j) {
            if (monsters_[i].state == ActorState::Dead || monsters_[j].state == ActorState::Dead) {
                continue;
            }
            float dx = monsters_[j].x - monsters_[i].x;
            float dy = monsters_[j].y - monsters_[i].y;
            float d = lengthOf(dx, dy);
            if (d < 0.001f) {
                dx = 1.f;
                dy = 0.f;
                d = 1.f;
            }
            if (d < 14.f) {
                const float push = (14.f - d) * 0.5f;
                monsters_[i].x -= dx / d * push;
                monsters_[i].y -= dy / d * push;
                monsters_[j].x += dx / d * push;
                monsters_[j].y += dy / d * push;
            }
        }
    }

    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(), [](const Monster& monster) {
        return monster.state == ActorState::Dead && monster.animT > 0.7f;
    }), monsters_.end());
}

void Session::recomputeGear() {
    player_.weaponAtk = 0;
    player_.maxHp = baseMaxHp_;
    player_.maxMp = baseMaxMp_;
    player_.armor = baseArmor_;
    player_.hp = std::min(player_.hp, player_.maxHp);
    player_.mp = std::min(player_.mp, player_.maxMp);
}
