#include "Cursors.h"
#include "MainWindow.h"
#include "Platform.h"
#include "Storage.h"

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
        "QCheckBox::indicator:hover { border-color: #c45a48; }"
        "QCheckBox::indicator:checked { background: #8a3a32; border: 1px solid #c45a48; }"
        // 点击反馈走伪状态而不是动态属性：:pressed 由绘制时解析状态位得到，改状态只需重画这一小块；
        // 动态属性要配 unpolish/polish 才会重新匹配选择器，那会把整套样式规则重算一遍
        "QCheckBox::indicator:pressed { background: #e8937e; border: 1px solid #f2c0ac; }"
        "QSlider::groove:horizontal { height: 6px; background: #1a1513; border: 1px solid #3a2c28; }"
        "QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #c45c48; border: 1px solid #7a342c; }"
        "QSlider::handle:horizontal:hover { background: #d8735c; border-color: #e8a08c; }"
        "QSlider::handle:horizontal:pressed { background: #f2e6d8; border-color: #c45c48; }"
        "QComboBox { background: #1a1513; color: #d7c7b4; border: 1px solid #3a2c28; padding: 6px 10px; }"
        "QComboBox QAbstractItemView { background: #14110f; color: #d7c7b4; selection-background-color: #3a2420; }");

    // 光标：先加载资源并装上全局替换规则（按钮手型 / 输入框文本光标）
    Cursors::init();
    MainWindow window;
    // 顶层窗口定成游戏默认箭头，未单独设置光标的子控件照此继承
    window.setCursor(Cursors::arrow());
    if (!iconPath.isEmpty()) {
        window.setWindowIcon(QIcon(iconPath));
    }
#ifdef Q_OS_ANDROID
    window.showFullScreen();
#else
    window.show();
#endif
    // 切后台 / 失活前把手头没落盘的设置写掉：安卓上「退出界面」常常没有可靠的退出回调
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &app, [](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) {
            Storage::flushSettings();
        }
    });
    const int execCode = app.exec();
    // 桌面关窗 / 进程正常结束前补一次
    Storage::flushSettings();
    return execCode;
}