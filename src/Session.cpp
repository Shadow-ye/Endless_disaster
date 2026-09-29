#include "Session.h"

#include "Audio.h"

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
    case MonsterKind::Killbot:
        monster.maxHp = 34.f;
        monster.maxShield = 12.f;
        monster.maxPoise = 14.f;
        break;
    case MonsterKind::Eye:
        monster.maxHp = 560.f;
        monster.maxShield = 48.f;
        monster.maxPoise = 36.f;
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
    case MonsterKind::Killbot:
        base = 35;
        break;
    case MonsterKind::Eye:
        base = 180;
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

void Session::queueVfx(VfxKind kind, float x, float y, float radius, bool crit, MonsterKind monster) {
    if (vfxQueue_.size() >= 96) {
        return;
    }
    VfxEvent event;
    event.kind = kind;
    event.x = x;
    event.y = y;
    event.radius = radius;
    event.crit = crit;
    event.monster = monster;
    vfxQueue_.push_back(event);
}

std::vector<VfxEvent> Session::drainVfx() {
    std::vector<VfxEvent> out;
    out.swap(vfxQueue_);
    return out;
}

bool Session::consumeRecoverBgm() {
    if (!recoverBgmPending_) {
        return false;
    }
    recoverBgmPending_ = false;
    return true;
}

bool Session::consumeVoidPrompt() {
    if (!voidPrompt_) {
        return false;
    }
    voidPrompt_ = false;
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

void Session::newGame(uint32_t seed, uint32_t runId, HeroClass hero, int skillD, int skillF, int skillC, int skillV,
    bool guideAtStart) {
    map_ = TileMap(seed);
    player_ = Player{};
    player_.x = 8.f;
    player_.y = 8.f;
    player_.hero = hero;
    if (guideAtStart) {
        player_.talentGuide = true;
    }
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
    } else if (hero == HeroClass::Robot) {
        player_.skillD = kSkillScatter;
        player_.skillF = kSkillMissile;
        player_.skillC = kSkillBoost;
        player_.skillV = -1;
        if (isRobotSkill(skillD)) {
            player_.skillD = skillD;
        }
        if (isRobotSkill(skillF)) {
            player_.skillF = skillF;
        }
        if (isRobotSkill(skillC)) {
            player_.skillC = skillC;
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
    player_.cursed = false;
    eliteCd_ = 0.f;
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
    } else if (hero == HeroClass::Robot) {
        baseMaxHp_ = 105.f;
        baseArmor_ = 12.f;
        baseMaxMp_ = 90.f;
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
    drones_.clear();
    drops_.clear();
    floats_.clear();
    attackFx_.clear();
    vfxQueue_.clear();
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
    ruin_.clear();
    shakeT_ = 0.f;
    plazaRed_ = 0.f;
    voidPrompt_ = false;
    eyeDefeats_ = 0;
    wallStrikeId_ = -1;
    reviveState_ = ReviveState::None;
    revivePrompt_ = false;
    talismanBonus_ = 0;
    if (guideAtStart) {
        note("天赋：世界指引（开局赠送）");
    }
    spawnRuin();
}

bool Session::loadFrom(const QJsonObject& game) {
    const uint32_t seed = uint32_t(game.value("seed").toDouble());
    const uint32_t runId = uint32_t(game.value("runId").toDouble());
    const QJsonObject savedPlayer = game.value("player").toObject();
    const HeroClass hero = HeroClass(std::clamp(savedPlayer.value("hero").toInt(), 0, int(HeroClass::Robot)));
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
    player_.ammo = std::clamp(p.value("ammo").toInt(kRobotMagazine), 0, kRobotMagazine);
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
    player_.talentGuide = p.value("talentGuide").toBool();
    player_.talentUnderdog = p.value("talentUnderdog").toBool();
    player_.worldKills = std::max(0, p.value("worldKills").toInt());
    player_.underdogKills = std::max(0, p.value("underdogKills").toInt());
    player_.skillCasts = p.value("skillCasts").toInt();
    player_.stacksQi = p.value("stacksQi").toInt(3);
    player_.stacksThrust = p.value("stacksThrust").toInt(3);
    const int savedV = p.value("skillV").toInt(player_.hero == HeroClass::Mage ? kSkillBurial : -1);
    if (player_.hero == HeroClass::Mage && isMageSkill(savedV)) {
        player_.skillV = savedV;
    } else if (player_.hero == HeroClass::Mage) {
        player_.skillV = kSkillBurial;
    } else {
        player_.skillV = -1;
    }
    player_.speedBonus = float(p.value("speedBonus").toDouble(player_.speedBonus));
    player_.critBonus = p.value("critBonus").toInt(player_.critBonus);
    player_.seekOn = p.value("seekOn").toBool(false);
    player_.cursed = p.value("cursed").toBool(false);
    eliteCd_ = float(p.value("eliteCd").toDouble(0.0));
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
        item.count = std::max(1, obj.value("count").toInt(1));
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
        } else if (kind == 5) {
            monster.kind = MonsterKind::Killbot;
        } else if (kind == 6) {
            continue;
        }
        setupMonster(monster, std::max(1, m.value("level").toInt(1)));
        monster.elite = m.value("elite").toBool(false);
        if (monster.elite) {
            monster.maxHp *= 2.f;
            monster.maxPoise *= 1.5f;
        }
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
    restoreRuin(game);
    eyeDefeats_ = std::max(0, game.value("eyeDefeats").toInt());
    reviveState_ = ReviveState::None;
    revivePrompt_ = false;
    talismanBonus_ = 0;
    checkTalents();
    return true;
}

QJsonObject Session::toJson() const {
    QJsonObject game;
    game.insert("runId", double(runId_));
    game.insert("seed", double(map_.seed()));
    game.insert("time", time_);
    game.insert("score", score_);
    game.insert("eyeDefeats", eyeDefeats_);
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
    p.insert("ammo", player_.ammo);
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
    p.insert("talentGuide", player_.talentGuide);
    p.insert("talentUnderdog", player_.talentUnderdog);
    p.insert("worldKills", player_.worldKills);
    p.insert("underdogKills", player_.underdogKills);
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
    p.insert("seekOn", player_.seekOn);
    if (player_.cursed) {
        p.insert("cursed", true);
    }
    p.insert("eliteCd", eliteCd_);
    game.insert("player", p);
    auto writeItem = [](const Item& item) {
        QJsonObject obj;
        obj.insert("slot", item.slot);
        obj.insert("kind", item.kind);
        obj.insert("power", item.power);
        obj.insert("count", item.count);
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
        } else if (m.kind == MonsterKind::Killbot) {
            kind = 5;
        } else if (m.kind == MonsterKind::Eye) {
            continue;
        }
        obj.insert("kind", kind);
        obj.insert("level", m.level);
        if (m.elite) {
            obj.insert("elite", true);
        }
        obj.insert("x", m.x);
        obj.insert("y", m.y);
        obj.insert("hp", m.hp);
        obj.insert("shield", m.shield);
        obj.insert("poise", m.poise);
        list.append(obj);
    }
    game.insert("monsters", list);
    if (ruin_.phase != MazeRuin::Phase::None) {
        QJsonObject ruin;
        ruin.insert("phase", int(ruin_.phase));
        ruin.insert("ox", ruin_.originX);
        ruin.insert("oy", ruin_.originY);
        ruin.insert("seed", double(ruin_.seed));
        ruin.insert("bossDead", ruin_.bossDead);
        ruin.insert("entered", ruin_.enteredPlaza);
        ruin.insert("cooldown", ruin_.cooldown);
        QString wallBits;
        wallBits.reserve(int(ruin_.wall.size()));
        for (uint8_t cell : ruin_.wall) {
            wallBits += cell ? QLatin1Char('1') : QLatin1Char('0');
        }
        ruin.insert("walls", wallBits);
        for (const Monster& m : monsters_) {
            if (m.kind == MonsterKind::Eye && m.state != ActorState::Dead) {
                ruin.insert("eyeHp", m.hp);
                ruin.insert("eyeShield", m.shield);
                ruin.insert("eyeLevel", m.level);
                break;
            }
        }
        game.insert("ruin", ruin);
    }
    return game;
}

void Session::settle() {
    finishRun(EndReason::Settle);
}

// 本局结束的唯一出口：符咒折算成积分后再定原因，界面只认 reason_
void Session::finishRun(EndReason reason) {
    if (reason_ != EndReason::None) {
        return;
    }
    convertTalismansToScore();
    reason_ = reason;
    paused_ = false;
}

void Session::convertTalismansToScore() {
    const int owned = talismanCount();
    if (owned <= 0) {
        return;
    }
    bag_.erase(std::remove_if(bag_.begin(), bag_.end(), [](const Item& item) { return item.kind == kItemReturnTalisman; }), bag_.end());
    const int bonus = owned * kTalismanScore;
    score_ += bonus;
    talismanBonus_ += bonus;
    note(QString("%1 x%2 折算 %3 积分").arg(itemText(kItemReturnTalisman).name).arg(owned).arg(bonus));
}

void Session::addItem(int kind, int count) {
    if (kind == kItemNone || count <= 0) {
        return;
    }
    for (Item& item : bag_) {
        if (item.kind == kind) {
            item.count += count;
            item.name = itemText(kind).name;
            return;
        }
    }
    Item item;
    item.kind = kind;
    item.count = count;
    item.name = itemText(kind).name;
    bag_.push_back(item);
}

