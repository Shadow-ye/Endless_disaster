#include "MenuTitle.h"

#include <QImage>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
// 展示页 style.css 里的动画参数：4.2 秒一次故障、6 秒（延迟 1.5 秒）一次闪烁、入场 0.9 秒（延迟 0.2 秒）
constexpr qreal kGlitchCycle = 4.2;
constexpr qreal kGlitchDelayCyan = 2.0;
constexpr qreal kGlitchDelayRed = 2.08;
constexpr qreal kFlickerCycle = 6.0;
constexpr qreal kFlickerDelay = 1.5;
constexpr qreal kEnterDelay = 0.2;
constexpr qreal kEnterTime = 0.9;
constexpr qreal kEnterOffset = 24.0;
// 辉光半径与立体投影按字号缩放，基准 120px
constexpr qreal kBasePixelSize = 120.0;
// 辉光层按这个比例缩小画好再放大回来，省下逐帧大半径模糊的开销，放大本身就是柔化
constexpr qreal kGlowScale = 0.3;
// 左右给辉光与故障错位留的余量（sizeHint 用）
constexpr qreal kSidePadding = 30.0;
// 上下留白（每侧）：容纳辉光与立体投影
constexpr qreal kHeightPadding = 8.0;

qreal wrapPhase(qreal value, qreal cycle) {
    const qreal t = std::fmod(value, cycle);
    return t < 0.0 ? t + cycle : t;
}

qreal lerp(qreal a, qreal b, qreal u) {
    return a + (b - a) * u;
}
}  // namespace

MenuTitle::MenuTitle(QWidget* parent) : QWidget(parent) {
    // 原来这里是个 QLabel：不吃鼠标事件、背景透明
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    // 全局 QSS 给 QWidget 铺了底色，这里显式透明，免得在背景图上盖一块黑
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("background: transparent;"));
    font_.setPixelSize(int(kBasePixelSize));
    font_.setBold(true);
    font_.setLetterSpacing(QFont::AbsoluteSpacing, 8.0);
    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, [this] {
        const qint64 ns = clock_.nsecsElapsed();
        clock_.restart();
        elapsed_ += qreal(ns) / 1e9;
        update();
    });
}

void MenuTitle::setText(const QString& text) {
    if (text_ == text) {
        return;
    }
    text_ = text;
    cache_ = QPixmap();
}

void MenuTitle::setTitleFont(const QFont& font) {
    font_ = font;
    if (!font_.bold()) {
        font_.setBold(true);
    }
    cache_ = QPixmap();
    update();
}

void MenuTitle::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // 切回主菜单时重新走一遍入场动画
    elapsed_ = 0.0;
    clock_.restart();
    timer_->start();
}

void MenuTitle::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // 切到设置 / 出发页就停表，别在后台白转
    timer_->stop();
}

QSize MenuTitle::sizeHint() const {
    const QFontMetricsF fm(font_);
    const int side = int(std::ceil(kSidePadding));
    // 高度按墨迹算：QFontMetrics::height() 含大量 leading，照它留白会让标题与副标题之间空得离谱
    return QSize(int(std::ceil(fm.horizontalAdvance(text_))) + side * 2,
        int(std::ceil(fm.tightBoundingRect(text_).height())) + 2 * int(std::ceil(kHeightPadding)));
}

QSize MenuTitle::minimumSizeHint() const {
    // 不挡住窗口变窄时的压缩（真挤不下时文字会被裁，但不会把主菜单布局撑破）
    return QSize(80, sizeHint().height());
}

// 文字实际占的那条横带：竖向居中，高度用字体的 ascent + descent 近似
QRectF MenuTitle::textBox() const {
    const QFontMetricsF fm(font_);
    const qreal lineH = fm.ascent() + fm.descent();
    return QRectF(0.0, (height() - lineH) / 2.0, width(), lineH);
}

