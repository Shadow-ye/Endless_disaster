#include "MainWindow.h"
#include "Platform.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>

namespace {
QString findGameIcon() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    for (const QString& root : Platform::dataRoots()) {
        candidates << root + QStringLiteral("/游戏图标.jpg")
                   << root + QStringLiteral("/assets/ui/游戏图标.jpg")
                   << root + QStringLiteral("/assets/ui/game_icon.jpg");
    }
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("../assets/ui/游戏图标.jpg"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../assets/ui/game_icon.jpg"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../build/游戏图标.jpg"));
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}
}  // namespace

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // FFmpeg 后端会在声道未就绪时反复初始化重采样。Windows 改走系统播放器，这条循环不会跑。
    qputenv("QT_MEDIA_BACKEND", "windows");
#endif
    Platform::configureDisplay();
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("EndlessDisaster");
    QCoreApplication::setApplicationName("EndlessDisaster");
    const QString iconPath = findGameIcon();
    if (!iconPath.isEmpty()) {
        app.setWindowIcon(QIcon(iconPath));
    }
    app.setStyleSheet(
        QStringLiteral("QWidget { background: #0c0a09; color: #cbbfae; font-family: '%1'; font-size: 15px; }")
            .arg(Platform::uiFontFamily()) +
        "QFrame#panel { background: #14110f; border: 1px solid #5c3a32; }"
        "QLabel { background: transparent; }"
        "QLabel#dim { color: #8a7b70; }"
        "QLabel#title { color: #e4d4c4; }"
        "QPushButton { background: #1a1513; color: #d7c7b4; border: 1px solid #3a2c28; border-left: 3px solid #7a342c; padding: 10px 16px; text-align: left; }"
        "QPushButton:hover { background: #2a1c18; color: #f2e6d8; border-left: 3px solid #c45c48; }"
        "QPushButton:disabled { color: #5c5148; border-left: 3px solid #3a302c; }"
        "QRadioButton { background: transparent; spacing: 8px; }"
        "QRadioButton::indicator { width: 12px; height: 12px; border: 1px solid #6a5048; background: #1a1412; border-radius: 6px; }"
        "QRadioButton::indicator:checked { background: #8a3a32; border: 1px solid #c45a48; }"
        "QCheckBox { background: transparent; spacing: 8px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #6a5048; background: #1a1412; }"
        "QCheckBox::indicator:checked { background: #8a3a32; border: 1px solid #c45a48; }"
        "QSlider::groove:horizontal { height: 6px; background: #1a1513; border: 1px solid #3a2c28; }"
        "QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #c45c48; border: 1px solid #7a342c; }"
        "QComboBox { background: #1a1513; color: #d7c7b4; border: 1px solid #3a2c28; padding: 6px 10px; }"
        "QComboBox QAbstractItemView { background: #14110f; color: #d7c7b4; selection-background-color: #3a2420; }");

    MainWindow window;
    if (!iconPath.isEmpty()) {
        window.setWindowIcon(QIcon(iconPath));
    }
#ifdef Q_OS_ANDROID
    window.showFullScreen();
#else
    window.show();
#endif
    return app.exec();
}