bool Session::consumeItem(int kind, int count) {
    for (auto it = bag_.begin(); it != bag_.end(); ++it) {
        if (it->kind != kind) {
            continue;
        }
        if (it->count < count) {
            return false;
        }
        it->count -= count;
        if (it->count <= 0) {
            bag_.erase(it);
        }
        return true;
    }
    return false;
}

int Session::itemCount(int kind) const {
    int total = 0;
    for (const Item& item : bag_) {
        if (item.kind == kind) {
            total += item.count;
        }
    }
    return total;
}

bool Session::consumeRevivePrompt() {
    if (!revivePrompt_) {
        return false;
    }
    revivePrompt_ = false;
    return true;
}

bool Session::acceptRevive() {
    if (reviveState_ != ReviveState::Offered) {
        return false;
    }
    if (!consumeItem(kItemReturnTalisman, 1)) {
        return false;
    }
    reviveState_ = ReviveState::None;
    revivePrompt_ = false;
    paused_ = false;
    revivePlayer();
    return true;
}

void Session::declineRevive() {
    if (reviveState_ != ReviveState::Offered) {
        return;
    }
    reviveState_ = ReviveState::Declined;
    revivePrompt_ = false;
    paused_ = false;
}

void Session::revivePlayer() {
    // 意识回归只把意识拉回来：生命恢复到 25%，盾与体力回满，蓝补到一半
    player_.hp = std::max(1.f, player_.maxHp * 0.25f);
    player_.shield = player_.maxShield;
    player_.stamina = player_.maxStamina;
    player_.mp = std::max(player_.mp, player_.maxMp * 0.5f);
    player_.ammo = kRobotMagazine;
    player_.state = ActorState::Idle;
    player_.animT = 0.f;
    player_.hurtT = 0.f;
    player_.attackT = 0.f;
    player_.dodgeT = 0.f;
    player_.jumpT = 0.f;
    player_.flying = false;
    player_.invuln = 3.f;
    damageSinceHeal_ = 0.f;
    hpTrack_ = player_.hp;
    bgmRecoverArmed_ = false;
    // 清掉身边的敌方弹幕并把怪物推开，避免复活瞬间又被秒
    bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(),
                     [this](const Bolt& bolt) {
                         return bolt.hostile && lengthOf(bolt.x - player_.x, bolt.y - player_.y) < 260.f;
                     }),
        bolts_.end());
    for (Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead || monster.kind == MonsterKind::Eye) {
            continue;
        }
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) > 220.f) {
            continue;
        }
        monster.stunT = std::max(monster.stunT, 1.2f);
        monster.attackT = 0.f;
        monster.attackApplied = false;
        monster.contactCd = 0.8f;
        float kx = monster.x - player_.x;
        float ky = monster.y - player_.y;
        float kd = lengthOf(kx, ky);
        if (kd < 0.01f) {
            kx = player_.facingX;
            ky = player_.facingY;
            kd = lengthOf(kx, ky);
        }
        if (kd > 0.01f) {
            const int pass = monster.kind == MonsterKind::Flyer ? 2 : 0;
            tryMove(monster.x, monster.y, kx / kd * 96.f, ky / kd * 96.f, 1.f, kMonsterRadius, pass, nullptr);
        }
    }
    pushFx(AttackFxKind::Ring, 120.f, 0.f, 0.75f, 0xFFD24A);
    pushFx(AttackFxKind::Pillar, 70.f, 0.f, 0.9f, 0xFFD24A);
    queueVfx(VfxKind::LevelUp, player_.x, player_.y);
    queueSfx(SfxId::Level);
    // 复活留下的代价：本轮带上诅咒，之后偶尔刷出双倍血量的精英怪
    if (!player_.cursed) {
        player_.cursed = true;
        eliteCd_ = 22.f + float(nextRand() % 22u);
    }
    note("意识回归　诅咒：存在被克苏鲁余光注意！");
}

