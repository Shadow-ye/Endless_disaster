#include "Storage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

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
    return settings;
}

void Storage::saveSettings(const AppSettings& settings) {
    QJsonObject obj;
    obj.insert("sfxEnabled", settings.sfxEnabled);
    obj.insert("sfxVolume", std::clamp(settings.sfxVolume, 0, 100));
    obj.insert("bgmEnabled", settings.bgmEnabled);
    obj.insert("bgmVolume", std::clamp(settings.bgmVolume, 0, 100));
    obj.insert("autoAim", settings.autoAim);
    QSaveFile file(dir() + "/settings.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.commit();
}
