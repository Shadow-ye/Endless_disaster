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
    QMediaPlayer* continueVoice = nullptr;
    QAudioOutput* continueOut = nullptr;
    // 「继续前进」男女两版配音，按角色性别选一条
    QUrl continueMale;
    QUrl continueFemale;
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

// 配音文件名里的空格 / 下划线 / 连字符 / 大小写都可能改（I am atomic → I_am_atomic_man），
// 比较前统一抹平，免得改个名就静默找不到配音
QString clipKey(const QString& text) {
    QString out;
    out.reserve(text.size());
    for (const QChar& ch : text) {
        if (ch.isLetterOrNumber()) {
            out.append(ch.toLower());
        }
    }
    return out;
}

QString findClip(const QString& assetDir, const QString& token) {
    // 配音可能是 mp3 / m4a / mp4（音频轨）等，这里都要认
    const QStringList filters = {"*.wav", "*.mp3", "*.m4a", "*.mp4", "*.ogg", "*.flac"};
    const QString key = clipKey(token);
    if (key.isEmpty()) {
        return {};
    }
    QString best;
    QDateTime bestTime;
    for (const QString& path : bgmDirCandidates(assetDir)) {
        const QDir dir(QDir::cleanPath(path));
        if (!dir.exists()) {
            continue;
        }
        for (const QString& name : dir.entryList(filters, QDir::Files, QDir::Name)) {
            if (!clipKey(name).contains(key)) {
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
        // 文件当前叫 I_am_atomic_man.mp3，取 "atomic" 这个片段，改名换扩展名都还能对上
        const QString atomic = findClip(assetDir, QStringLiteral("atomic"));
        if (!atomic.isEmpty()) {
            state().atomicVoice->setSource(Platform::mediaUrl(atomic));
        }
        state().continueOut = new QAudioOutput(QCoreApplication::instance());
        state().continueVoice = new QMediaPlayer(QCoreApplication::instance());
        state().continueVoice->setAudioOutput(state().continueOut);
        state().continueVoice->setLoops(1);
        const QString continueMale = findClip(assetDir, QStringLiteral("继续前进_男"));
        if (!continueMale.isEmpty()) {
            state().continueMale = Platform::mediaUrl(continueMale);
        }
        const QString continueFemale = findClip(assetDir, QStringLiteral("继续前进_女"));
        if (!continueFemale.isEmpty()) {
            state().continueFemale = Platform::mediaUrl(continueFemale);
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
    // BgmId 的序号就是文件名里的序号（1 号 / 2 号 / 3 号…），加曲只要补一个枚举值
    const int number = int(id);
    const QStringList files = dir.entryList(QStringList{"*.m4a", "*.mp3", "*.wav", "*.ogg", "*.flac"},
        QDir::Files, QDir::Name);
    // 先按「N号」精确匹配，再退回只写数字的写法（数字后面不能跟数字，否则 1 会抢到 10号）
    for (int pass = 0; pass < 2; ++pass) {
        const QString key = pass == 0 ? QString::number(number) + QStringLiteral("号")
                                      : QString::number(number);
        for (const QString& name : files) {
            if (!name.startsWith(key)) {
                continue;
            }
            const QChar next = name.size() > key.size() ? name.at(key.size()) : QChar();
            if (pass == 0 || next.isNull() || !next.isDigit()) {
                return dir.absoluteFilePath(name);
            }
        }
    }
    // 回退：序号 N 对应排序后的第 N 个文件
    const int want = number - 1;
    if (want >= 0 && want < files.size()) {
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

void Audio::playContinueVoice(bool female) {
    QMediaPlayer* voice = state().continueVoice;
    if (!loaded_ || !sfxEnabled_ || sfxVolumePercent_ <= 0 || !voice) {
        return;
    }
    const QUrl url = female ? state().continueFemale : state().continueMale;
    if (url.isEmpty()) {
        return;
    }
    if (state().continueOut) {
        state().continueOut->setVolume(float(sfxVolumePercent_) / 100.f);
    }
    if (voice->source() == url) {
        voice->stop();
        voice->setPosition(0);
    } else {
        voice->setSource(url);
    }
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

// 新的一局：退出结算后的终曲状态，回到 1 号循环
void Audio::beginRun() {
    finaleMode_ = false;
    playBgm(BgmId::Explore, true);
}

void Audio::startBgmLoop() {
    playBgm(idleBgm(), true);
}

void Audio::ensureBgmLoop() {
    if (!loaded_ || !bgmEnabled_ || bgmVolumePercent_ <= 0 || !state().bgm) {
        return;
    }
    const BgmId idle = idleBgm();
    // 一次性曲目（2 / 3 / 4 号）：在放就让它放完（notifyBgmEnded 会接回循环曲），
    // 之前被暂停过就唤醒，已经放完则清标志回落
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
        currentBgm_ = idle;
    }
    // 循环曲正在放 / 被暂停：保持进度，别回主菜单就从头再来
    if (currentBgm_ == idle) {
        if (state().bgm->playbackState() == QMediaPlayer::PlayingState) {
            return;
        }
        if (state().bgm->playbackState() == QMediaPlayer::PausedState) {
            state().bgm->play();
            return;
        }
    }
    startBgmLoop();
}

// 达到回血条件：播一次 2 号曲，播完自动接回循环曲
void Audio::playRecoverBgm() {
    if (!loaded_ || !bgmEnabled_) {
        return;
    }
    if (oneshotPlaying_) {
        return;
    }
    playBgm(BgmId::Recover, false);
}

// 玩家拒绝结束本轮：切到 3 号曲播一次，播完自动接回循环曲
void Audio::playRefuseBgm() {
    if (!loaded_ || !bgmEnabled_) {
        return;
    }
    playBgm(BgmId::Refuse, false);
}

// 复活：切到 4 号曲播一次（复活是玩家明确的选择，直接打断在放的一次性曲目）
void Audio::playReviveBgm() {
    if (!loaded_ || !bgmEnabled_) {
        return;
    }
    playBgm(BgmId::Revive, false);
}

// 本局结算：切到 5 号曲循环，回主菜单不断，下一局由 beginRun() 换回 1 号
void Audio::playFinaleBgm() {
    finaleMode_ = true;
    if (!loaded_ || !bgmEnabled_) {
        return;
    }
    playBgm(BgmId::Finale, true);
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
        return;
    }
    if (state().bgm->playbackState() == QMediaPlayer::PausedState) {
        state().bgm->play();
        return;
    }
    ensureBgmLoop();
}

void Audio::notifyBgmEnded() {
    // 只有一次性曲目（2 / 3 / 4 号）放完才回落；换曲导致的旧媒体状态不处理
    if (!oneshotPlaying_ || !state().bgm
        || state().bgm->playbackState() != QMediaPlayer::StoppedState) {
        return;
    }
    oneshotPlaying_ = false;
    startBgmLoop();
}

void Audio::applyFromSettings() {
    rebuildSfxVolumes();
    rebuildBgmVolume();
}