void Session::gainXp(int amount) {
    player_.xp += float(amount);
    if (player_.xp >= float(xpToNext())) {
        pushFx(AttackFxKind::Pillar, 90.f, 0.f, 0.9f, 0xFFD24A);
        pushFx(AttackFxKind::Ring, 40.f, 0.f, 0.6f, 0xFFD24A);
        queueVfx(VfxKind::LevelUp, player_.x, player_.y);
    }
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
    if (!player_.talentGuide && player_.worldKills >= 20) {
        player_.talentGuide = true;
        note("天赋：世界指引");
    }
    if (!player_.talentUnderdog && player_.underdogKills >= 10) {
        player_.talentUnderdog = true;
        note("天赋：以小博大");
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
    if (player_.overloadT > 0.f) {
        mul *= 1.6f;
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
    if (player_.overloadT > 0.f) {
        damage *= 1.4f;
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
    if (player_.fieldT > 0.f) {
        damage *= 0.05f;
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

void Session::pushFx(AttackFxKind kind, float radius, float halfAngle, float life, uint32_t color) {
    AttackFx fx;
    fx.kind = kind;
    fx.color = color;
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

void Session::pushSlashFx() {
    pushFx(AttackFxKind::Slash, 34.f, 1.0f, player_.attackT * 0.6f, player_.berserkT > 0.f ? 0xFF6A50 : 0);
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
    if (monster.kind == MonsterKind::Eye && ruin_.arrive >= 0.f) {
        return;
    }
    if (player_.talentUnderdog && monster.level > player_.level) {
        damage *= 1.3f;
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
    queueVfx(VfxKind::Hit, monster.x, monster.y, 0.f, crit, monster.kind);
    if (knockback > 0.f && monster.kind != MonsterKind::Eye) {
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
        monster.stunT = monster.kind == MonsterKind::Eye ? 1.6f : 0.7f;
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
    breakMazeWallsBeam(kRange, kHalfWidth);
    checkTalents();
}

void Session::castHeavySwordQi() {
    constexpr float kRange = 56.f;
    player_.state = ActorState::Attack;
    player_.attackT = 0.36f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Crescent, kRange, kMeleeConeHalf, 0.28f);
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
            if (dot > kMeleeConeDot) {
                bool crit = false;
                hurtMonster(monster, rollDamage(24.f, &crit), 18.f, crit, 28.f);
            }
        }
    }
    breakMazeWallsCone(kRange, kMeleeConeDot);
    checkTalents();
}

void Session::slashHostileBolts() {
    // 机甲人带了「肘击」时也能劈掉敌方飞弹
    if (player_.state != ActorState::Attack || (rangedHero() && !robotMelee())) {
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

bool Session::canBreakMazeWalls() const {
    if (!ruin_.active || ruin_.wall.size() != size_t(MazeRuin::kSize * MazeRuin::kSize)) {
        return false;
    }
    // 机甲人是远程职业：子弹、爆破弹、肘击、无人机爆炸都拆不动迷宫墙，只有打败克苏鲁之眼之后才行
    if (player_.hero == HeroClass::Robot) {
        return ruin_.bossDead;
    }
    if (player_.hero == HeroClass::Warrior || player_.hero == HeroClass::Sword) {
        return true;
    }
    return ruin_.bossDead;
}

bool Session::breakMazeWallTile(int tileX, int tileY) {
    if (!canBreakMazeWalls() || !ruin_.isWallAt(tileX, tileY)) {
        return false;
    }
    const int lx = tileX - ruin_.originX;
    const int ly = tileY - ruin_.originY;
    ruin_.wall[size_t(ly * MazeRuin::kSize + lx)] = 0;
    return true;
}

void Session::commitBrokenWalls(bool broken) {
    if (!broken) {
        return;
    }
    syncRuinMap();
    paths_.rebuild(map_, tileOf(player_.x), tileOf(player_.y));
    pathTileX_ = tileOf(player_.x);
    pathTileY_ = tileOf(player_.y);
    queueSfx(SfxId::Hit);
}

void Session::breakMazeWallsCone(float range, float minDot) {
    if (!canBreakMazeWalls()) {
        return;
    }
    const float fl = lengthOf(player_.facingX, player_.facingY);
    if (fl < 0.01f) {
        return;
    }
    const float nx = player_.facingX / fl;
    const float ny = player_.facingY / fl;
    const int span = int(std::ceil(range / float(kTile))) + 1;
    const int cx = tileOf(player_.x);
    const int cy = tileOf(player_.y);
    bool broken = false;
    for (int ty = cy - span; ty <= cy + span; ++ty) {
        for (int tx = cx - span; tx <= cx + span; ++tx) {
            const float dx = (float(tx) + 0.5f) * float(kTile) - player_.x;
            const float dy = (float(ty) + 0.5f) * float(kTile) - player_.y;
            const float dist = lengthOf(dx, dy);
            if (dist > range || dist < 0.01f) {
                continue;
            }
            if ((dx / dist) * nx + (dy / dist) * ny < minDot) {
                continue;
            }
            broken = breakMazeWallTile(tx, ty) || broken;
        }
    }
    commitBrokenWalls(broken);
}

void Session::breakMazeWallsRadius(float x, float y, float radius) {
    if (!canBreakMazeWalls() || radius <= 0.f) {
        return;
    }
    const int span = int(std::ceil(radius / float(kTile))) + 1;
    const int cx = tileOf(x);
    const int cy = tileOf(y);
    bool broken = false;
    for (int ty = cy - span; ty <= cy + span; ++ty) {
        for (int tx = cx - span; tx <= cx + span; ++tx) {
            const float dx = (float(tx) + 0.5f) * float(kTile) - x;
            const float dy = (float(ty) + 0.5f) * float(kTile) - y;
            if (lengthOf(dx, dy) > radius) {
                continue;
            }
            broken = breakMazeWallTile(tx, ty) || broken;
        }
    }
    commitBrokenWalls(broken);
}

void Session::breakMazeWallsBeam(float range, float halfWidth) {
    if (!canBreakMazeWalls()) {
        return;
    }
    const float fl = lengthOf(player_.facingX, player_.facingY);
    if (fl < 0.01f) {
        return;
    }
    const float nx = player_.facingX / fl;
    const float ny = player_.facingY / fl;
    const int span = int(std::ceil(range / float(kTile))) + 1;
    const int cx = tileOf(player_.x);
    const int cy = tileOf(player_.y);
    bool broken = false;
    for (int ty = cy - span; ty <= cy + span; ++ty) {
        for (int tx = cx - span; tx <= cx + span; ++tx) {
            const float dx = (float(tx) + 0.5f) * float(kTile) - player_.x;
            const float dy = (float(ty) + 0.5f) * float(kTile) - player_.y;
            const float along = dx * nx + dy * ny;
            if (along < 0.f || along > range) {
                continue;
            }
            const float perp = std::abs(dx * (-ny) + dy * nx);
            if (perp > halfWidth) {
                continue;
            }
            broken = breakMazeWallTile(tx, ty) || broken;
        }
    }
    commitBrokenWalls(broken);
}

void Session::applyEyeLevel() {
    const int bossLevel = std::max(1, player_.level) + 99;
    for (Monster& monster : monsters_) {
        if (monster.kind != MonsterKind::Eye || monster.state == ActorState::Dead) {
            continue;
        }
        setupMonster(monster, std::max(1, bossLevel - 99));
        monster.level = bossLevel;
    }
}

int Session::playerPass() const {
    if (player_.jumpT > 0.f || player_.flying) {
        return 1;
    }
    // 飞行或跳跃结束时落在岩石/灌木上，放行到走出来为止，否则会卡死
    return map_.blockedAt(player_.x, player_.y, kPlayerRadius, 0) ? 1 : 0;
}

bool Session::nearestWalkableTile(int tileX, int tileY, int& outX, int& outY) const {
    int best = std::abs(tileX - pathTileX_) + std::abs(tileY - pathTileY_);
    bool found = false;
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : dirs) {
        const int sx = tileX + d[0];
        const int sy = tileY + d[1];
        if (!map_.walkable(sx, sy)) {
            continue;
        }
        const int score = std::abs(sx - pathTileX_) + std::abs(sy - pathTileY_);
        if (score < best) {
            best = score;
            outX = sx;
            outY = sy;
            found = true;
        }
    }
    return found;
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
    if (skill == kSkillSeek) {
        player_.seekOn = !player_.seekOn;
        note(player_.seekOn ? "寻路开启" : "寻路关闭");
        return;
    }
    if (skill == kSkillMelee) {
        castMelee(cooldown);
        return;
    }
    if (skill == kSkillSwordQi) {
        castSwordQi();
        return;
    }
    if (skill == kSkillThrust) {
        castThrustStack();
        return;
    }
    if (skill == kSkillFlight || skill == kSkillJetpack) {
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
        castBerserk(cooldown, "狂化");
        return;
    }
    if (skill == kSkillOverload) {
        castOverload(cooldown);
        return;
    }
    if (skill == kSkillMagField) {
        castMagField(cooldown);
        return;
    }
    if (skill == kSkillMedkit) {
        castMedkit(cooldown);
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
    } else if (skill == kSkillScatter) {
        cost = 14.f;
        wait = 2.4f;
    } else if (skill == kSkillMissile) {
        cost = 20.f;
        wait = 4.5f;
    } else if (skill == kSkillBoost) {
        cost = 12.f;
        wait = 3.f;
    } else if (skill == kSkillSwarm) {
        cost = 22.f;
        wait = 7.f;
    }
    wait *= cdMul();
    cost *= skillCostMul();
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
    } else if (skill == kSkillScatter) {
        castScatter();
    } else if (skill == kSkillMissile) {
        castMissile();
    } else if (skill == kSkillBoost) {
        castBoost();
    } else if (skill == kSkillSwarm) {
        castSwarm();
    }
    checkTalents();
}

namespace {
// 枪口离地高度，与 GameWidget 里机甲人贴图下沉量一起调
constexpr float kRobotMuzzleLift = 18.8f;
constexpr float kRobotShotGap = 0.15f;
constexpr float kRobotReloadHold = 0.42f;

constexpr int kDroneCount = 6;
constexpr float kDroneLife = 12.f;
constexpr float kDroneBlast = 26.f;
constexpr float kDroneSeekRange = 240.f;
constexpr float kDroneOrbitRadius = 26.f;
constexpr float kDroneIdleSpeed = 150.f;
constexpr float kDroneAttackSpeed = 230.f;
constexpr float kDroneMaxAccel = 900.f;
constexpr float kDroneNeighbor = 36.f;
constexpr float kDroneSeparation = 12.f;
}

void Session::fireRobotShot(float angleOffset, float baseDamage) {
    const float a = std::atan2(player_.facingY, player_.facingX) + angleOffset;
    const float dx = std::cos(a);
    const float dy = std::sin(a);
    Bolt bolt;
    bolt.x = player_.x + dx * 14.f;
    bolt.y = player_.y + dy * 14.f;
    bolt.lift = kRobotMuzzleLift;
    bolt.vx = dx * 320.f;
    bolt.vy = dy * 320.f;
    bolt.life = 0.5f;
    bolt.robot = true;
    bolt.damage = rollDamage(baseDamage, &bolt.crit);
    bolts_.push_back(bolt);
}

void Session::castScatter() {
    player_.state = ActorState::Attack;
    player_.attackT = 0.3f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    for (int i = -2; i <= 2; ++i) {
        fireRobotShot(float(i) * 0.18f, 9.f);
    }
    pushFx(AttackFxKind::Cone, 30.f, 0.4f, 0.16f);
    queueSfx(SfxId::Skill);
}

void Session::castMissile() {
    player_.state = ActorState::Attack;
    player_.attackT = 0.34f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    Bolt bolt;
    bolt.x = player_.x + player_.facingX * 14.f;
    bolt.y = player_.y + player_.facingY * 14.f;
    bolt.lift = kRobotMuzzleLift;
    bolt.vx = player_.facingX * 190.f;
    bolt.vy = player_.facingY * 190.f;
    bolt.life = 0.8f;
    bolt.robot = true;
    bolt.blast = 44.f;
    bolt.damage = rollDamage(26.f, &bolt.crit);
    bolts_.push_back(bolt);
    queueSfx(SfxId::Skill);
}

void Session::explodeBolt(const Bolt& bolt) {
    AttackFx fx;
    fx.kind = AttackFxKind::Burst;
    fx.x = bolt.x;
    fx.y = bolt.y - 8.f;
    fx.radius = bolt.blast;
    fx.life = 0.36f;
    fx.maxLife = 0.36f;
    attackFx_.push_back(fx);
    queueSfx(SfxId::Explode);
    queueVfx(VfxKind::Explode, bolt.x, bolt.y, bolt.blast);
    breakMazeWallsRadius(bolt.x, bolt.y, bolt.blast);
    for (Monster& monster : monsters_) {
        if (monster.state != ActorState::Dead && lengthOf(monster.x - bolt.x, monster.y - bolt.y) <= bolt.blast) {
            hurtMonster(monster, bolt.damage, 16.f, bolt.crit, 18.f);
        }
    }
}

void Session::castSwarm() {
    bool taken[kDroneCount]{};
    for (Drone& drone : drones_) {
        drone.life = kDroneLife;
        taken[std::clamp(drone.slot, 0, kDroneCount - 1)] = true;
    }
    for (int slot = 0; slot < kDroneCount; ++slot) {
        if (taken[slot]) {
            continue;
        }
        const float a = 6.2831853f * float(slot) / float(kDroneCount);
        Drone drone;
        drone.x = player_.x + std::cos(a) * 10.f;
        drone.y = player_.y + std::sin(a) * 10.f;
        drone.vx = std::cos(a) * 80.f;
        drone.vy = std::sin(a) * 80.f;
        drone.life = kDroneLife;
        drone.slot = slot;
        drone.damage = rollDamage(11.f, &drone.crit);
        drones_.push_back(drone);
    }
    pushFx(AttackFxKind::Ring, 22.f, 0.7f, 0.25f, 0x60E0D0);
    queueSfx(SfxId::Skill);
}

// 鸟群算法：分离 + 对齐 + 聚拢，再叠加目标牵引（有怪追最近的怪，没怪绕玩家盘旋）
void Session::updateDrones(float dt) {
    if (drones_.empty()) {
        return;
    }
    const Monster* target = nullptr;
    float best = kDroneSeekRange;
    for (const Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        const float dist = lengthOf(monster.x - player_.x, monster.y - player_.y);
        if (dist < best) {
            best = dist;
            target = &monster;
        }
    }

    const size_t count = drones_.size();
    std::vector<float> accX(count, 0.f);
    std::vector<float> accY(count, 0.f);
    std::vector<char> attacking(count, 0);
    for (size_t i = 0; i < count; ++i) {
        const Drone& drone = drones_[i];
        float sepX = 0.f, sepY = 0.f, aliX = 0.f, aliY = 0.f, cohX = 0.f, cohY = 0.f;
        int neighbors = 0;
        for (size_t j = 0; j < count; ++j) {
            if (i == j) {
                continue;
            }
            const Drone& other = drones_[j];
            const float dx = drone.x - other.x;
            const float dy = drone.y - other.y;
            const float dist = lengthOf(dx, dy);
            if (dist >= kDroneNeighbor) {
                continue;
            }
            neighbors += 1;
            aliX += other.vx;
            aliY += other.vy;
            cohX += other.x;
            cohY += other.y;
            if (dist < kDroneSeparation) {
                const float push = (kDroneSeparation - dist) / kDroneSeparation;
                if (dist > 0.01f) {
                    sepX += dx / dist * push;
                    sepY += dy / dist * push;
                } else {
                    const float a = 6.2831853f * float(drone.slot) / float(kDroneCount);
                    sepX += std::cos(a) * push;
                    sepY += std::sin(a) * push;
                }
            }
        }
        float ax = sepX * 700.f;
        float ay = sepY * 700.f;
        if (neighbors > 0) {
            ax += (aliX / neighbors - drone.vx) * 1.5f + (cohX / neighbors - drone.x) * 3.f;
            ay += (aliY / neighbors - drone.vy) * 1.5f + (cohY / neighbors - drone.y) * 3.f;
        }
        // 按编号错开出击，看起来是一架接一架扑出去
        const bool attack = target && drone.age > 0.25f + 0.12f * float(drone.slot);
        attacking[i] = attack ? 1 : 0;
        float gx = 0.f;
        float gy = 0.f;
        float speed = kDroneAttackSpeed;
        if (attack) {
            gx = target->x - drone.x;
            gy = target->y - drone.y;
        } else {
            const float a = time_ * 1.6f + 6.2831853f * float(drone.slot) / float(kDroneCount);
            gx = player_.x + std::cos(a) * kDroneOrbitRadius - drone.x;
            gy = player_.y + std::sin(a) * kDroneOrbitRadius - drone.y;
            speed = std::min(kDroneIdleSpeed, lengthOf(gx, gy) * 5.f);
        }
        const float goalLen = lengthOf(gx, gy);
        if (goalLen > 0.01f) {
            ax += (gx / goalLen * speed - drone.vx) * 5.f;
            ay += (gy / goalLen * speed - drone.vy) * 5.f;
        }
        const float accLen = lengthOf(ax, ay);
        if (accLen > kDroneMaxAccel) {
            ax *= kDroneMaxAccel / accLen;
            ay *= kDroneMaxAccel / accLen;
        }
        accX[i] = ax;
        accY[i] = ay;
    }

    for (size_t i = 0; i < count; ++i) {
        Drone& drone = drones_[i];
        drone.vx += accX[i] * dt;
        drone.vy += accY[i] * dt;
        const float maxSpeed = attacking[i] ? kDroneAttackSpeed : kDroneIdleSpeed;
        const float speed = lengthOf(drone.vx, drone.vy);
        if (speed > maxSpeed) {
            drone.vx *= maxSpeed / speed;
            drone.vy *= maxSpeed / speed;
        }
        drone.x += drone.vx * dt;
        drone.y += drone.vy * dt;
        drone.age += dt;
        drone.life -= dt;
        // 撞上迷宫墙壁就地爆炸：碎片仍在半径 26 内生效，但拆不掉墙（机甲人要等 boss 倒下才能拆）
        if (map_.at(tileOf(drone.x), tileOf(drone.y)) == Tile::MazeWall) {
            drone.life = 0.f;
            explodeDrone(drone, true);
            continue;
        }
        if (drone.life <= 0.f) {
            explodeDrone(drone, false);
            continue;
        }
        for (Bolt& bolt : bolts_) {
            if (bolt.hostile && bolt.life > 0.f && lengthOf(bolt.x - drone.x, bolt.y - drone.y) < 9.f) {
                bolt.life = 0.f;
                drone.life = 0.f;
                explodeDrone(drone, false);
                break;
            }
        }
        if (drone.life <= 0.f) {
            continue;
        }
        for (const Monster& monster : monsters_) {
            if (monster.state != ActorState::Dead && lengthOf(monster.x - drone.x, monster.y - drone.y) < 12.f) {
                drone.life = 0.f;
                break;
            }
        }
        if (drone.life <= 0.f) {
            explodeDrone(drone, true);
        }
    }
    drones_.erase(std::remove_if(drones_.begin(), drones_.end(), [](const Drone& drone) { return drone.life <= 0.f; }), drones_.end());
}

void Session::explodeDrone(const Drone& drone, bool harmful) {
    AttackFx fx;
    fx.kind = harmful ? AttackFxKind::Burst : AttackFxKind::Pulse;
    fx.x = drone.x;
    fx.y = drone.y - 12.f;
    fx.radius = harmful ? kDroneBlast : 10.f;
    fx.life = harmful ? 0.32f : 0.2f;
    fx.maxLife = fx.life;
    fx.color = harmful ? 0 : 0x6EE6FF;
    attackFx_.push_back(fx);
    if (!harmful) {
        queueSfx(SfxId::Hit);
        return;
    }
    queueSfx(SfxId::Explode);
    queueVfx(VfxKind::Explode, drone.x, drone.y, kDroneBlast);
    breakMazeWallsRadius(drone.x, drone.y, kDroneBlast);
    for (Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        float kx = monster.x - drone.x;
        float ky = monster.y - drone.y;
        float kd = lengthOf(kx, ky);
        if (kd > kDroneBlast) {
            continue;
        }
        hurtMonster(monster, drone.damage, 10.f, drone.crit);
        // 击退方向以爆点为圆心，而不是 hurtMonster 默认的以玩家为圆心
        if (kd < 0.01f) {
            kx = drone.vx;
            ky = drone.vy;
            kd = lengthOf(kx, ky);
        }
        if (kd > 0.01f) {
            const int pass = monster.kind == MonsterKind::Flyer ? 2 : 0;
            tryMove(monster.x, monster.y, kx / kd * 22.f, ky / kd * 22.f, 1.f, kMonsterRadius, pass, nullptr);
        }
    }
}

void Session::castBoost() {
    player_.state = ActorState::Dodge;
    player_.dodgeT = 0.22f;
    player_.dodgeX = player_.facingX;
    player_.dodgeY = player_.facingY;
    player_.invuln = 0.26f;
    player_.attackId += 1;
    player_.animT = 0.f;
    pushFx(AttackFxKind::Ring, 36.f, 0.f, 0.26f, 0x80D8FF);
    queueSfx(SfxId::Dodge);
    for (Monster& monster : monsters_) {
        if (monster.state != ActorState::Dead && lengthOf(monster.x - player_.x, monster.y - player_.y) < 36.f) {
            bool crit = false;
            hurtMonster(monster, rollDamage(10.f, &crit), 8.f, crit, 20.f);
        }
    }
}

bool Session::robotMelee() const {
    return player_.hero == HeroClass::Robot
        && (player_.skillD == kSkillMelee || player_.skillF == kSkillMelee || player_.skillC == kSkillMelee);
}

// 一次肘击：只扣体力，特效与判定都沿用战士普攻（伤害在 updateMonsters 里结算）
void Session::swingMelee() {
    player_.stamina = std::max(0.f, player_.stamina - kMeleeStaminaCost);
    player_.state = ActorState::Attack;
    player_.attackT = 0.36f / atkSpeedMul();
    player_.heavy = false;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushSlashFx();
    queueSfx(SfxId::Swing);
}

void Session::castMelee(float& cooldown) {
    if (cooldown > 0.f || !robotMelee() || player_.state == ActorState::Dodge) {
        return;
    }
    if (player_.stamina < kMeleeStaminaCost) {
        note("体力不足");
        return;
    }
    swingMelee();
    cooldown = kMeleeCooldown * cdMul();
}

void Session::castSpin() {
    player_.state = ActorState::Attack;
    player_.attackT = 0.35f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Spin, 42.f, 0.f, 0.4f, 0xFFE0C2);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) < 42.f) {
            bool crit = false;
            const float dmg = rollDamage(16.f, &crit);
            hurtMonster(monster, dmg, 12.f, crit, 14.f);
        }
    }
    breakMazeWallsRadius(player_.x, player_.y, 42.f);
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
    pushFx(AttackFxKind::Pulse, 64.f, 0.f, 0.4f, 0xB070FF);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) < 64.f) {
            bool crit = false;
            const float dmg = rollDamage(12.f, &crit);
            hurtMonster(monster, dmg, 8.f, crit, 12.f);
        }
    }
    breakMazeWallsRadius(player_.x, player_.y, 64.f);
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
    // 判定是释放瞬间的一整片扇形，特效画成一道快速推满射程的大月牙，并提前散掉
    pushFx(AttackFxKind::Qi, kQiRange, kMeleeConeHalf, 0.26f, 0xBFE0FF);
    queueSfx(SfxId::Skill);
    for (Monster& monster : monsters_) {
        const float dx = monster.x - player_.x;
        const float dy = monster.y - player_.y;
        const float dist = lengthOf(dx, dy);
        if (dist < kQiRange && dist > 0.01f) {
            const float dot = (dx / dist) * player_.facingX + (dy / dist) * player_.facingY;
            if (dot > kMeleeConeDot) {
                bool crit = false;
                const float dmg = rollDamage(18.f, &crit);
                hurtMonster(monster, dmg, 12.f, crit, 16.f);
            }
        }
    }
    breakMazeWallsCone(kQiRange, kMeleeConeDot);
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
    pushFx(AttackFxKind::Lunge, 72.f, 0.35f, 0.3f, 0x8FB8FF);
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
    breakMazeWallsCone(48.f, 0.2f);
    checkTalents();
}