void MenuTitle::rebuild() {
    cacheSize_ = size();
    cacheFont_ = font_;
    if (text_.isEmpty() || width() <= 0 || height() <= 0) {
        cache_ = QPixmap();
        return;
    }

    const qreal ratio = font_.pixelSize() > 0 ? qreal(font_.pixelSize()) / kBasePixelSize : 1.0;
    // 辉光不能超出控件边界，否则会被切平；按上下留白收着来
    const QFontMetricsF metrics(font_);
    const qreal marginY = std::max(0.0, (height() - (metrics.ascent() + metrics.descent())) / 2.0);
    const qreal glowLimit = std::max(4.0, marginY - 3.0);
    const qreal glowFar = std::min(13.0 * ratio, glowLimit);
    const qreal glowNear = std::min(5.0 * ratio, glowLimit * 0.6);
    const qreal dpr = devicePixelRatioF();
    cache_ = QPixmap(QSize(std::max(1, int(std::ceil(width() * dpr))), std::max(1, int(std::ceil(height() * dpr)))));
    cache_.setDevicePixelRatio(dpr);
    cache_.fill(Qt::transparent);

    // drawText 居中用的是排版宽度，跟眼睛看到的墨迹边界差着一对左右留白；
    // 这里把差值补掉，标题的左右两端才能和副标题对齐
    {
        const QFontMetricsF fm(font_);
        const QRectF ink = fm.tightBoundingRect(text_);
        inkShiftX_ = fm.horizontalAdvance(text_) / 2.0 - (ink.left() + ink.width() / 2.0);
    }

    QPainter painter(&cache_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF box(0.0, 0.0, width(), height());

    // 红色辉光：对应 CSS 的 text-shadow 0 0 18px / 0 0 60px。
    // 在缩小的图上按多个方向偏移叠画红色文字，再平滑放大回来，得到柔和的连续光晕
    const QSize glowSize(std::max(8, int(std::ceil(width() * kGlowScale))),
        std::max(8, int(std::ceil(height() * kGlowScale))));
    QImage glow(glowSize, QImage::Format_ARGB32_Premultiplied);
    glow.fill(Qt::transparent);
    {
        QPainter glowPainter(&glow);
        glowPainter.setRenderHint(QPainter::Antialiasing, true);
        glowPainter.scale(kGlowScale, kGlowScale);
        glowPainter.translate(inkShiftX_, 0.0);
        glowPainter.setFont(font_);
        struct Ring {
            qreal radius;
            int steps;
            int alpha;
        };
        const Ring rings[] = {{glowFar, 10, 30}, {glowNear, 8, 55}};
        for (const Ring& ring : rings) {
            for (int i = 0; i < ring.steps; ++i) {
                const qreal angle = 2.0 * M_PI * qreal(i) / qreal(ring.steps);
                glowPainter.setPen(QColor(255, 75, 62, ring.alpha));
                glowPainter.save();
                glowPainter.translate(std::cos(angle) * ring.radius, std::sin(angle) * ring.radius);
                glowPainter.drawText(box, Qt::AlignCenter, text_);
                glowPainter.restore();
            }
        }
        glowPainter.setPen(QColor(255, 75, 62, 90));
        glowPainter.drawText(box, Qt::AlignCenter, text_);
    }
    painter.drawImage(QRectF(0.0, 0.0, width(), height()), glow);

    painter.setFont(font_);
    painter.translate(inkShiftX_, 0.0);
    // 立体投影：0 6px 0 #5a0f0a
    painter.save();
    painter.translate(0.0, 6.0 * ratio);
    painter.setPen(QColor(0x5a, 0x0f, 0x0a));
    painter.drawText(box, Qt::AlignCenter, text_);
    painter.restore();
    // 主体白字
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(box, Qt::AlignCenter, text_);
}

MenuTitle::GlitchFrame MenuTitle::glitchAt(qreal phase) {
    const qreal p = phase / kGlitchCycle;
    // CSS 用的是 steps(1)：关键帧之间直接跳变
    if (p < 0.88 || p >= 0.95) {
        return {};
    }
    if (p < 0.89) {
        return {true, -6.0, 2.0, 0.8, 0.12, 0.58};
    }
    if (p < 0.91) {
        return {true, 5.0, -2.0, 0.8, 0.60, 0.12};
    }
    if (p < 0.93) {
        return {true, -3.0, 1.0, 0.8, 0.35, 0.38};
    }
    return {};
}

qreal MenuTitle::flicker() const {
    // CSS：0%, 92%, 100% = 1；93% = .55；94% = 1；96% = .7；97% = 1
    const qreal p = wrapPhase(elapsed_ - kFlickerDelay, kFlickerCycle) / kFlickerCycle;
    if (p < 0.92) {
        return 1.0;
    }
    if (p < 0.93) {
        return lerp(1.0, 0.55, (p - 0.92) / 0.01);
    }
    if (p < 0.94) {
        return lerp(0.55, 1.0, (p - 0.93) / 0.01);
    }
    if (p < 0.96) {
        return 1.0;
    }
    if (p < 0.97) {
        return lerp(1.0, 0.7, (p - 0.96) / 0.01);
    }
    return lerp(0.7, 1.0, (p - 0.97) / 0.03);
}

void MenuTitle::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (cache_.isNull() || cacheSize_ != size() || cacheFont_ != font_) {
        rebuild();
    }
    if (cache_.isNull()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 入场：0.2 秒后开始，0.9 秒内从下方 24px 淡入（展示页的 fadeUp）
    qreal enter = 1.0;
    qreal enterDy = 0.0;
    if (elapsed_ < kEnterDelay + kEnterTime) {
        const qreal t = std::clamp((elapsed_ - kEnterDelay) / kEnterTime, 0.0, 1.0);
        const qreal ease = 1.0 - (1.0 - t) * (1.0 - t);
        enter = ease;
        enterDy = kEnterOffset * (1.0 - ease);
    }

    const qreal alpha = flicker() * enter;
    painter.translate(0.0, enterDy);
    painter.setOpacity(alpha);
    painter.drawPixmap(0, 0, cache_);

    // 故障层：青 / 红各复制一份文字，只显示一条横带并整体错位
    const QRectF strip = textBox();
    const QRectF box(0.0, 0.0, width(), height());
    painter.setFont(font_);
    const auto drawGhost = [&](qreal phase, const QColor& color) {
        const GlitchFrame frame = glitchAt(wrapPhase(phase, kGlitchCycle));
        if (!frame.visible) {
            return;
        }
        painter.save();
        painter.translate(frame.dx + inkShiftX_, frame.dy);
        painter.setClipRect(QRectF(strip.left(), strip.top() + strip.height() * frame.clipTop, strip.width(),
                               strip.height() * (1.0 - frame.clipTop - frame.clipBottom)),
            Qt::ReplaceClip);
        painter.setOpacity(alpha * frame.alpha);
        painter.setPen(color);
        painter.drawText(box, Qt::AlignCenter, text_);
        painter.restore();
    };
    drawGhost(elapsed_ - kGlitchDelayCyan, QColor(0x5e, 0xea, 0xd4));
    drawGhost(elapsed_ - kGlitchDelayRed, QColor(0xff, 0x4b, 0x3e));
}
