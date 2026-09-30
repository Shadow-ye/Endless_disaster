#include "Audio.h"
#include "Platform.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QSoundEffect>
#include <QUrl>

#include <algorithm>
#include <array>
#include <vector>

namespace {
constexpr int kVoices = 3;

const char* sfxFile(SfxId id) {
    switch (id) {
    case SfxId::Ui:
        return "ui.wav";
    case SfxId::Swing:
        return "swing.wav";
    case SfxId::Hit:
        return "hit.wav";
    case SfxId::Crit:
        return "crit.wav";
    case SfxId::Hurt:
        return "hurt.wav";
    case SfxId::Skill:
        return "skill.wav";
    case SfxId::Dodge:
        return "dodge.wav";
    case SfxId::Heal:
        return "heal.wav";
    case SfxId::Death:
        return "death.wav";
    case SfxId::Level:
        return "level.wav";
    case SfxId::Explode:
        return "explode.wav";
    default:
        return "ui.wav";
    }
}

struct AudioState {
    std::array<std::vector<QSoundEffect*>, int(SfxId::Count)> pools{};
    std::array<int, int(SfxId::Count)> cursor{};
    QMediaPlayer* bgm = nullptr;
    QAudioOutput* bgmOut = nullptr;
    QMediaPlayer* burialVoice = nullptr;
    QAudioOutput* burialOut = nullptr;
    QMediaPlayer* atomicVoice = nullptr;
    QAudioOutput* atomicOut = nullptr;
};

AudioState& state() {
    static AudioState s;
    return s;
}

QStringList bgmDirCandidates(const QString& assetDir) {
    return {
        QDir(assetDir).absoluteFilePath("../BGM"),
        QCoreApplication::applicationDirPath() + "/BGM",
        assetDir + "/BGM",
        QDir(assetDir).absoluteFilePath("../../BGM"),
    };
}

QString resolveBgmDir(const QString& assetDir) {
    for (const QString& path : bgmDirCandidates(assetDir)) {
        const QDir dir(QDir::cleanPath(path));
        if (dir.exists()) {
            return dir.absolutePath();
        }
    }
    return QCoreApplication::applicationDirPath() + "/BGM";
}

QString findClip(const QString& assetDir, const QString& token) {
    const QStringList filters = {"*.wav", "*.mp3", "*.m4a", "*.ogg", "*.flac"};
    QString best;
    QDateTime bestTime;
    for (const QString& path : bgmDirCandidates(assetDir)) {
        const QDir dir(QDir::cleanPath(path));
        if (!dir.exists()) {
            continue;
        }
        for (const QString& name : dir.entryList(filters, QDir::Files, QDir::Name)) {
            if (!name.contains(token)) {
                continue;
            }
            const QFileInfo info(dir.absoluteFilePath(name));
            if (best.isEmpty() || info.lastModified() > bestTime) {
                best = info.absoluteFilePath();
                bestTime = info.lastModified();
            }
        }
    }
    return best;
}
}  // namespace

Audio& Audio::instance() {
    static Audio audio;
    return audio;
}

void Audio::load(const QString& assetDir) {
    assetDir_ = assetDir;
    bgmDir_ = resolveBgmDir(assetDir);
    if (!loaded_) {
        const QString sfxDir = assetDir + "/sfx";
        for (int i = 0; i < int(SfxId::Count); ++i) {
            state().pools[i].clear();
            state().cursor[i] = 0;
            const QUrl url = Platform::mediaUrl(sfxDir + "/" + sfxFile(SfxId(i)));
            for (int v = 0; v < kVoices; ++v) {
                auto* effect = new QSoundEffect(QCoreApplication::instance());
                effect->setSource(url);
                effect->setLoopCount(1);
                state().pools[i].push_back(effect);
            }
        }
        state().bgmOut = new QAudioOutput(QCoreApplication::instance());
        state().bgm = new QMediaPlayer(QCoreApplication::instance());
        state().bgm->setAudioOutput(state().bgmOut);
        QObject::connect(state().bgm, &QMediaPlayer::mediaStatusChanged, state().bgm,
            [](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) {
                    Audio::instance().notifyBgmEnded();
                }
            });
        state().burialOut = new QAudioOutput(QCoreApplication::instance());
        state().burialVoice = new QMediaPlayer(QCoreApplication::instance());
        state().burialVoice->setAudioOutput(state().burialOut);
        state().burialVoice->setLoops(1);
        const QString burial = findClip(assetDir, QStringLiteral("万葬"));
        if (!burial.isEmpty()) {
            state().burialVoice->setSource(Platform::mediaUrl(burial));
        }
        state().atomicOut = new QAudioOutput(QCoreApplication::instance());
        state().atomicVoice = new QMediaPlayer(QCoreApplication::instance());
        state().atomicVoice->setAudioOutput(state().atomicOut);
        state().atomicVoice->setLoops(1);
        const QString atomic = findClip(assetDir, QStringLiteral("I am atomic"));
        if (!atomic.isEmpty()) {
            state().atomicVoice->setSource(Platform::mediaUrl(atomic));
        }
        loaded_ = true;
    }
    rebuildSfxVolumes();
    rebuildBgmVolume();
}