void Session::castBurial(float& cooldown) {
    if (cooldown > 0.f || player_.mp < 40.f) {
        return;
    }
    player_.mp -= 40.f;
    cooldown = 9.f * cdMul();
    player_.skillCasts += 1;
    player_.burialStage = 1;
    player_.burialT = kBurialStage1Life;
    player_.burialNext = kBurialStage1Hit;
    player_.burialR = 96.f;
    player_.state = ActorState::Attack;
    player_.attackT = 0.55f / atkSpeedMul();
    player_.heavy = true;
    player_.animT = 0.f;
    player_.attackId += 1;
    queueSfx(SfxId::Skill);
    Audio::instance().playBurialVoice();
    note("万葬");
}

// 一段出伤：法阵铺开后的第一段；二段出伤：第一段特效结束、六芒星落下时的结算
void Session::burialBlast(int stage) {
    const bool first = stage == 1;
    const float base = first ? 16.f : 22.f;
    const float poise = first ? 10.f : 16.f;
    const float knock = first ? 6.f : 14.f;
    player_.attackId += 1;
    pushFx(AttackFxKind::Pulse, player_.burialR, 0.f, first ? 0.35f : 0.45f, first ? 0xC040FF : 0xFF7BE8);
    queueSfx(first ? SfxId::Skill : SfxId::Explode);
    for (Monster& monster : monsters_) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        if (lengthOf(monster.x - player_.x, monster.y - player_.y) <= player_.burialR) {
            bool crit = false;
            const float dmg = rollDamage(base, &crit);
            hurtMonster(monster, dmg, poise, crit, knock);
        }
    }
    breakMazeWallsRadius(player_.x, player_.y, player_.burialR);
    checkTalents();
}

