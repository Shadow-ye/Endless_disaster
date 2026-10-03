#pragma once

#include <QString>

enum class SfxId {
    Ui = 0,
    Swing,
    Hit,
    Crit,
    Hurt,
    Skill,
    Dodge,
    Heal,
    Death,
    Level,
    Explode,
    Count
};

// 文件名里的序号就是这里的值（1 号 / 2 号 …），加曲补一个枚举值即可
// 1 号：局内默认循环；2 号：回血触发一次；3 号：拒绝结束本轮一次；
// 4 号：复活后一次；5 号：结算后循环，进主菜单不断，开局才回到 1 号
enum class BgmId {
    Explore = 1,
    Recover = 2,
    Refuse = 3,
    Revive = 4,
    Finale = 5
};

class Audio {
public:
    static Audio& instance();

    void load(const QString& assetDir);
    void setSfxEnabled(bool enabled);
    void setSfxVolume(int percent);
    void setBgmEnabled(bool enabled);
    void setBgmVolume(int percent);
    bool sfxEnabled() const { return sfxEnabled_; }
    int sfxVolume() const { return sfxVolumePercent_; }
    bool bgmEnabled() const { return bgmEnabled_; }
    int bgmVolume() const { return bgmVolumePercent_; }

    // 兼容旧调用
    void setEnabled(bool enabled) { setSfxEnabled(enabled); }
    void setVolume(int percent) { setSfxVolume(percent); }
    bool enabled() const { return sfxEnabled_; }
    int volume() const { return sfxVolumePercent_; }

    void play(SfxId id);
    void playBurialVoice();
    // 战士隐藏连招「次元斩」的配音：起配音时把 BGM 静音（不停播）。
    // finishDimensionVoice() 用于次元斩正常打完（停配音，隔一拍再让 BGM 渐入），
    // stopDimensionVoice() 用于判定失败 / 换曲等需要立刻把 BGM 交回来的场合。
    void playDimensionVoice();
    void stopDimensionVoice();
    // 次元斩打完：配音立即停，BGM 等 kBgmRestoreDelay 秒后渐渐恢复
    void finishDimensionVoice();
    // 战士「I am atomic」的吟唱配音，整段 5 秒：前 4 秒蓄力，最后一秒是引爆
    void playAtomicVoice();
    // 复活 / 拒绝结算本轮时的「继续前进」配音：按角色性别选男声或女声
    void playContinueVoice(bool female);
    // 新的一局：清掉结算后的终曲状态，切回 1 号循环
    void beginRun();
    void startBgmLoop();
    void ensureBgmLoop();
    void playRecoverBgm();
    // 玩家拒绝结束本轮：切到 3 号曲播一次，播完自动接回当前循环曲
    void playRefuseBgm();
    // 复活：切到 4 号曲播一次
    void playReviveBgm();
    // 本局结算：切到 5 号曲循环，回主菜单也不断，直到 beginRun()
    void playFinaleBgm();
    void stopBgm();
    void setBgmPaused(bool paused);
    // 弹出界面时把 BGM 压低而不是停播，界面关掉后回到原音量
    void setBgmDucked(bool ducked);
    void notifyBgmEnded();
    void applyFromSettings();

private:
    Audio() = default;
    void rebuildSfxVolumes();
    void rebuildBgmVolume();
    // 配音接管期间 BGM 静音（照常播，只把音量压到 0），配音结束 / 判定失败再放回来
    void setBgmSilenced(bool silenced);
    // BGM 恢复的渐入：先等一拍，再按 kBgmRestoreFade 秒把音量推上来
    void startBgmRestore();
    void stepBgmRestore();
    void playBgm(BgmId id, bool loop);
    QString bgmPath(BgmId id) const;
    // 当前该循环的曲目：局内是 1 号，结算后是 5 号
    BgmId idleBgm() const { return finaleMode_ ? BgmId::Finale : BgmId::Explore; }

    bool loaded_ = false;
    bool sfxEnabled_ = true;
    int sfxVolumePercent_ = 70;
    bool bgmEnabled_ = true;
    int bgmVolumePercent_ = 55;
    BgmId currentBgm_ = BgmId::Explore;
    // 正在播放一次性曲目（2 号 / 3 号 / 4 号），放完由 notifyBgmEnded 接回循环曲
    bool oneshotPlaying_ = false;
    // 有界面弹出：BGM 照放，音量按 kBgmDuckScale 压低
    bool bgmDucked_ = false;
    // 配音接管：BGM 照放但音量归零，配音一停就放回来
    bool bgmSilenced_ = false;
    // BGM 渐入：bgmFade_ 从 0 涨到 1 的这段时间里，音量按它缩放
    float bgmFade_ = 1.f;
    // 本局已结算：循环曲从 1 号换成 5 号，直到下一局开始
    bool finaleMode_ = false;
    QString assetDir_;
    QString bgmDir_;
};
