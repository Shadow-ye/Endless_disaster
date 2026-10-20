#include "Storage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

namespace {
// settings.json 的进程内唯一真源。UI 回填、Audio 同步、落盘都读这一份：
// 拖动时先不写盘的话，界面回填仍能拿到最新值（以前每次回填都重读文件，
// 一旦落盘被推迟，滑块就会弹回旧位置、音量还会被旧值当场覆盖）。
AppSettings& settingsSnapshot() {
    static AppSettings settings;
    return settings;
}
bool& settingsSnapshotValid() {
    static bool valid = false;
    return valid;
}
bool& settingsPending() {
    static bool pending = false;
    return pending;
}
}  // namespace

QString Storage::dir() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path;
}

bool Storage::hasContinue() {
    return QFile::exists(dir() + "/continue.json");
}

bool Storage::saveContinue(const QJsonObject& game) {
    QSaveFile file(dir() + "/continue.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(game).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool Storage::loadContinue(QJsonObject& game) {
    QFile file(dir() + "/continue.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }
    game = doc.object();
    return true;
}

void Storage::deleteIfRun(uint32_t runId) {
    QJsonObject game;
    if (!loadContinue(game)) {
        return;
    }
    if (uint32_t(game.value("runId").toDouble()) != runId) {
        return;
    }
    QFile::remove(dir() + "/continue.json");
}

Records Storage::loadRecords() {
    Records records;
    QFile file(dir() + "/records.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return records;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    records.bestTime = float(obj.value("bestTime").toDouble());
    records.bestScore = obj.value("bestScore").toInt();
    return records;
}

void Storage::saveRecords(const Records& records) {
    QJsonObject obj;
    obj.insert("bestTime", records.bestTime);
    obj.insert("bestScore", records.bestScore);
    QSaveFile file(dir() + "/records.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.commit();
}

AppSettings Storage::loadSettings() {
    if (!settingsSnapshotValid()) {
        settingsSnapshot() = readSettingsFile();
        settingsSnapshotValid() = true;
        settingsPending() = false;
    }
    return settingsSnapshot();
}

void Storage::saveSettings(const AppSettings& settings) {
    settingsSnapshot() = settings;
    settingsSnapshotValid() = true;
    settingsPending() = false;
    writeSettingsFile(settings);
}

void Storage::stageSettings(const AppSettings& settings) {
    settingsSnapshot() = settings;
    settingsSnapshotValid() = true;
    settingsPending() = true;
}

void Storage::flushSettings() {
    if (!settingsPending()) {
        return;
    }
    settingsPending() = false;
    writeSettingsFile(settingsSnapshot());
}

AppSettings Storage::readSettingsFile() {
    AppSettings settings;
    QFile file(dir() + "/settings.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return settings;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    settings.sfxEnabled = obj.value("sfxEnabled").toBool(true);
    settings.sfxVolume = std::clamp(obj.value("sfxVolume").toInt(70), 0, 100);
    settings.bgmEnabled = obj.value("bgmEnabled").toBool(true);
    settings.bgmVolume = std::clamp(obj.value("bgmVolume").toInt(55), 0, 100);
    settings.autoAim = obj.value("autoAim").toBool(true);
    settings.autoAimDesktop = obj.value("autoAimDesktop").toBool(false);
    settings.guideAtStart = obj.value("guideAtStart").toBool(false);
    settings.showFps = obj.value("showFps").toBool(false);
    // 天气系统：旧设置里没有这一项，按默认打开
    settings.weather = obj.value("weather").toBool(true);
    settings.brightness = std::clamp(obj.value("brightness").toInt(100), 50, 150);
    return settings;
}

void Storage::writeSettingsFile(const AppSettings& settings) {
    QJsonObject obj;
    obj.insert("sfxEnabled", settings.sfxEnabled);
    obj.insert("sfxVolume", std::clamp(settings.sfxVolume, 0, 100));
    obj.insert("bgmEnabled", settings.bgmEnabled);
    obj.insert("bgmVolume", std::clamp(settings.bgmVolume, 0, 100));
    obj.insert("autoAim", settings.autoAim);
    obj.insert("autoAimDesktop", settings.autoAimDesktop);
    obj.insert("guideAtStart", settings.guideAtStart);
    obj.insert("showFps", settings.showFps);
    obj.insert("weather", settings.weather);
    // 亮度按 50~150 存，超范围一律夹回来（手改 settings.json 也读得回来）
    obj.insert("brightness", std::clamp(settings.brightness, 50, 150));
    QSaveFile file(dir() + "/settings.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.commit();
}

QSet<QString> Storage::loadAchievements() {
    QSet<QString> unlocked;
    QFile file(dir() + "/achievements.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return unlocked;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue& value : obj.value("unlocked").toArray()) {
        if (value.isString()) {
            unlocked.insert(value.toString());
        }
    }
    return unlocked;
}

void Storage::saveAchievements(const QSet<QString>& unlocked) {
    // 排序后写入：文件内容稳定，方便手改与对比
    QStringList ids(unlocked.begin(), unlocked.end());
    ids.sort();
    QJsonArray array;
    for (const QString& id : ids) {
        array.append(id);
    }
    QJsonObject obj;
    obj.insert("unlocked", array);
    QSaveFile file(dir() + "/achievements.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.commit();
}

bool Storage::unlockAchievement(const QString& id) {
    if (id.isEmpty()) {
        return false;
    }
    QSet<QString> unlocked = loadAchievements();
    if (unlocked.contains(id)) {
        return false;
    }
    unlocked.insert(id);
    saveAchievements(unlocked);
    return true;
}