void Session::updateBurial(float dt) {
    if (player_.burialStage <= 0) {
        return;
    }
    player_.burialT = std::max(0.f, player_.burialT - dt);
    if (player_.burialNext > 0.f) {
        player_.burialNext -= dt;
        if (player_.burialNext <= 0.f) {
            burialBlast(player_.burialStage);
        }
    }
    if (player_.burialT > 0.f) {
        return;
    }
    if (player_.burialStage == 1) {
        player_.burialStage = 2;
        player_.burialT = kBurialStage2Life;
        player_.burialNext = kBurialStage2Hit;
        return;
    }
    player_.burialStage = 0;
    player_.burialT = 0.f;
    player_.burialNext = 0.f;
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
    pushFx(AttackFxKind::Pulse, 28.f, 0.f, 0.35f, 0x60FF90);
    queueVfx(VfxKind::Heal, player_.x, player_.y);
    queueSfx(SfxId::Heal);
    note(QString("治疗术 +%1").arg(int(amount)));
    checkTalents();
}

void Session::castBerserk(float& cooldown, const QString& name) {
    if (cooldown > 0.f || player_.mp < 18.f) {
        return;
    }
    player_.mp -= 18.f;
    cooldown = 8.f * cdMul();
    player_.skillCasts += 1;
    player_.berserkT = 3.f;
    pushFx(AttackFxKind::Pulse, 36.f, 0.f, 0.4f, 0xFF4A3A);
    queueVfx(VfxKind::Rage, player_.x, player_.y);
    queueSfx(SfxId::Skill);
    note(name);
    checkTalents();
}

void Session::castOverload(float& cooldown) {
    if (cooldown > 0.f || player_.mp < 18.f) {
        return;
    }
    player_.mp -= 18.f;
    cooldown = 20.f * cdMul();
    player_.skillCasts += 1;
    player_.overloadT = 10.f;
    pushFx(AttackFxKind::Pulse, 36.f, 0.f, 0.4f, 0xFFA040);
    queueSfx(SfxId::Skill);
    note("过载");
    checkTalents();
}

void Session::castMagField(float& cooldown) {
    const float cost = 20.f * skillCostMul();
    if (cooldown > 0.f || player_.mp < cost) {
        return;
    }
    player_.mp -= cost;
    cooldown = 10.f * cdMul();
    player_.skillCasts += 1;
    player_.fieldT = 6.f;
    player_.fieldTick = 0.f;
    pushFx(AttackFxKind::Ring, 28.f, 0.7f, 0.3f, 0x60B0FF);
    queueSfx(SfxId::Skill);
    note("磁力场");
    checkTalents();
}

void Session::castMedkit(float& cooldown) {
    const float cost = 16.f * skillCostMul();
    if (cooldown > 0.f || player_.mp < cost) {
        return;
    }
    player_.mp -= cost;
    cooldown = 16.f * cdMul();
    player_.skillCasts += 1;
    player_.medkitT = 3.f;
    pushFx(AttackFxKind::Pulse, 28.f, 0.f, 0.35f, 0x60FF90);
    queueVfx(VfxKind::Heal, player_.x, player_.y);
    queueSfx(SfxId::Heal);
    note("战术医疗包");
    checkTalents();
}

void Session::updateRobotBuffs(float dt) {
    player_.overloadT = std::max(0.f, player_.overloadT - dt);
    if (player_.medkitT > 0.f) {
        const float step = std::min(dt, player_.medkitT);
        player_.medkitT -= step;
        player_.hp = std::min(player_.maxHp, player_.hp + player_.maxHp * 0.24f / 3.f * step);
    }
    if (player_.fieldT <= 0.f) {
        return;
    }
    player_.fieldT = std::max(0.f, player_.fieldT - dt);
    player_.fieldTick -= dt;
    if (player_.fieldTick > 0.f) {
        return;
    }
    player_.fieldTick = 0.4f;
    // 要盖住近战怪的出手距离（28），否则贴身怪碰不到磁场
    constexpr float kFieldRadius = 30.f;
    for (Monster& monster : monsters_) {
        if (monster.state != ActorState::Dead && lengthOf(monster.x - player_.x, monster.y - player_.y) <= kFieldRadius) {
            bool crit = false;
            hurtMonster(monster, rollDamage(8.f, &crit), 4.f, crit, 4.f);
        }
    }
}

