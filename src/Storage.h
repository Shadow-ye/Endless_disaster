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

    // 进程内唯一真源：回填 UI、同步音频、落盘都读这一份（首次才真的读 settings.json）。
    // 这样「拖动中先不写盘」不会让界面读回旧值，也不会出现滑块显示与实际音量不一致。
    static AppSettings loadSettings();
    // 改完立刻写文件（复选框、松手、离开界面这些「确定」的时机）
    static void saveSettings(const AppSettings& settings);
    // 只更新进程内快照、暂不落盘：拖动中的实时预览走这条，安卓上不会因 fsync 卡手
    static void stageSettings(const AppSettings& settings);
    // 有暂未落盘的修改才真正写文件（各「离开界面 / 切后台」的兜底）
    static void flushSettings();

    // 成就：已解锁的 id 集合存在 achievements.json（id 定义见 Codex.h）
    static QSet<QString> loadAchievements();
    static void saveAchievements(const QSet<QString>& unlocked);
    // 解锁一个成就；此前尚未解锁时返回 true（用来决定要不要弹解锁提示）
    static bool unlockAchievement(const QString& id);

private:
    static QString dir();
    static AppSettings readSettingsFile();
    static void writeSettingsFile(const AppSettings& settings);
};
