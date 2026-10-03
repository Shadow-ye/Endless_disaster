#include "LoadingWidget.h"

#include "Platform.h"
#include "Types.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
// 加载页至少停留这么久，让扫描带与史莱姆动画完整可见
constexpr int kMinStayMs = 1000;
// 动画刷新间隔：与游戏主循环同频
constexpr int kFrameMs = 16;
// 扫描带自左向右的速度：每秒掠过一屏，快扫为主
constexpr float kScanScreensPerSecond = 1.0f;
// 进度条在视口高度上的位置
constexpr float kBarYRatio = 0.62f;
// 史莱姆素材的帧边长（与 SpriteSet 一致）
constexpr int kSlimeFramePx = 64;
// 史莱姆在屏幕上的显示高度：条首的大块头，走路动画要看得清
constexpr qreal kSlimeHeight = 240.0;
// 条首的蓝色史莱姆
const QColor kSlimeBlue(0x4F, 0xA6, 0xFF);
const QColor kGridDim(0x1A, 0x0B, 0x2E);
const QColor kBandLeft(0x2A, 0x0E, 0x4E);
const QColor kBandMid(0xA8, 0x55, 0xF7);
const QColor kBandRight(0xE9, 0xD5, 0xFF);

QColor blend(const QColor& a, const QColor& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return QColor(int(a.red() + (b.red() - a.red()) * t),
        int(a.green() + (b.green() - a.green()) * t),
        int(a.blue() + (b.blue() - a.blue()) * t));
}

qreal bandWidth(qreal viewW) {
    return std::max(140.0, viewW * 0.16);
}
}  // namespace

LoadingWidget::LoadingWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    // 加载页不接收任何输入：点击 / 触摸都不产生副作用
    setFocusPolicy(Qt::NoFocus);
    timer_ = new QTimer(this);
    timer_->setInterval(kFrameMs);
    connect(timer_, &QTimer::timeout, this, &LoadingWidget::tick);
}

void LoadingWidget::start() {
    const QString assets = Platform::assetDir();
    if (!slimeWalk_.ok()) {
        slimeWalk_.load(assets + "/slime/walk.png", kSlimeFramePx);
    }
    if (!slimeIdle_.ok()) {
        slimeIdle_.load(assets + "/slime/idle.png", kSlimeFramePx);
    }
    animT_ = 0.f;
    // 扫描带从屏幕左侧外进来
    scanX_ = float(-bandWidth(width()));
    target_ = 0.f;
    visual_ = 0.f;
    fadeT_ = 0.f;
    pulseT_ = 0.f;
    pulsePlayed_ = false;
    done_ = false;
    stageText_.clear();
    clock_.start();
    lastNs_ = 0;
    timer_->start();
    update();
}

void LoadingWidget::stop() {
    timer_->stop();
}

void LoadingWidget::setProgress(float value) {
    target_ = std::clamp(value, 0.f, 1.f);
}

void LoadingWidget::setStageText(const QString& text) {
    if (stageText_ == text) {
        return;
    }
    stageText_ = text;
    update();
}

void LoadingWidget::tick() {
    const qint64 now = clock_.nsecsElapsed();
    const float dt = std::clamp(float(now - lastNs_) / 1e9f, 0.f, 0.1f);
    lastNs_ = now;
    animT_ += dt;

    const qreal viewW = width();
    const qreal band = bandWidth(viewW);
    scanX_ += float(viewW * kScanScreensPerSecond) * dt;
    if (scanX_ > viewW) {
        // 扫出右边界后从左侧重新进入，形成无缝循环
        scanX_ = float(-band);
    }
    if (fadeT_ < 1.f) {
        fadeT_ = std::min(1.f, fadeT_ + dt / 0.15f);
    }
    if (pulseT_ > 0.f) {
        pulseT_ = std::max(0.f, pulseT_ - dt / 0.45f);
    }
    // 视觉进度按指数逼近目标：步子均匀，不会一帧跳满
    const float diff = target_ - visual_;
    visual_ = std::abs(diff) < 0.0008f ? target_ : std::clamp(visual_ + diff * std::min(1.f, dt * 6.f), 0.f, 1.f);

    if (!done_ && target_ >= 1.f && visual_ >= 1.f && clock_.elapsed() >= kMinStayMs) {
        if (pulseT_ > 0.f) {
            // 进度条紫色脉冲还在放，放完再收尾
        } else if (pulsePlayed_) {
            done_ = true;
            emit finished();
        } else {
            pulsePlayed_ = true;
            pulseT_ = 1.f;
        }
    }
    update();
}

void LoadingWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0, 0, 0));
    const bool touch = Platform::touchUi();
    const qreal scale = touch ? 1.1 : 1.0;
    painter.setOpacity(fadeT_);
    drawGrid(painter);
    drawProgress(painter, scale);
    painter.setOpacity(1.0);
}

// 底纹：稀疏的极暗紫方块（电路板感），被扫描带覆盖的格子点亮成紫色
void LoadingWidget::drawGrid(QPainter& painter) {
    const qreal viewW = width();
    const qreal viewH = height();
    if (viewW <= 1.0 || viewH <= 1.0) {
        return;
    }
    const qreal cell = std::max(16.0, viewW * 0.026);
    const qreal gap = std::max(2.5, cell * 0.16);
    const qreal band = bandWidth(viewW);
    const qreal bandEnd = scanX_ + band;

    painter.setPen(Qt::NoPen);
    const int cols = int(viewW / cell) + 1;
    const int rows = int(viewH / cell) + 1;
    for (int gy = 0; gy < rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            const QRectF box(gx * cell + gap * 0.5, gy * cell + gap * 0.5, cell - gap, cell - gap);
            const qreal cx = box.center().x();
            const uint32_t hash = mixHash(uint32_t(gx) * 0x9E3779B1u ^ uint32_t(gy) * 0x85EBCA6Bu);
            const bool inBand = cx >= scanX_ && cx <= bandEnd;
            if (!inBand && hash % 5u != 0u) {
                continue;
            }
            if (!inBand) {
                painter.setBrush(kGridDim);
                painter.drawRect(box);
                continue;
            }
            // 带内方块：横向亮度梯度 #2A0E4E → #A855F7 → #E9D5FF
            const float t = float((cx - scanX_) / band);
            const QColor color = t < 0.5f ? blend(kBandLeft, kBandMid, t * 2.f) : blend(kBandMid, kBandRight, (t - 0.5f) * 2.f);
            // 每个方块独立相位的轻微闪烁，让扫描带一直有细碎的动感
            const float phase = float(hash % 997u) / 997.f * 6.2831853f;
            const float flicker = 0.76f + 0.24f * std::sin(animT_ * 7.f + phase);
            QColor lit = color;
            lit.setAlphaF(std::clamp(0.9f * flicker, 0.f, 1.f));
            painter.setBrush(lit);
            painter.drawRect(box.adjusted(-1.0, -1.0, 1.0, 1.0));
        }
    }
}

