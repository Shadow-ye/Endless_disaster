#pragma once

#include <QJsonObject>
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

private:
    static QString dir();
};
