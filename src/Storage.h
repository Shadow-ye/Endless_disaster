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