QString Audio::bgmPath(BgmId id) const {
    const QDir dir(bgmDir_);
    if (!dir.exists()) {
        return {};
    }
    const QString prefix = id == BgmId::Recover ? QStringLiteral("2")
        : id == BgmId::Refuse                  ? QStringLiteral("3")
                                               : QStringLiteral("1");
    const QStringList files = dir.entryList(QStringList{"*.m4a", "*.mp3", "*.wav", "*.ogg", "*.flac"},
        QDir::Files, QDir::Name);
    for (const QString& name : files) {
        if (name.startsWith(prefix) || name.startsWith(prefix + QStringLiteral("号"))) {
            return dir.absoluteFilePath(name);
        }
    }
    // 回退：按排序取第 1 / 第 2 / 第 3 个文件
    const int want = id == BgmId::Recover ? 1 : id == BgmId::Refuse ? 2 : 0;
    if (want < files.size()) {
        return dir.absoluteFilePath(files.at(want));
    }
    if (!files.isEmpty()) {
        return dir.absoluteFilePath(files.first());
    }
    return {};
}

void Audio::setSfxEnabled(bool enabled) {
    sfxEnabled_ = enabled;
    rebuildSfxVolumes();
}

void Audio::setSfxVolume(int percent) {
    sfxVolumePercent_ = std::max(0, std::min(100, percent));
    rebuildSfxVolumes();
}

void Audio::setBgmEnabled(bool enabled) {
    bgmEnabled_ = enabled;
    rebuildBgmVolume();
    if (!bgmEnabled_) {
        if (state().bgm) {
            state().bgm->stop();
        }
        oneshotPlaying_ = false;
    }
}

void Audio::setBgmVolume(int percent) {
    bgmVolumePercent_ = std::max(0, std::min(100, percent));
    rebuildBgmVolume();
}

void Audio::rebuildSfxVolumes() {
    const float vol = sfxEnabled_ ? float(sfxVolumePercent_) / 100.f : 0.f;
    for (auto& pool : state().pools) {
        for (QSoundEffect* effect : pool) {
            if (effect) {
                effect->setVolume(vol);
            }
        }
    }
    if (state().burialOut) {
        state().burialOut->setVolume(vol);
    }
    if (state().atomicOut) {
        state().atomicOut->setVolume(vol);
    }
}

void Audio::rebuildBgmVolume() {
    if (state().bgmOut) {
        const float vol = bgmEnabled_ ? float(bgmVolumePercent_) / 100.f : 0.f;
        state().bgmOut->setVolume(vol);
    }
}

void Audio::play(SfxId id) {
    if (!loaded_ || !sfxEnabled_ || sfxVolumePercent_ <= 0) {
        return;
    }
    const int index = int(id);
    if (index < 0 || index >= int(SfxId::Count)) {
        return;
    }
    auto& pool = state().pools[index];
    if (pool.empty()) {
        return;
    }
    int& cursor = state().cursor[index];
    QSoundEffect* effect = pool[cursor % int(pool.size())];
    cursor = (cursor + 1) % int(pool.size());
    if (effect->isPlaying()) {
        effect->stop();
    }
    effect->play();
}