void LoadingWidget::drawProgress(QPainter& painter, qreal scale) {
    const qreal viewW = width();
    const qreal viewH = height();
    // 窗口极窄时也要留出边距，别让进度条顶到屏幕外
    const qreal barW = std::min(std::clamp(viewW * 0.42, 220.0, 520.0), std::max(120.0, viewW - 48.0));
    // 加粗版轨道：厚度是原来的两倍
    const qreal barH = 12.0 * scale;
    const qreal barX = (viewW - barW) * 0.5;
    const qreal barY = viewH * kBarYRatio;
    const qreal radius = barH * 0.5;
    const qreal filled = barW * visual_;

    // 轨道
    QPainterPath track;
    track.addRoundedRect(QRectF(barX, barY, barW, barH), radius, radius);
    painter.setPen(Qt::NoPen);
    painter.fillPath(track, QColor(0x1B, 0x10, 0x30));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0x3A, 0x24, 0x61), 1.0));
    painter.drawPath(track);

    if (filled > 0.5) {
        QLinearGradient gradient(barX, 0.0, barX + barW, 0.0);
        gradient.setColorAt(0.0, QColor(0x7C, 0x2C, 0xCE));
        gradient.setColorAt(1.0, QColor(0x38, 0xBD, 0xF8));
        // 轨道下方的同色加宽柔光
        painter.setPen(Qt::NoPen);
        QPainterPath glow;
        glow.addRoundedRect(QRectF(barX, barY - barH * 0.35, filled, barH * 1.7),
            radius + barH * 0.35, radius + barH * 0.35);
        painter.fillPath(glow, QColor(0xA8, 0x55, 0xF7, 70));
        QPainterPath fill;
        fill.addRoundedRect(QRectF(barX, barY, filled, barH), radius, radius);
        painter.fillPath(fill, gradient);
    }

    // 进度走满后的紫色脉冲：整条光晕扩散一次
    if (pulseT_ > 0.f) {
        const qreal grow = qreal(1.f - pulseT_) * 8.0 * scale;
        QPainterPath pulse;
        pulse.addRoundedRect(QRectF(barX - grow * 0.5, barY - grow * 0.35, barW + grow, barH + grow * 0.7),
            radius + grow * 0.3, radius + grow * 0.3);
        painter.setPen(Qt::NoPen);
        painter.fillPath(pulse, QColor(0xA8, 0x55, 0xF7, int(90.f * pulseT_)));
    }

    // 条首的蓝色史莱姆：脚底踩在进度端点上，随进度往右挪；大块头也要留在画面内
    const qreal slimeHalf = kSlimeHeight * scale * 0.5;
    const qreal minX = slimeHalf + 8.0;
    const qreal maxX = std::max(minX, viewW - slimeHalf - 8.0);
    const qreal slimeX = std::clamp(barX + std::clamp(filled, 0.0, barW), minX, maxX);
    const qreal slimeY = barY + barH * 0.5 + 1.0 * scale;
    // 进度没走满时一直在走，走满后切成待机帧，像在终点站定
    drawSlime(painter, scale, slimeX, slimeY, visual_ < 1.f);

    // 阶段提示：进度条上方一行暗色小字
    if (!stageText_.isEmpty()) {
        painter.setFont(QFont(Platform::uiFontFamily(), int(14.0 * scale)));
        painter.setPen(QColor(0x8A, 0x7B, 0x70));
        // 文字留在史莱姆头顶之上；史莱姆太大时贴着画面顶部，至少不超出屏幕
        const qreal textY = std::max(8.0, barY - kSlimeHeight * scale - 34.0 * scale);
        painter.drawText(QRectF(0.0, textY, viewW, 22.0 * scale), Qt::AlignCenter, stageText_);
    }
}

void LoadingWidget::drawSlime(QPainter& painter, qreal scale, qreal x, qreal y, bool walking) {
    const SpriteAnim& anim = walking && slimeWalk_.ok() ? slimeWalk_ : slimeIdle_;
    if (!anim.ok()) {
        // 素材缺失时的兜底：画个蓝色圆球，保证条首不空
        painter.setBrush(kSlimeBlue);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, y - 9.0 * scale), 9.0 * scale, 9.0 * scale);
        return;
    }
    // 脚下淡紫软阴影
    // 阴影随体型一起放大，否则大个子史莱姆会像悬在空中
    const qreal slimeH = kSlimeHeight * scale;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(124, 44, 206, 48));
    painter.drawEllipse(QRectF(x - slimeH * 0.36, y - slimeH * 0.04, slimeH * 0.72, slimeH * 0.15));

    const int frames = std::max(1, anim.frames());
    const int frame = int(animT_ * 8.f) % frames;
    // 前进时上下浮动一点，像踩着进度条往前挪（幅度按体型走，小浮动在大家伙上看不出来）
    const qreal bob = walking ? std::sin(animT_ * 5.2) * slimeH * 0.04 : 0.0;
    anim.draw(painter, frame, float(x), float(y), false, float(kSlimeHeight * scale / kSlimeFramePx), float(bob),
        kSlimeBlue, 0, true);
}
