#pragma once

#include <QJsonObject>
#include <QSet>
#include <QString>

struct Records {
    float bestTime = 0.f;
    int bestScore = 0;
};

struct AppSettings {
    bool sfxEnabled = true;
    int sfxVolume = 70;
    bool bgmEnabled = true;
    int bgmVolume = 55;
    bool autoAim = true;
    bool autoAimDesktop = false;
    // 开局直接拥有天赋「世界指引」，不用先击杀 20 个入侵怪物
    bool guideAtStart = false;
    // 游戏内 HUD 顶部显示帧率 / 单帧耗时 / 实体数，默认关
    bool showFps = false;
    // 天气系统：局内周期性刮起六种天灾（默认开）
    bool weather = true;
    // 画面亮度（百分比，100 为默认）：只作用在游戏画面上，HUD 不受影响
    int brightness = 100;
};

class Storage {
public:
    static bool hasContinue();
    static bool saveContinue(const QJsonObject& game);
    static bool loadContinue(QJsonObject& game);
    static void deleteIfRun(uint32_t runId);

    static Records loadRecords();
    static void saveRecords(const Records& records);

    static AppSettings loadSettings();
    static void saveSettings(const AppSettings& settings);

    // 成就：已解锁的 id 集合存在 achievements.json（id 定义见 Codex.h）
    static QSet<QString> loadAchievements();
    static void saveAchievements(const QSet<QString>& unlocked);
    // 解锁一个成就；此前尚未解锁时返回 true（用来决定要不要弹解锁提示）
    static bool unlockAchievement(const QString& id);

private:
    static QString dir();
};