void Audio::playBurialVoice() {
    QMediaPlayer* voice = state().burialVoice;
    if (!loaded_ || !sfxEnabled_ || sfxVolumePercent_ <= 0 || !voice || voice->source().isEmpty()) {
        return;
    }
    if (state().burialOut) {
        state().burialOut->setVolume(float(sfxVolumePercent_) / 100.f);
    }
    voice->stop();
    voice->setPosition(0);
    voice->play();
}

void Audio::playAtomicVoice() {
    QMediaPlayer* voice = state().atomicVoice;
    if (!loaded_ || !sfxEnabled_ || sfxVolumePercent_ <= 0 || !voice || voice->source().isEmpty()) {
        return;
    }
    if (state().atomicOut) {
        state().atomicOut->setVolume(float(sfxVolumePercent_) / 100.f);
    }
    voice->stop();
    voice->setPosition(0);
    voice->play();
}

void Audio::playBgm(BgmId id, bool loop) {
    if (!loaded_ || !state().bgm) {
        return;
    }
    if (!bgmEnabled_ || bgmVolumePercent_ <= 0) {
        state().bgm->stop();
        oneshotPlaying_ = false;
        return;
    }
    const QString file = bgmPath(id);
    if (file.isEmpty()) {
        return;
    }
    currentBgm_ = id;
    oneshotPlaying_ = !loop;
    state().bgm->setSource(Platform::mediaUrl(file));
    state().bgm->setLoops(loop ? QMediaPlayer::Infinite : 1);
    rebuildBgmVolume();
    state().bgm->play();
}

void Audio::startBgmLoop() {
    playBgm(BgmId::Explore, true);
}

void Audio::ensureBgmLoop() {
    if (!loaded_ || !bgmEnabled_ || bgmVolumePercent_ <= 0 || !state().bgm) {
        return;
    }
    // 一次性曲目（2 号 / 3 号）：在放就让它放完（notifyBgmEnded 会接回 1 号循环），
    // 之前被暂停过就唤醒，已经放完则清标志回落到 1 号循环
    if (oneshotPlaying_) {
        const QMediaPlayer::PlaybackState bgmState = state().bgm->playbackState();
        if (bgmState == QMediaPlayer::PlayingState) {
            return;
        }
        if (bgmState == QMediaPlayer::PausedState) {
            state().bgm->play();
            return;
        }
        oneshotPlaying_ = false;
        currentBgm_ = BgmId::Explore;
    }
    if (currentBgm_ == BgmId::Explore
        && state().bgm->playbackState() == QMediaPlayer::PlayingState) {
        return;
    }
    startBgmLoop();
}

// 达到回血条件：播一次 2 号曲，播完自动接回 1 号循环
void Audio::playRecoverBgm() {
    if (!loaded_ || !bgmEnabled_) {
        return;
    }
    if (oneshotPlaying_) {
        return;
    }
    playBgm(BgmId::Recover, false);
}

// 玩家拒绝结束本轮：切到 3 号曲播一次，播完自动接回 1 号循环
void Audio::playRefuseBgm() {
    if (!loaded_ || !bgmEnabled_) {
        return;
    }
    playBgm(BgmId::Refuse, false);
}

void Audio::stopBgm() {
    oneshotPlaying_ = false;
    if (state().bgm) {
        state().bgm->stop();
    }
}

void Audio::setBgmPaused(bool paused) {
    if (!state().bgm || !bgmEnabled_) {
        return;
    }
    if (paused) {
        state().bgm->pause();
    } else if (state().bgm->playbackState() == QMediaPlayer::PausedState) {
        state().bgm->play();
    } else if (state().bgm->playbackState() != QMediaPlayer::PlayingState) {
        if (oneshotPlaying_ && currentBgm_ != BgmId::Explore) {
            playBgm(currentBgm_, false);
        } else {
            startBgmLoop();
        }
    }
}

void Audio::notifyBgmEnded() {
    // 只有一次性曲目（2 号 / 3 号）放完才接回 1 号循环；换曲导致的旧媒体状态不处理
    if (!oneshotPlaying_ || !state().bgm
        || state().bgm->playbackState() != QMediaPlayer::StoppedState) {
        return;
    }
    oneshotPlaying_ = false;
    currentBgm_ = BgmId::Explore;
    startBgmLoop();
}

void Audio::applyFromSettings() {
    rebuildSfxVolumes();
    rebuildBgmVolume();
}