void Session::toggleFlight() {
    const bool jet = player_.hero == HeroClass::Robot;
    if (player_.hero != HeroClass::Mage && !jet) {
        return;
    }
    if (player_.flying) {
        player_.flying = false;
        note(jet ? "关闭喷气背包" : "落地");
        return;
    }
    if (player_.stamina < 5.f && player_.mp < 5.f) {
        return;
    }
    player_.flying = true;
    player_.skillCasts += 1;
    note(jet ? "喷气背包" : "飞行");
    if (jet) {
        queueSfx(SfxId::Dodge);
    }
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
            note(player_.hero == HeroClass::Robot ? "燃料耗尽，落地" : "体力耗尽，落地");
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
    updateDrones(dt);
    updateBolts(dt);
    updateFloats(dt);
    updateAttackFx(dt);
    updateRuin(dt);
    if (player_.state != ActorState::Dead) {
        spawn(dt);
    } else if (player_.animT > 0.85f) {
        // 死亡瞬间被动触发意识回归符咒：先让玩家选回归还是结算
        if (reviveState_ == ReviveState::None && talismanCount() > 0) {
            reviveState_ = ReviveState::Offered;
            revivePrompt_ = true;
            paused_ = true;
            queueSfx(SfxId::Level);
            note("意识回归符咒生效");
        } else {
            finishRun(EndReason::Death);
        }
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
    updateBurial(dt);
    player_.berserkT = std::max(0.f, player_.berserkT - dt);
    if (player_.state != ActorState::Dead) {
        updateRobotBuffs(dt);
    }
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
        const int pass = playerPass();
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
            const int pass = playerPass();
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
        pushFx(AttackFxKind::Pulse, 24.f, 0.f, 0.3f, 0x60FF90);
        queueVfx(VfxKind::Heal, player_.x, player_.y);
        queueSfx(SfxId::Heal);
    }
    // 突进类技能进入 Dodge 后本帧不再处理移动，否则会被改回 Run
    if (input.rEdge) {
        castSlot(player_.skillD, player_.cdD);
        if (player_.state == ActorState::Dodge) {
            return;
        }
    }
    if (input.fEdge) {
        castSlot(player_.skillF, player_.cdF);
        if (player_.state == ActorState::Dodge) {
            return;
        }
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
    if (input.gEdge && player_.talentGuide) {
        player_.seekOn = !player_.seekOn;
        note(player_.seekOn ? "寻路开启" : "寻路关闭");
    }

    const float as = atkSpeedMul();
    if (player_.attackT > 0.f) {
        player_.attackT -= dt;
        slashHostileBolts();
        if (!rangedHero() && !player_.heavy && player_.attackT > 0.12f && player_.attackT < 0.30f && wallStrikeId_ != player_.attackId) {
            wallStrikeId_ = player_.attackId;
            breakMazeWallsCone(34.f, 0.35f);
        }
        if (player_.attackT <= 0.f) {
            player_.state = ActorState::Idle;
            player_.heavy = false;
        }
    } else if (input.lmb && player_.hero == HeroClass::Robot) {
        if (!player_.reloadLatch) {
            player_.heavyCharge += dt;
            if (player_.heavyCharge >= kRobotReloadHold) {
                player_.heavyCharge = 0.f;
                player_.reloadLatch = true;
                if (player_.ammo < kRobotMagazine) {
                    player_.ammo = kRobotMagazine;
                    note("换弹完成");
                    queueSfx(SfxId::Ui);
                }
            }
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
    if (input.lmbUp && player_.reloadLatch) {
        player_.reloadLatch = false;
        player_.heavyCharge = 0.f;
    }
    if (input.lmbUp && player_.attackT <= 0.f && player_.heavyCharge > 0.f) {
        player_.heavyCharge = 0.f;
        const bool robot = player_.hero == HeroClass::Robot;
        // 机甲人带了「肘击」：轻击改为肘击挥击，耗体力不耗弹药
        if (robot && robotMelee()) {
            if (player_.stamina < kMeleeStaminaCost) {
                note("体力不足");
            } else {
                swingMelee();
            }
        } else if (robot && player_.ammo <= 0) {
            note("弹匣已空，长按左键换弹");
        } else {
            player_.state = ActorState::Attack;
            player_.attackT = (robot ? kRobotShotGap : 0.36f) / as;
            player_.heavy = false;
            player_.animT = 0.f;
            player_.attackId += 1;
            if (player_.hero == HeroClass::Mage) {
                fireMageBolt(false);
            } else if (robot) {
                player_.ammo -= 1;
                fireRobotShot(0.f, 10.f);
                queueSfx(SfxId::Swing);
            } else {
                pushSlashFx();
                queueSfx(SfxId::Swing);
            }
        }
    }
    if (!input.lmb) {
        player_.heavyCharge = 0.f;
        player_.reloadLatch = false;
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
    if (player_.overloadT > 0.f) {
        speed *= 1.25f;
    }
    if (player_.state == ActorState::Attack) {
        speed *= 0.45f;
    }
    const int pass = playerPass();
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
        if (map_.inRuin(tileOf(sx), tileOf(sy))) {
            continue;
        }
        if (map_.walkable(tileOf(sx), tileOf(sy))) {
            x = (tileOf(sx) + 0.5f) * kTile;
            y = (tileOf(sy) + 0.5f) * kTile;
            return true;
        }
    }
    return false;
}

void Session::spawn(float dt) {
    // 诅咒：余光偶尔盯上你，额外刷一只双倍血量的精英
    if (player_.cursed) {
        eliteCd_ -= dt;
        if (eliteCd_ <= 0.f) {
            eliteCd_ = 22.f + float(nextRand() % 22u);
            spawnElite();
        }
    }
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
    if (time_ > 30.f && roll >= 86u) {
        kind = MonsterKind::Killbot;
    } else if (time_ > 20.f && roll < 16u) {
        kind = MonsterKind::Caster;
    } else if (time_ > 25.f && roll < 32u) {
        kind = MonsterKind::Mushroom;
    } else if (time_ > 15.f && roll < 58u) {
        kind = MonsterKind::Skeleton;
    }
    spawnMonster(kind, x, y);
}

void Session::spawnElite() {
    float x = 0.f;
    float y = 0.f;
    if (!findSpawn(x, y)) {
        return;
    }
    MonsterKind kind = MonsterKind::Skeleton;
    switch (nextRand() % 4u) {
    case 1:
        kind = MonsterKind::Mushroom;
        break;
    case 2:
        kind = MonsterKind::Caster;
        break;
    case 3:
        kind = MonsterKind::Killbot;
        break;
    default:
        break;
    }
    const std::size_t before = monsters_.size();
    spawnMonster(kind, x, y);
    if (monsters_.size() <= before) {
        return;
    }
    Monster& elite = monsters_.back();
    elite.elite = true;
    elite.maxHp *= 2.f;
    elite.hp = elite.maxHp;
    elite.maxPoise *= 1.5f;
    elite.poise = elite.maxPoise;
    AttackFx fx;
    fx.kind = AttackFxKind::Ring;
    fx.x = elite.x;
    fx.y = elite.y - 8.f;
    fx.radius = 46.f;
    fx.life = 0.6f;
    fx.maxLife = 0.6f;
    fx.color = 0x8A2BE2;
    attackFx_.push_back(fx);
    queueVfx(VfxKind::Rage, elite.x, elite.y, 20.f);
    queueSfx(SfxId::Skill);
    note("余光注意到了你：精英怪物出现");
}

void Session::updateBolts(float dt) {
    for (Bolt& bolt : bolts_) {
        // 本帧已被无人机拦下的子弹
        if (bolt.life <= 0.f) {
            continue;
        }
        bolt.life -= dt;
        bolt.age += dt;
        if (bolt.life <= 0.f && bolt.blast > 0.f) {
            explodeBolt(bolt);
            continue;
        }
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
            if (bolt.blast > 0.f) {
                explodeBolt(bolt);
            } else if (!bolt.hostile) {
                commitBrokenWalls(breakMazeWallTile(tileOf(bolt.x), tileOf(bolt.y)));
            }
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
            if (monster.kind == MonsterKind::Eye && ruin_.arrive >= 0.f) {
                continue;
            }
            if (lengthOf(monster.x - bolt.x, monster.y - bolt.y) < 14.f) {
                if (bolt.blast > 0.f) {
                    explodeBolt(bolt);
                } else {
                    hurtMonster(monster, bolt.damage, 6.f, bolt.crit, 16.f);
                }
                bolt.life = 0.f;
                break;
            }
        }
    }
    bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(), [](const Bolt& bolt) { return bolt.life <= 0.f; }), bolts_.end());
}

bool Session::chaseOffscreen(Monster& monster, float dt) {
    const float dx = player_.x - monster.x;
    const float dy = player_.y - monster.y;
    const float dist = lengthOf(dx, dy);
    if (dist < 1.f) {
        return false;
    }
    // 比可视范围再出去一截才切换，避免贴着画面边缘来回变速度
    const float outsideX = std::abs(dx) - (kViewW * 0.5f + 24.f);
    const float outsideY = std::abs(dy) - (kViewH * 0.5f + 24.f);
    const float outside = std::max(outsideX, outsideY);
    if (outside <= 0.f) {
        return false;
    }

    float base = 40.f;
    if (monster.kind == MonsterKind::Skeleton) {
        base = 36.f;
    } else if (monster.kind == MonsterKind::Mushroom) {
        base = 32.f;
    } else if (monster.kind == MonsterKind::Flyer) {
        base = 48.f;
    } else if (monster.kind == MonsterKind::Caster) {
        base = 34.f;
    } else if (monster.kind == MonsterKind::Killbot) {
        base = 38.f;
    }
    // 玩家奔跑约 96。离画面越远越快，贴回边缘时回到原本速度
    const float speed = base + std::min(100.f, outside * 0.55f);
    // 飞行怪只被迷宫墙挡住；其余画面外可以迈过岩石和灌木，水和迷宫墙仍然绕开
    int pass = monster.kind == MonsterKind::Flyer ? 2 : 1;
    // 被击退或互相挤推进水里时放行：陷在过不去的地形里会永久卡住，先迈出来再绕行
    if (pass < 2 && map_.blockedAt(monster.x, monster.y, kMonsterRadius, pass)) {
        pass = 2;
    }

    const float inv = 1.f / dist;
    const float towardX = dx * inv;
    const float towardY = dy * inv;
    float dirX = towardX;
    float dirY = towardY;
    const float probe = kMonsterRadius + 2.f;
    const auto probeBlocked = [&](float x, float y) {
        return map_.blockedAt(monster.x + x * probe, monster.y + y * probe, kMonsterRadius, pass);
    };
    if (probeBlocked(towardX, towardY)) {
        int nx = 0;
        int ny = 0;
        bool following = false;
        if (paths_.nextTile(tileOf(monster.x), tileOf(monster.y), nx, ny)) {
            const float gx = (nx + 0.5f) * kTile - monster.x;
            const float gy = (ny + 0.5f) * kTile - monster.y;
            const float gd = lengthOf(gx, gy);
            if (gd > 0.5f && !probeBlocked(gx / gd, gy / gd)) {
                dirX = gx / gd;
                dirY = gy / gd;
                following = true;
            }
        }
        if (!following && nearestWalkableTile(tileOf(monster.x), tileOf(monster.y), nx, ny)) {
            // 画面外能踩过岩石和灌木，脚下这格就不在寻路图里：先挪回相邻可走的格子
            const float gx = (nx + 0.5f) * kTile - monster.x;
            const float gy = (ny + 0.5f) * kTile - monster.y;
            const float gd = lengthOf(gx, gy);
            if (gd > 0.5f && !probeBlocked(gx / gd, gy / gd)) {
                dirX = gx / gd;
                dirY = gy / gd;
                following = true;
            }
        }
        if (!following) {
            // 寻路覆盖不到时贴着水或墙侧移；四周都堵住就后退，避免嵌在凹角里
            const float turns[5][2] = {
                {towardX * 0.7071f - towardY * 0.7071f, towardY * 0.7071f + towardX * 0.7071f},
                {towardX * 0.7071f + towardY * 0.7071f, towardY * 0.7071f - towardX * 0.7071f},
                {-towardY, towardX},
                {towardY, -towardX},
                {-towardX, -towardY},
            };
            float best = -2.f;
            for (const auto& turn : turns) {
                if (probeBlocked(turn[0], turn[1])) {
                    continue;
                }
                const float score = turn[0] * towardX + turn[1] * towardY;
                if (score > best) {
                    best = score;
                    dirX = turn[0];
                    dirY = turn[1];
                }
            }
        }
    }

    monster.attackT = 0.f;
    tryMove(monster.x, monster.y, dirX * speed, dirY * speed, dt, kMonsterRadius, pass, nullptr);
    monster.state = ActorState::Run;
    return true;
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
            queueVfx(VfxKind::Kill, monster.x, monster.y, 0.f, false, monster.kind);
            if (!monster.scored) {
                const bool overLevel = monster.level > player_.level;
                const int gained = scoreFor(monster.kind, monster.level) * (monster.elite ? 2 : 1);
                score_ += gained;
                gainXp(gained);
                monster.scored = true;
                if (overLevel) {
                    player_.underdogKills += 1;
                }
                if (monster.kind == MonsterKind::Eye) {
                    onEyeDefeated();
                } else {
                    player_.worldKills += 1;
                }
                checkTalents();
            }
            continue;
        }

        const float dx = player_.x - monster.x;
        const float dy = player_.y - monster.y;
        const float dist = lengthOf(dx, dy);
        faceToward(monster.facingX, monster.facingY, monster.flip, dx, dy);
        // 画面外能踩过岩石和灌木，踏进画面时脚下若正压着这类地形就会卡死：放行到走出来为止
        int pass = monster.kind == MonsterKind::Flyer ? 2 : 0;
        if (pass == 0 && map_.blockedAt(monster.x, monster.y, kMonsterRadius, 0)) {
            pass = 1;
        }

        if (monster.kind == MonsterKind::Eye) {
            updateEye(monster, dt);
            continue;
        }

        if ((!rangedHero() || robotMelee()) && player_.state == ActorState::Attack && !player_.heavy && player_.attackT > 0.12f && player_.attackT < 0.30f && monster.lastHitBy != player_.attackId) {
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

        if (chaseOffscreen(monster, dt)) {
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

        if (monster.kind == MonsterKind::Killbot) {
            updateKillbot(monster, dt, dist);
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
        } else if (monster.kind != MonsterKind::Flyer && dist > 20.f && nearestWalkableTile(tx, ty, nx, ny)) {
            gx = (nx + 0.5f) * kTile;
            gy = (ny + 0.5f) * kTile;
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
            if (monsters_[i].kind == MonsterKind::Eye || monsters_[j].kind == MonsterKind::Eye) {
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
                // 互推不看地形：先确认落点站得住，否则会把怪挤进水里或墙里再也出不来
                const float push = (14.f - d) * 0.5f;
                const float ux = dx / d * push;
                const float uy = dy / d * push;
                const int passI = monsters_[i].kind == MonsterKind::Flyer ? 2 : 0;
                const int passJ = monsters_[j].kind == MonsterKind::Flyer ? 2 : 0;
                if (!map_.blockedAt(monsters_[i].x - ux, monsters_[i].y - uy, kMonsterRadius, passI)) {
                    monsters_[i].x -= ux;
                    monsters_[i].y -= uy;
                }
                if (!map_.blockedAt(monsters_[j].x + ux, monsters_[j].y + uy, kMonsterRadius, passJ)) {
                    monsters_[j].x += ux;
                    monsters_[j].y += uy;
                }
            }
        }
    }

    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(), [](const Monster& monster) {
        return monster.state == ActorState::Dead && monster.animT > 0.7f;
    }), monsters_.end());
}

