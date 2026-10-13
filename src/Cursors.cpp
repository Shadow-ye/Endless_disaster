#include "Cursors.h"

#include "Platform.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QScreen>
#include <QTextEdit>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {
// 光标按 32 逻辑像素显示：乘上屏幕缩放比，高清屏上不至于糊成一团
constexpr int kCursorLogicalSize = 32;

int cursorPixelSize() {
    qreal dpr = 1.0;
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        dpr = screen->devicePixelRatio();
    }
    return std::max(16, int(std::lround(kCursorLogicalSize * dpr)));
}

// .cur 与 .ico 同源：文件头里每个图标的 wPlanes / wBitCount 两个字段在光标格式里被复用成热点坐标。
// Qt 的 ico 插件只给像素不给热点，这里自己读一次文件头补上。
bool readCurHotspot(const QString& path, int* hotX, int* hotY) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray head = file.read(6 + 16);
    if (head.size() < 22) {
        return false;
    }
    const uchar* d = reinterpret_cast<const uchar*>(head.constData());
    const auto u16 = [d](int offset) { return int(d[offset]) | (int(d[offset + 1]) << 8); };
    if (u16(2) != 2) {  // 文件类型：2 = 光标（1 才是普通图标）
        return false;
    }
    *hotX = u16(10);
    *hotY = u16(12);
    return true;
}

QCursor loadCursor(const QString& file) {
    const QString path = Platform::assetDir() + QStringLiteral("/cursor/") + file;
    const QImage image = QImageReader(path).read();
    if (image.isNull()) {
        return QCursor();  // 默认构造就是系统箭头：资源缺失时至少还有一个能用的指针
    }
    QPoint hotspot(image.width() / 2, image.height() / 2);
    int hotX = 0;
    int hotY = 0;
    if (readCurHotspot(path, &hotX, &hotY)) {
        hotspot = QPoint(hotX, hotY);
    }
    const int size = cursorPixelSize();
    const QPixmap pixmap = QPixmap::fromImage(image).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // 热点要跟着缩放一起换算，否则指针会"点偏"
    const QPoint scaled(qRound(qreal(hotspot.x()) * pixmap.width() / image.width()),
        qRound(qreal(hotspot.y()) * pixmap.height() / image.height()));
    return QCursor(pixmap, scaled.x(), scaled.y());
}

// 全局替换规则：按钮悬停给手型、输入框给文本光标、独立顶层窗口给默认箭头。
// 用事件过滤器而不是逐个 setCursor，是为了让运行时才建出来的面板 / 弹窗也自动生效。
class CursorFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (!widget) {
            return QObject::eventFilter(watched, event);
        }
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            if (qobject_cast<QAbstractButton*>(widget) || qobject_cast<QComboBox*>(widget)) {
                widget->setCursor(Cursors::hand());
            } else if (isTextInput(widget)) {
                widget->setCursor(Cursors::text());
            } else if (insideScrollArea(widget)) {
                // 图鉴 / 操作说明这类滚动阅读区继承自游戏画面的准星会很怪，换回箭头
                widget->setCursor(Cursors::arrow());
            }
            break;
        case QEvent::Show:
            // 弹窗（QMessageBox 之类）是独立顶层窗口，不会继承主窗口的光标
            if (widget->isWindow() && !widget->testAttribute(Qt::WA_SetCursor)) {
                widget->setCursor(Cursors::arrow());
            }
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    static bool isTextInput(QWidget* widget) {
        return qobject_cast<QLineEdit*>(widget) || qobject_cast<QTextEdit*>(widget)
            || qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QAbstractSpinBox*>(widget);
    }

    // 滚动区本身和它的 viewport 都算阅读区（viewport 是独立控件，光标从父级继承）
    static bool insideScrollArea(QWidget* widget) {
        return qobject_cast<QAbstractScrollArea*>(widget) != nullptr
            || qobject_cast<QAbstractScrollArea*>(widget->parentWidget()) != nullptr;
    }
};
}  // namespace

void Cursors::init() {
#ifdef Q_OS_ANDROID
    return;  // 触屏没有鼠标指针，没必要解析这套图
#else
    static bool installed = false;
    if (installed) {
        return;
    }
    installed = true;
    // 先全部加载一遍：每张 .cur 有 66KB，第一次解码若落在悬停那一刻会有明显顿挫
    arrow();
    hand();
    text();
    crosshair();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(new CursorFilter(QCoreApplication::instance()));
    }
#endif
}

// 四个光标各加载一次后缓存。这里故意用不析构的堆对象：
// 静态 QCursor 会晚于 QApplication 析构，退出时容易踩到已经清掉的平台对象。
QCursor Cursors::arrow() {
    static const QCursor* cursor = new QCursor(loadCursor(QStringLiteral("pointer.cur")));
    return *cursor;
}

QCursor Cursors::hand() {
    static const QCursor* cursor = new QCursor(loadCursor(QStringLiteral("link.cur")));
    return *cursor;
}

QCursor Cursors::text() {
    static const QCursor* cursor = new QCursor(loadCursor(QStringLiteral("text.cur")));
    return *cursor;
}

QCursor Cursors::crosshair() {
    static const QCursor* cursor = new QCursor(loadCursor(QStringLiteral("precision.cur")));
    return *cursor;
}
