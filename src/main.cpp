#include "Cursors.h"
#include "MainWindow.h"
#include "Platform.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QIcon>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QWidget>

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

// 控件点击动效：按下方块按钮（QCheckBox）或滑动条时来一记短促的高亮脉冲，
// 颜色由全局样式表的 [_pulse=true] 属性选择器驱动，定时回落无需逐控件接线
class ClickPulse : public QObject {
public:
    explicit ClickPulse(QObject* parent) : QObject(parent) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::MouseButtonPress) {
            return QObject::eventFilter(watched, event);
        }
        if (qobject_cast<QCheckBox*>(watched) || qobject_cast<QSlider*>(watched)) {
            pulse(qobject_cast<QWidget*>(watched));
        }
        return QObject::eventFilter(watched, event);
    }

private:
    static void pulse(QWidget* widget) {
        if (!widget) {
            return;
        }
        widget->setProperty("_pulse", true);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        QTimer::singleShot(160, widget, [widget] {
            widget->setProperty("_pulse", false);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        });
    }
};
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
        "QCheckBox::indicator[_pulse=true] { background: #e8937e; border: 1px solid #f2c0ac; }"
        "QSlider::groove:horizontal { height: 6px; background: #1a1513; border: 1px solid #3a2c28; }"
        "QSlider::groove:horizontal[_pulse=true] { background: #2a1c18; border: 1px solid #c45c48; }"
        "QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #c45c48; border: 1px solid #7a342c; }"
        "QSlider::handle:horizontal:hover { background: #d8735c; border-color: #e8a08c; }"
        "QSlider::handle:horizontal:pressed { background: #f2e6d8; border-color: #c45c48; }"
        "QComboBox { background: #1a1513; color: #d7c7b4; border: 1px solid #3a2c28; padding: 6px 10px; }"
        "QComboBox QAbstractItemView { background: #14110f; color: #d7c7b4; selection-background-color: #3a2420; }");

    // 光标：先加载资源并装上全局替换规则（按钮手型 / 输入框文本光标）
    Cursors::init();
    // 控件点击动效：应用级过滤器覆盖设置页与暂停菜单里的方块按钮 / 滑动条
    ClickPulse clickPulse(&app);
    app.installEventFilter(&clickPulse);
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
    return app.exec();
}