// 机器人小兵：保持中距离绕玩家横移，定时点射；contactCd 兼作射击冷却
void Session::updateKillbot(Monster& monster, float dt, float dist) {
    const bool playerAlive = player_.state != ActorState::Dead;
    if (monster.attackT > 0.f) {
        monster.attackT -= dt;
        monster.state = ActorState::Attack;
        if (!monster.attackApplied && monster.attackT <= 0.2f) {
            monster.attackApplied = true;
            if (playerAlive) {
                Bolt bolt;
                bolt.hostile = true;
                bolt.robot = true;
                bolt.x = monster.x + monster.facingX * 8.f;
                bolt.y = monster.y + monster.facingY * 8.f;
                bolt.lift = 12.f;
                bolt.vx = monster.facingX * 180.f;
                bolt.vy = monster.facingY * 180.f;
                bolt.life = 1.0f;
                bolt.damage = scaledMonsterDamage(monster, 8.f);
                bolts_.push_back(bolt);
            }
        }
        if (monster.attackT <= 0.f) {
            monster.state = ActorState::Idle;
        }
        return;
    }
    if (playerAlive && dist < 140.f && dist > 24.f && monster.contactCd <= 0.f) {
        monster.attackT = 0.45f;
        monster.attackApplied = false;
        monster.contactCd = 1.5f + float(monster.id % 3) * 0.2f;
        monster.animT = 0.f;
        monster.state = ActorState::Attack;
        return;
    }
    float mx = 0.f;
    float my = 0.f;
    float speed = 38.f;
    if (dist > 96.f) {
        float gx = player_.x;
        float gy = player_.y;
        int nx = 0;
        int ny = 0;
        if (paths_.nextTile(tileOf(monster.x), tileOf(monster.y), nx, ny)) {
            gx = (nx + 0.5f) * kTile;
            gy = (ny + 0.5f) * kTile;
        }
        mx = gx - monster.x;
        my = gy - monster.y;
    } else if (dist < 56.f) {
        mx = -monster.facingX;
        my = -monster.facingY;
    } else {
        const float side = (monster.id & 1) ? 1.f : -1.f;
        mx = -monster.facingY * side;
        my = monster.facingX * side;
        speed = 26.f;
    }
    const float md = lengthOf(mx, my);
    if (md > 1.f || (md > 0.01f && dist <= 96.f)) {
        tryMove(monster.x, monster.y, mx / md * speed, my / md * speed, dt, kMonsterRadius, 0, nullptr);
        monster.state = ActorState::Run;
    } else {
        monster.state = ActorState::Idle;
    }
}

void Session::recomputeGear() {
    player_.weaponAtk = 0;
    player_.maxHp = baseMaxHp_;
    player_.maxMp = baseMaxMp_;
    player_.armor = baseArmor_;
    player_.hp = std::min(player_.hp, player_.maxHp);
    player_.mp = std::min(player_.mp, player_.maxMp);
}

void Session::syncRuinMap() {
    if (ruin_.active) {
        map_.setRuin(ruin_.originX, ruin_.originY, MazeRuin::kSize, MazeRuin::kSize, ruin_.walls());
    } else {
        map_.clearRuin();
    }
}

void Session::dismissRuin() {
    ruin_.active = false;
    ruin_.wall.clear();
    ruin_.route.clear();
    ruin_.pulseR = -1.f;
    map_.clearRuin();
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(), [](const Monster& monster) {
        return monster.kind == MonsterKind::Eye;
    }), monsters_.end());
}

void Session::spawnEye(float hp, float shield, int level) {
    Monster monster;
    monster.id = nextId_++;
    monster.kind = MonsterKind::Eye;
    monster.x = ruin_.centerX();
    monster.y = ruin_.centerY();
    const bool locked = level > 0;
    const int shown = locked ? level : 1;
    setupMonster(monster, locked ? std::max(1, shown - 99) : 1);
    monster.level = shown;
    if (hp >= 0.f) {
        monster.hp = std::min(monster.maxHp, hp);
    }
    if (shield >= 0.f) {
        monster.shield = std::min(monster.maxShield, shield);
    }
    monsters_.push_back(monster);
    ruin_.attackStep = 0;
    ruin_.attackCd = 1.2f;
    ruin_.pulseR = -1.f;
    ruin_.pulseHit = false;
}

bool Session::spawnRuin() {
    const int px = tileOf(player_.x);
    const int py = tileOf(player_.y);
    for (int attempt = 0; attempt < 40; ++attempt) {
        const float angle = float(nextRand() % 628u) / 100.f;
        const int dist = 52 + int(nextRand() % 48u);
        const int cx = px + int(std::cos(angle) * float(dist));
        const int cy = py + int(std::sin(angle) * float(dist));
        const int ox = cx - MazeRuin::kSize / 2;
        const int oy = cy - MazeRuin::kSize / 2;
        const int x1 = ox + MazeRuin::kSize - 1;
        const int y1 = oy + MazeRuin::kSize - 1;
        const int nearX = std::clamp(px, ox, x1);
        const int nearY = std::clamp(py, oy, y1);
        if (std::max(std::abs(px - nearX), std::abs(py - nearY)) < 36) {
            continue;
        }
        if (!ruin_.generate(ox, oy, nextRand() | 1u)) {
            continue;
        }
        ruin_.phase = MazeRuin::Phase::Live;
        ruin_.cooldown = 0.f;
        syncRuinMap();
        spawnEye();
        for (Monster& monster : monsters_) {
            if (monster.kind == MonsterKind::Eye || monster.state == ActorState::Dead) {
                continue;
            }
            if (!ruin_.contains(tileOf(monster.x), tileOf(monster.y))) {
                continue;
            }
            float ox = monster.x;
            float oy = monster.y;
            if (ruin_.entranceX == 0) {
                ox = (float(ruin_.originX) - 0.5f) * float(kTile);
                oy = (float(ruin_.originY + ruin_.entranceY) + 0.5f) * float(kTile);
            } else if (ruin_.entranceX == MazeRuin::kSize - 1) {
                ox = (float(ruin_.originX + MazeRuin::kSize) + 0.5f) * float(kTile);
                oy = (float(ruin_.originY + ruin_.entranceY) + 0.5f) * float(kTile);
            } else if (ruin_.entranceY == 0) {
                ox = (float(ruin_.originX + ruin_.entranceX) + 0.5f) * float(kTile);
                oy = (float(ruin_.originY) - 0.5f) * float(kTile);
            } else {
                ox = (float(ruin_.originX + ruin_.entranceX) + 0.5f) * float(kTile);
                oy = (float(ruin_.originY + MazeRuin::kSize) + 0.5f) * float(kTile);
            }
            monster.x = ox;
            monster.y = oy;
        }
        note("迷宫遗迹出现");
        return true;
    }
    ruin_.clear();
    map_.clearRuin();
    return false;
}

