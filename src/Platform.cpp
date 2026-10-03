#include "Platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>

#include <algorithm>
#endif

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
#ifdef Q_OS_ANDROID
    return {QStringLiteral(":")};
#else
    const QDir appDir(QCoreApplication::applicationDirPath());
    return {
        appDir.absolutePath(),
        QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("../share/endless-disaster"))),
    };
#endif
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

bool Platform::touchUi() {
#ifdef Q_OS_ANDROID
    return true;
#else
    static const bool forced = qEnvironmentVariableIntValue("ENDLESS_TOUCH_UI") != 0;
    return forced;
#endif
}

void Platform::configureDisplay() {
#ifdef Q_OS_ANDROID
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        return;
    }
    const QJniObject resources = QJniObject::callStaticObjectMethod(
        "android/content/res/Resources", "getSystem", "()Landroid/content/res/Resources;");
    if (!resources.isValid()) {
        return;
    }
    const QJniObject metrics = resources.callObjectMethod("getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
    if (!metrics.isValid()) {
        return;
    }
    const int widthPx = metrics.getField<jint>("widthPixels");
    const int heightPx = metrics.getField<jint>("heightPixels");
    const float density = metrics.getField<jfloat>("density");
    if (density <= 0.f) {
        return;
    }
    // 界面按桌面 960×540 设计，手机横屏通常只有 360~430dp 高。
    constexpr float kMinLogicalHeight = 560.f;
    const float shortSideDp = float(std::min(widthPx, heightPx)) / density;
    if (shortSideDp < kMinLogicalHeight) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(shortSideDp / kMinLogicalHeight, 'f', 3));
    }
#endif
}

QString Platform::assetDir() {
    for (const QString& root : dataRoots()) {
        if (QFile::exists(root + QStringLiteral("/assets/hero_warrior/idle.png"))) {
            return root + QStringLiteral("/assets");
        }
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFile::exists(dir.filePath(QStringLiteral("assets/hero_warrior/idle.png")))) {
            return dir.filePath(QStringLiteral("assets"));
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QCoreApplication::applicationDirPath() + QStringLiteral("/assets");
}

QUrl Platform::mediaUrl(const QString& path) {
    if (!path.startsWith(QLatin1String(":/"))) {
        return QUrl::fromLocalFile(path);
    }
    const QString target = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/media") + path.mid(1);
    const QFileInfo cached(target);
    if (!cached.exists() || cached.size() != QFileInfo(path).size()) {
        QDir().mkpath(cached.absolutePath());
        QFile::remove(target);
        if (!QFile::copy(path, target)) {
            return QUrl(QStringLiteral("qrc") + path);
        }
    }
    return QUrl::fromLocalFile(target);
}
