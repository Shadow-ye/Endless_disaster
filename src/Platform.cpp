#include "Platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>

namespace {
QString firstAvailable(const QStringList& preferred) {
    const QStringList installed = QFontDatabase::families();
    for (const QString& family : preferred) {
        if (installed.contains(family, Qt::CaseInsensitive)) {
            return family;
        }
    }
    return {};
}
}  // namespace

QStringList Platform::dataRoots() {
    const QDir appDir(QCoreApplication::applicationDirPath());
    return {
        appDir.absolutePath(),
        QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("../share/endless-disaster"))),
    };
}

QString Platform::uiFontFamily() {
    static const QString family = [] {
        const QString found = firstAvailable({
            QStringLiteral("Microsoft YaHei UI"),
            QStringLiteral("Microsoft YaHei"),
            QStringLiteral("Noto Sans CJK SC"),
            QStringLiteral("Source Han Sans SC"),
            QStringLiteral("WenQuanYi Micro Hei"),
            QStringLiteral("WenQuanYi Zen Hei"),
            QStringLiteral("Droid Sans Fallback"),
        });
        return found.isEmpty() ? QGuiApplication::font().family() : found;
    }();
    return family;
}

QString Platform::titleFontFamily() {
    static const QString family = [] {
        const QString found = firstAvailable({
            QStringLiteral("KaiTi"),
            QStringLiteral("STKaiti"),
            QStringLiteral("AR PL UKai CN"),
            QStringLiteral("AR PL UKai"),
            QStringLiteral("Noto Serif CJK SC"),
        });
        return found.isEmpty() ? uiFontFamily() : found;
    }();
    return family;
}