void Session::updateEye(Monster& monster, float dt) {
    monster.x = ruin_.centerX();
    monster.y = ruin_.centerY();
    if (ruin_.phase != MazeRuin::Phase::Live || ruin_.bossDead) {
        monster.state = ActorState::Idle;
        return;
    }
    const float dx = player_.x - monster.x;
    const float dy = player_.y - monster.y;
    const float dist = lengthOf(dx, dy);
    faceToward(monster.facingX, monster.facingY, monster.flip, dx, dy);
    if (player_.state == ActorState::Dead || ruin_.arrive > 0.f || !ruin_.enteredPlaza) {
        monster.state = ActorState::Idle;
        return;
    }
    if (monster.stunT > 0.f) {
        ruin_.pulseR = -1.f;
        monster.state = ActorState::Hurt;
        return;
    }
    const bool inPlaza = ruin_.inPlaza(tileOf(player_.x), tileOf(player_.y));
    if (ruin_.pulseR >= 0.f) {
        const float prev = ruin_.pulseR;
        ruin_.pulseR += 68.f * dt;
        if (!ruin_.pulseHit && inPlaza && dist >= prev - 2.f && dist <= ruin_.pulseR + 8.f) {
            ruin_.pulseHit = true;
            const int statLevel = std::max(1, monster.level - 99);
            hurtPlayer(16.f * (1.f + float(statLevel - 1) * 0.1f), &monster);
        }
        if (ruin_.pulseR > 128.f) {
            ruin_.pulseR = -1.f;
        }
        monster.state = ActorState::Attack;
        return;
    }
    ruin_.attackCd -= dt;
    if (ruin_.attackCd > 0.f || dist < 8.f || dist > 320.f) {
        if (monster.state != ActorState::Hurt) {
            monster.state = ActorState::Idle;
        }
        return;
    }
    auto shoot = [&](float angle, float speed, float damage, float life) {
        Bolt bolt;
        bolt.hostile = true;
        bolt.x = monster.x;
        bolt.y = monster.y - 18.f;
        bolt.vx = std::cos(angle) * speed;
        bolt.vy = std::sin(angle) * speed;
        bolt.life = life;
        const int statLevel = std::max(1, monster.level - 99);
        bolt.damage = damage * (1.f + float(statLevel - 1) * 0.1f);
        bolts_.push_back(bolt);
    };
    const float aim = std::atan2(dy, dx);
    switch (ruin_.attackStep % 3) {
    case 0:
        shoot(aim, 190.f, 12.f, 1.4f);
        ruin_.attackCd = 1.35f;
        break;
    case 1:
        for (int i = -2; i <= 2; ++i) {
            shoot(aim + float(i) * 0.30f, 145.f, 7.f, 1.7f);
        }
        ruin_.attackCd = 1.9f;
        break;
    default:
        ruin_.pulseR = 8.f;
        ruin_.pulseHit = false;
        ruin_.attackCd = 0.9f;
        break;
    }
    ruin_.attackStep = (ruin_.attackStep + 1) % 3;
    monster.state = ActorState::Attack;
}

void Session::onEyeDefeated() {
    ruin_.bossDead = true;
    ruin_.phase = MazeRuin::Phase::Leave;
    ruin_.pulseR = -1.f;
    triggerShake();
    addItem(kItemReturnTalisman, 1);
    note(QString("克苏鲁之眼被击败　获得 %1").arg(itemText(kItemReturnTalisman).name));
    eyeDefeats_ += 1;
    if (eyeDefeats_ == 2 || (nextRand() % 5u) == 0u) {
        voidPrompt_ = true;
    }
}

void Session::triggerShake() {
    shakeT_ = 0.9f;
}

void Session::cameraShake(float& sx, float& sy) const {
    if (shakeT_ <= 0.f) {
        sx = 0.f;
        sy = 0.f;
        return;
    }
    constexpr float kDur = 0.9f;
    const float amp = 6.f * (shakeT_ / kDur);
    const float t = kDur - shakeT_;
    sx = amp * std::sin(t * 11.f * 6.2831853f);
    sy = amp * 0.65f * std::sin(t * 7.f * 6.2831853f + 0.7f);
}

void Session::updateRuin(float dt) {
    shakeT_ = std::max(0.f, shakeT_ - dt);
    const bool wantRed = ruin_.phase == MazeRuin::Phase::Live && !ruin_.bossDead
        && ruin_.inPlaza(tileOf(player_.x), tileOf(player_.y));
    const float target = wantRed ? 1.f : 0.f;
    plazaRed_ += (target - plazaRed_) * std::min(1.f, dt * 3.f);
    if (plazaRed_ < 0.01f) {
        plazaRed_ = 0.f;
    }

    if (ruin_.phase == MazeRuin::Phase::None || (ruin_.phase == MazeRuin::Phase::Wait && ruin_.cooldown <= 0.f)) {
        if (!spawnRuin()) {
            ruin_.phase = MazeRuin::Phase::Wait;
            ruin_.cooldown = 5.f;
        }
        return;
    }
    if (ruin_.phase == MazeRuin::Phase::Wait) {
        ruin_.cooldown -= dt;
        return;
    }
    if (ruin_.phase == MazeRuin::Phase::Live) {
        if (ruin_.arrive > 0.f) {
            ruin_.arrive -= dt;
            if (ruin_.arrive <= 0.f) {
                ruin_.arrive = -1.f;
            }
        }
        if (!ruin_.enteredPlaza && ruin_.inPlaza(tileOf(player_.x), tileOf(player_.y))) {
            applyEyeLevel();
            ruin_.enteredPlaza = true;
            ruin_.arrive = 1.7f;
            triggerShake();
            note("投影降临");
        }
        return;
    }
    if (ruin_.phase == MazeRuin::Phase::Leave && !ruin_.contains(tileOf(player_.x), tileOf(player_.y))) {
        dismissRuin();
        ruin_.phase = MazeRuin::Phase::Wait;
        ruin_.bossDead = false;
        ruin_.enteredPlaza = false;
        ruin_.cooldown = 45.f;
        note("遗迹沉入地下");
    }
}

void Session::restoreRuin(const QJsonObject& game) {
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(), [](const Monster& monster) {
        return monster.kind == MonsterKind::Eye;
    }), monsters_.end());
    if (!game.contains("ruin")) {
        dismissRuin();
        ruin_.phase = MazeRuin::Phase::None;
        if (!spawnRuin()) {
            ruin_.phase = MazeRuin::Phase::Wait;
            ruin_.cooldown = 5.f;
        }
        return;
    }
    const QJsonObject ruin = game.value("ruin").toObject();
    const auto phase = MazeRuin::Phase(std::clamp(ruin.value("phase").toInt(), 0, 3));
    if (phase == MazeRuin::Phase::None || phase == MazeRuin::Phase::Wait) {
        dismissRuin();
        ruin_.phase = MazeRuin::Phase::Wait;
        ruin_.cooldown = float(ruin.value("cooldown").toDouble(45.0));
        if (phase == MazeRuin::Phase::None) {
            ruin_.cooldown = 0.f;
        }
        return;
    }
    const int ox = ruin.value("ox").toInt();
    const int oy = ruin.value("oy").toInt();
    const uint32_t mazeSeed = uint32_t(ruin.value("seed").toDouble());
    if (!ruin_.generate(ox, oy, mazeSeed == 0 ? 1u : mazeSeed)) {
        dismissRuin();
        ruin_.phase = MazeRuin::Phase::Wait;
        ruin_.cooldown = 5.f;
        return;
    }
    ruin_.phase = phase;
    ruin_.bossDead = ruin.value("bossDead").toBool(false);
    ruin_.enteredPlaza = ruin.value("entered").toBool(false);
    ruin_.arrive = ruin_.enteredPlaza ? -1.f : 0.f;
    ruin_.cooldown = float(ruin.value("cooldown").toDouble(0));
    const QString wallBits = ruin.value("walls").toString();
    if (wallBits.size() == int(ruin_.wall.size())) {
        for (int i = 0; i < wallBits.size(); ++i) {
            ruin_.wall[size_t(i)] = wallBits.at(i) == QLatin1Char('1') ? 1 : 0;
        }
    }
    syncRuinMap();
    if (phase == MazeRuin::Phase::Live && !ruin_.bossDead) {
        const float hp = ruin.contains("eyeHp") ? float(ruin.value("eyeHp").toDouble()) : -1.f;
        const float shield = ruin.contains("eyeShield") ? float(ruin.value("eyeShield").toDouble()) : -1.f;
        const int level = ruin.value("eyeLevel").toInt(-1);
        spawnEye(hp, shield, level);
    }
}
