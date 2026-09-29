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

// 1 号：默认循环；2 号：回血触发，播一次；3 号：拒绝结束本轮，播一次
enum class BgmId {
    Explore = 1,
    Recover = 2,
    Refuse = 3
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
    void startBgmLoop();
    void ensureBgmLoop();
    void playRecoverBgm();
    // 玩家拒绝结束本轮：切到 3 号曲播一次，播完自动接回 1 号循环
    void playRefuseBgm();
    void stopBgm();
    void setBgmPaused(bool paused);
    void notifyBgmEnded();
    void applyFromSettings();

private:
    Audio() = default;
    void rebuildSfxVolumes();
    void rebuildBgmVolume();
    void playBgm(BgmId id, bool loop);
    QString bgmPath(BgmId id) const;

    bool loaded_ = false;
    bool sfxEnabled_ = true;
    int sfxVolumePercent_ = 70;
    bool bgmEnabled_ = true;
    int bgmVolumePercent_ = 55;
    BgmId currentBgm_ = BgmId::Explore;
    // 正在播放一次性曲目（2 号 / 3 号），放完由 notifyBgmEnded 接回 1 号循环
    bool oneshotPlaying_ = false;
    QString assetDir_;
    QString bgmDir_;
};
