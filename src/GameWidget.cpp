#include "GameWidget.h"

#include "Audio.h"
#include "Codex.h"
#include "Platform.h"
#include "Storage.h"
#include "TileMap.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPointingDevice>
#include <QPushButton>
#include <QRadialGradient>
#include <QScrollArea>
#include <QScroller>
#include <QRandomGenerator>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QTouchEvent>
#include <QVector>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
constexpr size_t kMaxParticles = 560;
// 右上角雷达盘的边长，以及靠它定位的迷宫地图边长
constexpr int kRadarSide = 112;
// I am atomic 的白闪：比 Session 里的闪白那一拍长得多，让爆炸在强光没消时就涌出来
constexpr float kAtomicFlashFade = 0.64f;
constexpr int kMazeMapSide = 132;

QColor fxColor(const AttackFx& fx, const QColor& fallback) {
    return fx.color == 0 ? fallback : QColor::fromRgb(QRgb(0xFF000000u | fx.color));
}

QColor withAlpha(QColor color, int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

// QPainter 没有 shadowBlur：先叠两层加宽的半透明描边充当辉光，再画实线
template <typename Draw>
void strokeGlow(QPainter& painter, const QColor& color, float width, Draw draw) {
    const int alpha = color.alpha();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(withAlpha(color, alpha / 6), width * 3.4f, Qt::SolidLine, Qt::RoundCap));
    draw();
    painter.setPen(QPen(withAlpha(color, alpha * 2 / 5), width * 2.f, Qt::SolidLine, Qt::RoundCap));
    draw();
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
    draw();
}

// 屏幕角度（y 向下）转成 Qt 圆弧用的角度（逆时针为正）
QPainterPath arcPath(float cx, float cy, float r, float from, float to) {
    const QRectF box(cx - r, cy - r, r * 2.f, r * 2.f);
    QPainterPath path;
    path.arcMoveTo(box, -from * 180.f / kPi);
    path.arcTo(box, -from * 180.f / kPi, -(to - from) * 180.f / kPi);
    return path;
}

// 技能释放瞬间的朝向，取特效里记录的方向并归一化
void fxDir(const AttackFx& fx, float& nx, float& ny) {
    const float d = std::sqrt(fx.fx * fx.fx + fx.fy * fx.fy);
    nx = d > 0.001f ? fx.fx / d : 1.f;
    ny = d > 0.001f ? fx.fy / d : 0.f;
}

QString formatTime(float t) {
    if (t < 0.f) {
        t = 0.f;
    }
    const int total = int(t);
    const int minutes = total / 60;
    const int seconds = total % 60;
    const int tenth = int((t - float(total)) * 10.f);
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(tenth);
}

QString findAssets() {
    for (const QString& root : Platform::dataRoots()) {
        if (QFile::exists(root + "/assets/hero_warrior/idle.png")) {
            return root + "/assets";
        }
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFile::exists(dir.filePath("assets/hero_warrior/idle.png"))) {
            return dir.filePath("assets");
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QCoreApplication::applicationDirPath() + "/assets";
}

float skillCooldownMax(int skill, float mul) {
    switch (skill) {
    case kSkillSpin:
        return 2.8f * mul;
    case kSkillMageBolt:
        return 1.6f * mul;
    case kSkillNova:
        return 4.2f * mul;
    case kSkillBurial:
        return 9.f * mul;
    case kSkillMirror:
        return 15.f * mul;
    case kSkillMageHeal:
        return 20.f * mul;
    case kSkillBerserk:
        return 8.f * mul;
    case kSkillOverload:
        return 20.f * mul;
    case kSkillMagField:
        return 10.f * mul;
    case kSkillMedkit:
        return 16.f * mul;
    case kSkillScatter:
        return 2.4f * mul;
    case kSkillMissile:
        return 4.5f * mul;
    case kSkillBoost:
        return 3.f * mul;
    case kSkillSwarm:
        return 7.f * mul;
    case kSkillMelee:
        return kMeleeCooldown * mul;
    case kSkillSwordQi:
        return 1.5f * mul;
    case kSkillThrust:
        return 1.0f * mul;
    case kSkillAtomic:
        return kAtomicCdMax * mul;
    default:
        return 0.f;
    }
}

// I am atomic 的冷却不是定值：烧掉的 MP 越多越短（kAtomicCdMin ~ kAtomicCdMax）。
// 冷却环要用「这一发实际定下的冷却」当分母才准，lastCd 就是释放时记下的那个值；
// 没有记录（还没放过，或读档进来的）时退回最坏值 kAtomicCdMax。
float skillCooldownMax(int skill, float mul, float lastCd) {
    if (skill == kSkillAtomic && lastCd > 0.01f) {
        return lastCd;
    }
    return skillCooldownMax(skill, mul);
}

QColor tileColor(Tile tile) {
    switch (tile) {
    case Tile::Dirt:
        return QColor(196, 154, 86);
    case Tile::Water:
        return QColor(48, 112, 186);
    case Tile::Rock:
        return QColor(96, 98, 102);
    case Tile::Bush:
        return QColor(46, 120, 52);
    case Tile::Grass:
        return QColor(86, 158, 62);
    case Tile::MazeWall:
        return QColor(42, 36, 40);
    case Tile::MazeFloor:
        return QColor(70, 60, 54);
    case Tile::Plaza:
        return QColor(92, 42, 40);
    }
    return QColor(86, 158, 62);
}

void drawGround(QPainter& painter, const QImage& tiles, bool hasTiles, Tile tile, int x, int y, const TileMap& map) {
    const QRect dest(x * kTile, y * kTile, kTile, kTile);
    if (tile == Tile::Water) {
        painter.fillRect(dest, QColor(22, 36, 44));
        painter.setPen(QColor(8, 16, 22));
        if (!map.blocks(x - 1, y, 0) || map.at(x - 1, y) != Tile::Water) {
            painter.drawLine(dest.topLeft(), dest.bottomLeft());
        }
        if (map.at(x, y - 1) != Tile::Water) {
            painter.drawLine(dest.topLeft(), dest.topRight());
        }
        if (hasTiles && map.at(x - 1, y) != Tile::Water && map.at(x, y - 1) != Tile::Water) {
            painter.drawImage(QRect(dest.x() - 4, dest.y() - 8, 40, 36), tiles, QRect(0, 48, 48, 40));
        }
        return;
    }
    if (tile == Tile::Dirt) {
        painter.fillRect(dest, QColor(62, 48, 36));
        if (hasTiles) {
            painter.drawImage(dest, tiles, QRect(128, 16, 16, 16));
        }
        return;
    }
    painter.fillRect(dest, QColor(42, 50, 32));
    if (hasTiles) {
        painter.drawImage(dest, tiles, QRect(80, 16, 16, 16));
        if (tile == Tile::Grass && (mixHash(uint32_t(x) * 17u ^ uint32_t(y)) % 11u) == 0u) {
            painter.drawImage(dest, tiles, QRect(16, 0, 16, 16));
        }
    }
}

constexpr float kGuardR = 22.05f;
constexpr float kGuardCenterAboveFoot = 13.5f;

void drawFacingMarker(QPainter& painter, float cx, float cy, float fx, float fy, float radius, const QColor& color) {
    const float d = std::sqrt(fx * fx + fy * fy);
    if (d < 0.001f) {
        return;
    }
    fx /= d;
    fy /= d;
    // 底边贴在护盾圆周上，尖端沿朝向伸出
    const float halfAngle = 0.36f;
    const float tipExtra = 8.f;
    const float ang = std::atan2(fy, fx);
    const QPointF tip(cx + std::cos(ang) * (radius + tipExtra), cy + std::sin(ang) * (radius + tipExtra));
    const QPointF left(cx + std::cos(ang + halfAngle) * radius, cy + std::sin(ang + halfAngle) * radius);
    const QPointF right(cx + std::cos(ang - halfAngle) * radius, cy + std::sin(ang - halfAngle) * radius);
    const float rim = radius / std::max(0.2f, std::cos(halfAngle));
    const QPointF arc(cx + std::cos(ang) * rim, cy + std::sin(ang) * rim);
    QPainterPath path;
    path.moveTo(tip);
    path.lineTo(left);
    path.quadTo(arc, right);
    path.closeSubpath();
    painter.setPen(QPen(QColor(40, 8, 8, 220), 1));
    painter.setBrush(color);
    painter.drawPath(path);
}

QColor attackMarkerColor(const Player& player) {
    if (player.state == ActorState::Attack) {
        if (player.heavy) {
            return QColor(255, 96, 24, 230);  // 重击：橙红
        }
        return QColor(255, 210, 48, 230);  // 轻击：金黄
    }
    if (player.heavyCharge > 0.12f) {
        const float t = std::clamp(player.heavyCharge / 0.42f, 0.f, 1.f);
        return QColor(255, int(80 + 100 * t), int(40 + 80 * t), 230);  // 蓄力渐变
    }
    return QColor(220, 48, 42, 210);  // 默认：红
}
}  // namespace

int GameWidget::frameIndex(const SpriteAnim& anim, float time, bool loop, float fps) {
    if (!anim.ok()) {
        return 0;
    }
    const int count = anim.frames();
    int frame = int(time * fps);
    if (loop) {
        frame %= count;
    } else {
        frame = std::min(frame, count - 1);
    }
    return std::max(0, frame);
}

GameWidget::GameWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    // paintEvent 会铺满整个控件；不声明的话每帧都要先重画父窗口背景
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    touchUi_ = Platform::touchUi();
    if (!touchUi_) {
        setMinimumSize(960, 540);
    }
    spritesOk_ = sprites_.load(findAssets());
    canvas_ = QImage(kViewW, kViewH, QImage::Format_RGB32);
    vignette_ = QImage(kViewW, kViewH, QImage::Format_ARGB32_Premultiplied);
    vignette_.fill(Qt::transparent);
    {
        QPainter vig(&vignette_);
        QRadialGradient gradient(kViewW * 0.5, kViewH * 0.5, kViewW * 0.72);
        gradient.setColorAt(0.42, QColor(0, 0, 0, 0));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 150));
        vig.fillRect(vignette_.rect(), gradient);
    }
    {
        const AppSettings settings = Storage::loadSettings();
        Audio::instance().load(findAssets());
        Audio::instance().setSfxEnabled(settings.sfxEnabled);
        Audio::instance().setSfxVolume(settings.sfxVolume);
        Audio::instance().setBgmEnabled(settings.bgmEnabled);
        Audio::instance().setBgmVolume(settings.bgmVolume);
#if (defined(Q_OS_WIN) || defined(Q_OS_LINUX)) && !defined(Q_OS_ANDROID)
        autoAim_ = touchUi_ ? settings.autoAim : settings.autoAimDesktop;
#else
        autoAim_ = settings.autoAim;
#endif
    }

    pausePanel_ = new QWidget(this);
    // 手机横屏高度不够，技能说明放到右栏
    auto* pauseRoot = new QBoxLayout(touchUi_ ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom, pausePanel_);
    pauseRoot->setContentsMargins(18, 18, 18, 18);
    if (touchUi_) {
        pauseRoot->setSpacing(18);
    }
    auto* pauseLayout = new QVBoxLayout();
    pauseLayout->setContentsMargins(0, 0, 0, 0);
    pauseRoot->addLayout(pauseLayout);
    auto* pauseTitle = new QLabel("暂停", pausePanel_);
    pauseTitle->setAlignment(Qt::AlignCenter);
    pauseLayout->addWidget(pauseTitle);
    auto addButton = [this](QWidget* panel, QVBoxLayout* layout, const QString& text, auto slot) {
        auto* button = new QPushButton(text, panel);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, slot);
    };
    addButton(pausePanel_, pauseLayout, "继续", [this] { setPaused(false); setFocus(); });
    addButton(pausePanel_, pauseLayout, "存档", [this] { saveGame(); });
    addButton(pausePanel_, pauseLayout, "主动结算", [this] { askSettle(); });
    addButton(pausePanel_, pauseLayout, "退出", [this] { leaveToMenu(); });

    auto* sfxRow = new QWidget(pausePanel_);
    auto* sfxLayout = new QHBoxLayout(sfxRow);
    sfxLayout->setContentsMargins(0, 8, 0, 0);
    pauseSfxCheck_ = new QCheckBox("音效", sfxRow);
    pauseSfxSlider_ = new QSlider(Qt::Horizontal, sfxRow);
    pauseSfxSlider_->setRange(0, 100);
    pauseSfxValue_ = new QLabel(sfxRow);
    pauseSfxValue_->setMinimumWidth(36);
    sfxLayout->addWidget(pauseSfxCheck_);
    sfxLayout->addWidget(pauseSfxSlider_, 1);
    sfxLayout->addWidget(pauseSfxValue_);
    pauseLayout->addWidget(sfxRow);

    auto* bgmRow = new QWidget(pausePanel_);
    auto* bgmLayout = new QHBoxLayout(bgmRow);
    bgmLayout->setContentsMargins(0, 4, 0, 0);
    pauseBgmCheck_ = new QCheckBox("BGM", bgmRow);
    pauseBgmSlider_ = new QSlider(Qt::Horizontal, bgmRow);
    pauseBgmSlider_->setRange(0, 100);
    pauseBgmValue_ = new QLabel(bgmRow);
    pauseBgmValue_->setMinimumWidth(36);
    bgmLayout->addWidget(pauseBgmCheck_);
    bgmLayout->addWidget(pauseBgmSlider_, 1);
    bgmLayout->addWidget(pauseBgmValue_);
    pauseLayout->addWidget(bgmRow);

    auto syncPauseAudioUi = [this](const AppSettings& settings) {
        pauseSfxCheck_->setChecked(settings.sfxEnabled);
        pauseSfxSlider_->setValue(settings.sfxVolume);
        pauseSfxSlider_->setEnabled(settings.sfxEnabled);
        pauseSfxValue_->setText(QString("%1%").arg(settings.sfxVolume));
        pauseBgmCheck_->setChecked(settings.bgmEnabled);
        pauseBgmSlider_->setValue(settings.bgmVolume);
        pauseBgmSlider_->setEnabled(settings.bgmEnabled);
        pauseBgmValue_->setText(QString("%1%").arg(settings.bgmVolume));
    };
    auto applyPauseAudio = [this](bool playUi) {
        AppSettings settings = Storage::loadSettings();
        settings.sfxEnabled = pauseSfxCheck_->isChecked();
        settings.sfxVolume = pauseSfxSlider_->value();
        settings.bgmEnabled = pauseBgmCheck_->isChecked();
        settings.bgmVolume = pauseBgmSlider_->value();
        pauseSfxSlider_->setEnabled(settings.sfxEnabled);
        pauseSfxValue_->setText(QString("%1%").arg(settings.sfxVolume));
        pauseBgmSlider_->setEnabled(settings.bgmEnabled);
        pauseBgmValue_->setText(QString("%1%").arg(settings.bgmVolume));
        Audio::instance().setSfxEnabled(settings.sfxEnabled);
        Audio::instance().setSfxVolume(settings.sfxVolume);
        Audio::instance().setBgmEnabled(settings.bgmEnabled);
        Audio::instance().setBgmVolume(settings.bgmVolume);
        Storage::saveSettings(settings);
        if (playUi && settings.sfxEnabled) {
            Audio::instance().play(SfxId::Ui);
        }
        if (settings.bgmEnabled && running_ && !session_.ended() && !session_.paused()) {
            Audio::instance().startBgmLoop();
        }
    };
    {
        syncPauseAudioUi(Storage::loadSettings());
    }
    connect(pauseSfxCheck_, &QCheckBox::toggled, this, [applyPauseAudio](bool) { applyPauseAudio(true); });
    connect(pauseSfxSlider_, &QSlider::valueChanged, this, [applyPauseAudio](int) { applyPauseAudio(false); });
    connect(pauseBgmCheck_, &QCheckBox::toggled, this, [this, applyPauseAudio](bool on) {
        applyPauseAudio(false);
        if (on && running_ && !session_.ended()) {
            Audio::instance().startBgmLoop();
        }
    });
    connect(pauseBgmSlider_, &QSlider::valueChanged, this, [applyPauseAudio](int) { applyPauseAudio(false); });

    guideText_ = new QLabel(pausePanel_);
    guideText_->setWordWrap(true);
    guideText_->setTextFormat(Qt::RichText);
    guideText_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    guideText_->setFixedWidth(390);
    guideScroll_ = new QScrollArea(pausePanel_);
    guideScroll_->setWidget(guideText_);
    guideScroll_->setWidgetResizable(false);
    guideScroll_->setFixedSize(420, 280);
    guideScroll_->setFrameShape(QFrame::NoFrame);
    guideScroll_->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    if (touchUi_) {
        QScroller::grabGesture(guideScroll_->viewport(), QScroller::LeftMouseButtonGesture);
    }
    if (touchUi_) {
        pauseLayout->addStretch(1);
    }
    pauseRoot->addWidget(guideScroll_);
    pausePanel_->setObjectName("panel");
    pausePanel_->setStyleSheet("QWidget#panel { background: #14110f; border: 1px solid #5c3a32; } QLabel { background: transparent; color: #d7c7b4; border: none; } QCheckBox { color: #d7c7b4; }");
    pausePanel_->hide();

    resultPanel_ = new QWidget(this);
    auto* resultLayout = new QVBoxLayout(resultPanel_);
    resultLayout->setContentsMargins(18, 18, 18, 18);
    resultText_ = new QLabel(resultPanel_);
    resultText_->setAlignment(Qt::AlignCenter);
    resultLayout->addWidget(resultText_);
    auto* back = new QPushButton("返回主菜单", resultPanel_);
    resultLayout->addWidget(back);
    connect(back, &QPushButton::clicked, this, [this] { leaveToMenu(); });
    resultPanel_->setObjectName("panel");
    resultPanel_->setStyleSheet("QWidget#panel { background: #14110f; border: 1px solid #5c3a32; } QLabel { background: transparent; color: #d7c7b4; border: none; }");
    resultPanel_->hide();

    // 不用 QMessageBox：安卓上它是独立窗口，点击经常收不到
    confirmPanel_ = new QWidget(this);
    auto* confirmLayout = new QVBoxLayout(confirmPanel_);
    confirmLayout->setContentsMargins(18, 18, 18, 18);
    auto* confirmText = new QLabel("结束本局并记下用时和积分？", confirmPanel_);
    confirmText->setAlignment(Qt::AlignCenter);
    confirmLayout->addWidget(confirmText);
    auto* confirmRow = new QHBoxLayout();
    auto* confirmNo = new QPushButton("取消", confirmPanel_);
    auto* confirmYes = new QPushButton("结算", confirmPanel_);
    confirmRow->addWidget(confirmNo);
    confirmRow->addWidget(confirmYes);
    confirmLayout->addLayout(confirmRow);
    connect(confirmNo, &QPushButton::clicked, this, [this] {
        confirmPanel_->hide();
        Audio::instance().playRefuseBgm();
        if (session_.paused() && !session_.ended()) {
            pausePanel_->show();
            pausePanel_->raise();
            layoutOverlays();
        }
    });
    connect(confirmYes, &QPushButton::clicked, this, [this] {
        confirmPanel_->hide();
        session_.settle();
        commitEnd();
    });
    confirmPanel_->setObjectName("panel");
    confirmPanel_->setStyleSheet("QWidget#panel { background: #14110f; border: 1px solid #5c3a32; } QLabel { background: transparent; color: #d7c7b4; border: none; }");
    confirmPanel_->hide();

    voidPanel_ = new QWidget(this);
    auto* voidLayout = new QVBoxLayout(voidPanel_);
    voidLayout->setContentsMargins(18, 18, 18, 18);
    auto* voidText = new QLabel(QStringLiteral("呵呵呵，还要在这没有希望的世界中挣扎吗？万灵寂灭才是这个世界的归宿。"), voidPanel_);
    voidText->setWordWrap(true);
    voidText->setAlignment(Qt::AlignCenter);
    voidText->setMinimumWidth(400);
    voidLayout->addWidget(voidText);
    auto* voidRow = new QHBoxLayout();
    auto* voidEnd = new QPushButton(QStringLiteral("结束本轮游戏"), voidPanel_);
    auto* voidStay = new QPushButton(QStringLiteral("继续本轮游戏"), voidPanel_);
    voidRow->addWidget(voidEnd);
    voidRow->addWidget(voidStay);
    voidLayout->addLayout(voidRow);
    connect(voidEnd, &QPushButton::clicked, this, [this] {
        voidPanel_->hide();
        session_.settle();
        commitEnd();
    });
    connect(voidStay, &QPushButton::clicked, this, [this] {
        voidPanel_->hide();
        session_.setPaused(false);
        Audio::instance().setBgmPaused(false);
        Audio::instance().playRefuseBgm();
        setFocus();
    });
    voidPanel_->setObjectName("panel");
    voidPanel_->setStyleSheet("QWidget#panel { background: #14110f; border: 1px solid #5c3a32; } QLabel { background: transparent; color: #d7c7b4; border: none; }");
    voidPanel_->hide();

    revivePanel_ = new QWidget(this);
    auto* reviveLayout = new QVBoxLayout(revivePanel_);
    reviveLayout->setContentsMargins(18, 18, 18, 18);
    reviveText_ = new QLabel(revivePanel_);
    reviveText_->setWordWrap(true);
    reviveText_->setAlignment(Qt::AlignCenter);
    reviveText_->setMinimumWidth(400);
    reviveLayout->addWidget(reviveText_);
    auto* reviveRow = new QHBoxLayout();
    auto* reviveYes = new QPushButton(QStringLiteral("确认回归"), revivePanel_);
    auto* reviveNo = new QPushButton(QStringLiteral("拒绝回归"), revivePanel_);
    reviveRow->addWidget(reviveYes);
    reviveRow->addWidget(reviveNo);
    reviveLayout->addLayout(reviveRow);
    connect(reviveYes, &QPushButton::clicked, this, [this] {
        revivePanel_->hide();
        session_.acceptRevive();
        Audio::instance().setBgmPaused(false);
        setFocus();
    });
    connect(reviveNo, &QPushButton::clicked, this, [this] {
        revivePanel_->hide();
        session_.declineRevive();
        Audio::instance().setBgmPaused(false);
        Audio::instance().playRefuseBgm();
        setFocus();
    });
    revivePanel_->setObjectName("panel");
    revivePanel_->setStyleSheet("QWidget#panel { background: #14110f; border: 1px solid #5c3a32; } QLabel { background: transparent; color: #d7c7b4; border: none; }");
    revivePanel_->hide();

    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, [this] { togglePause(); });
    auto* tab = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    tab->setContext(Qt::WidgetWithChildrenShortcut);
    connect(tab, &QShortcut::activated, this, [this] { togglePause(); });

#ifdef Q_OS_ANDROID
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        const bool inGame = running_ && !session_.ended();
        if (state == Qt::ApplicationActive) {
            if (!(inGame && session_.paused())) {
                Audio::instance().setBgmPaused(false);
            }
            return;
        }
        if (inGame && !session_.paused()) {
            setPaused(true);
        }
        Audio::instance().setBgmPaused(true);
    });
#endif

    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, [this] { tick(); });
    timer_->start(16);
    clock_.start();
}

void GameWidget::leaveToMenu() {
    running_ = false;
    session_.setPaused(false);
    Audio::instance().ensureBgmLoop();
    pausePanel_->hide();
    resultPanel_->hide();
    confirmPanel_->hide();
    voidPanel_->hide();
    revivePanel_->hide();
    releaseAllTouches();
    emit returnedToMenu();
}

void GameWidget::refreshGuide() {
    const Player& player = session_.player();
    auto block = [](const QString& key, const SkillText& text, const QString& extra) {
        return QString("<p style='margin:8px 0 10px 0;'><span style='color:#c45c48;'>%1</span>　<b>%2</b>　%3<br><span style='color:#a89888;'>%4</span></p>")
            .arg(key, text.name, extra, text.detail);
    };
    auto cdText = [](float cd) {
        return cd > 0.05f ? QString("%1 秒").arg(cd, 0, 'f', 1) : QString("0.0 秒");
    };
    auto progress = [](const QString& name, float current, float need, bool owned, const QString& detail) {
        const int shown = int(std::min(current, need));
        const QString state = owned ? "已获得" : "未获得";
        return QString("<p style='margin:8px 0 10px 0;'><b>%1</b>　%2 / %3　%4<br><span style='color:#a89888;'>%5</span></p>")
            .arg(name)
            .arg(shown)
            .arg(int(need))
            .arg(state)
            .arg(detail);
    };
    QString html;
    html += "<p style='color:#e4d4c4; margin:0 0 4px 0;'><b>技能</b></p>";
    const bool touch = touchUi_;
    html += block(touch ? "点按攻击" : "鼠标", lightAttackText(), "");
    html += block(touch ? "长按攻击" : "长按", heavyAttackText(), "");
#if (defined(Q_OS_WIN) || defined(Q_OS_LINUX)) && !defined(Q_OS_ANDROID)
    if (!touch) {
        html += block("中键", SkillText{QStringLiteral("索敌"), QStringLiteral("开关自动锁定附近敌人，朝向和攻击转向锁定目标。")},
            autoAim_ ? QStringLiteral("开启") : QStringLiteral("关闭"));
    }
#endif
    html += block(touch ? "按钮" : "Q", guardSkillText(), "冷却 " + cdText(player.cdGuard));
    html += block(touch ? "按钮" : "E", healSkillText(), "冷却 " + cdText(player.cdHeal));
    auto skillExtra = [&](int skill, float cd) -> QString {
        if (skill == kSkillSwordQi) {
            return player.stacksQi < 3
                ? QString("层数 %1/3　回层 %2").arg(player.stacksQi).arg(cdText(player.cdQiStack))
                : QString("层数 %1/3").arg(player.stacksQi);
        }
        if (skill == kSkillThrust) {
            return player.stacksThrust < 3
                ? QString("层数 %1/3　回层 %2").arg(player.stacksThrust).arg(cdText(player.cdThrustStack))
                : QString("层数 %1/3").arg(player.stacksThrust);
        }
        if (skill == kSkillFlight || skill == kSkillJetpack) {
            return player.flying ? QString("飞行中") : QString("关");
        }
        if (skill == kSkillSeek) {
            return player.seekOn ? QString("开启") : QString("关闭");
        }
        if (skill == kSkillMirror) {
            return player.mirrorT > 0.f
                ? QString("吸收 %1/%2　剩余 %3s").arg(int(player.mirrorAbsorbed)).arg(int(player.mirrorCap)).arg(player.mirrorT, 0, 'f', 1)
                : "冷却 " + cdText(cd);
        }
        const float active = skill == kSkillBerserk ? player.berserkT
            : skill == kSkillOverload              ? player.overloadT
            : skill == kSkillMagField              ? player.fieldT
            : skill == kSkillMedkit                ? player.medkitT
                                                   : 0.f;
        if (active > 0.f) {
            return QString("%1中　剩余 %2s　冷却 %3").arg(skillText(skill).name).arg(active, 0, 'f', 1).arg(cdText(cd));
        }
        return "冷却 " + cdText(cd);
    };
    html += block(touch ? "技能" : "R", skillText(player.skillD), skillExtra(player.skillD, player.cdD));
    html += block(touch ? "技能" : "F", skillText(player.skillF), skillExtra(player.skillF, player.cdF));
    html += block(touch ? "技能" : "C", skillText(player.skillC), skillExtra(player.skillC, player.cdC));
    if (player.hero == HeroClass::Mage && player.skillV >= 0) {
        html += block(touch ? "技能" : "V", skillText(player.skillV), skillExtra(player.skillV, player.cdV));
    }
    if (player.talentGuide) {
        html += block(touch ? "技能" : "G", skillText(kSkillSeek), skillExtra(kSkillSeek, 0.f));
    }
    html += "<p style='color:#e4d4c4; margin:12px 0 4px 0;'><b>天赋</b></p>";
    html += progress("重手", player.damageDealt, 250.f, player.talentMight, talentMightDetail());
    html += progress("远行", player.distanceMoved, 900.f, player.talentStride, talentStrideDetail());
    html += progress("轻身", float(player.dodgeCount), 6.f, player.talentLight, talentLightDetail());
    html += progress("熟练", float(player.skillCasts), 12.f, player.talentMastery, talentMasteryDetail());
    html += progress("世界指引", float(player.worldKills), 20.f, player.talentGuide, talentGuideDetail());
    html += progress("以小博大", float(player.underdogKills), 10.f, player.talentUnderdog, talentUnderdogDetail());
    if (const int charms = session_.talismanCount()) {
        html += QString("<p style='color:#e4d4c4; margin:12px 0 4px 0;'><b>物品</b></p>");
        html += QString("<p style='margin:8px 0 10px 0;'><b>%1 x%2</b><br><span style='color:#a89888;'>%3</span></p>")
            .arg(itemText(kItemReturnTalisman).name)
            .arg(charms)
            .arg(itemText(kItemReturnTalisman).detail);
    }
    if (session_.cursed()) {
        html += QString("<p style='color:#e4d4c4; margin:12px 0 4px 0;'><b>诅咒</b></p>");
        html += QString("<p style='margin:8px 0 10px 0;'><span style='color:#c45c48;'><b>存在被克苏鲁余光注意！</b></span>"
                        "<br><span style='color:#a89888;'>本轮永久：偶尔刷出双倍血量的精英怪物，击杀精英的积分翻倍。</span></p>");
    }
    guideText_->setText(html);
    guideText_->adjustSize();
}

void GameWidget::startNew(HeroClass hero, int skillD, int skillF, int skillC, int skillV) {
    running_ = true;
    const uint32_t seed = uint32_t(QRandomGenerator::global()->generate());
    const uint32_t runId = uint32_t(QRandomGenerator::global()->generate());
    const AppSettings settings = Storage::loadSettings();
    session_.newGame(seed == 0 ? 1u : seed, runId == 0 ? 1u : runId, hero, skillD, skillF, skillC, skillV,
        settings.guideAtStart);
    particles_.clear();
    endCommitted_ = false;
    clock_.restart();
    releaseAllTouches();
    pausePanel_->hide();
    resultPanel_->hide();
    confirmPanel_->hide();
    voidPanel_->hide();
    revivePanel_->hide();
    Audio::instance().startBgmLoop();
    setFocus();
}

void GameWidget::startContinue(const QJsonObject& game) {
    running_ = true;
    session_.loadFrom(game);
    session_.drainVfx();
    particles_.clear();
    endCommitted_ = false;
    clock_.restart();
    releaseAllTouches();
    pausePanel_->hide();
    resultPanel_->hide();
    confirmPanel_->hide();
    voidPanel_->hide();
    revivePanel_->hide();
    Audio::instance().startBgmLoop();
    setFocus();
}

void GameWidget::setPaused(bool paused) {
    if (session_.ended()) {
        return;
    }
    // 结算确认还开着时取消暂停（Esc 等），同样是拒绝结束本轮
    const bool declined = !paused && confirmPanel_ && confirmPanel_->isVisible();
    session_.setPaused(paused);
    confirmPanel_->hide();
    pausePanel_->setVisible(paused);
    Audio::instance().setBgmPaused(paused);
    if (declined) {
        Audio::instance().playRefuseBgm();
    }
    if (paused) {
        const AppSettings settings = Storage::loadSettings();
        QSignalBlocker b1(pauseSfxCheck_);
        QSignalBlocker b2(pauseSfxSlider_);
        QSignalBlocker b3(pauseBgmCheck_);
        QSignalBlocker b4(pauseBgmSlider_);
        pauseSfxCheck_->setChecked(settings.sfxEnabled);
        pauseSfxSlider_->setValue(settings.sfxVolume);
        pauseSfxSlider_->setEnabled(settings.sfxEnabled);
        pauseSfxValue_->setText(QString("%1%").arg(settings.sfxVolume));
        pauseBgmCheck_->setChecked(settings.bgmEnabled);
        pauseBgmSlider_->setValue(settings.bgmVolume);
        pauseBgmSlider_->setEnabled(settings.bgmEnabled);
        pauseBgmValue_->setText(QString("%1%").arg(settings.bgmVolume));
        refreshGuide();
        pausePanel_->raise();
        layoutOverlays();
    }
}

void GameWidget::togglePause() {
    if (session_.ended() || (voidPanel_ && voidPanel_->isVisible()) || (revivePanel_ && revivePanel_->isVisible())) {
        return;
    }
    const bool pausing = !session_.paused();
    if (pausing) {
        releaseAllTouches();
    }
    setPaused(pausing);
    if (!pausing) {
        setFocus();
    }
}

void GameWidget::saveGame() {
    if (Storage::saveContinue(session_.toJson())) {
        toast_ = "已存档";
        toastTime_ = 1.3f;
    } else {
        toast_ = "存档失败";
        toastTime_ = 1.3f;
    }
}

void GameWidget::askSettle() {
    if (session_.ended() || (voidPanel_ && voidPanel_->isVisible())) {
        return;
    }
    pausePanel_->hide();
    confirmPanel_->show();
    confirmPanel_->raise();
    layoutOverlays();
}

void GameWidget::showVoidPrompt() {
    if (!running_ || session_.ended() || (voidPanel_ && voidPanel_->isVisible())) {
        return;
    }
    session_.setPaused(true);
    Audio::instance().setBgmPaused(true);
    pausePanel_->hide();
    confirmPanel_->hide();
    voidPanel_->show();
    voidPanel_->raise();
    layoutOverlays();
}

void GameWidget::showRevivePrompt() {
    if (!running_ || session_.ended() || (revivePanel_ && revivePanel_->isVisible())) {
        return;
    }
    session_.setPaused(true);
    Audio::instance().setBgmPaused(true);
    pausePanel_->hide();
    confirmPanel_->hide();
    voidPanel_->hide();
    const int owned = session_.talismanCount();
    reviveText_->setText(QStringLiteral("意识回归符咒生效\n持有 %1 张\n确认回归：消耗一张，原地复活，生命恢复到 25%\n拒绝回归：直接结算，剩余符咒折算积分")
            .arg(owned));
    revivePanel_->show();
    revivePanel_->raise();
    layoutOverlays();
}

void GameWidget::commitEnd() {
    if (!session_.ended() || endCommitted_) {
        return;
    }
    endCommitted_ = true;
    session_.setPaused(false);
    // 结算前若正暂停（从暂停菜单主动结算），把 BGM 唤醒，结果面板不再静音
    Audio::instance().setBgmPaused(false);
    pausePanel_->hide();
    confirmPanel_->hide();
    voidPanel_->hide();
    revivePanel_->hide();
    releaseAllTouches();
    Records records = Storage::loadRecords();
    if (session_.time() > records.bestTime) {
        records.bestTime = session_.time();
    }
    if (session_.score() > records.bestScore) {
        records.bestScore = session_.score();
    }
    Storage::saveRecords(records);
    Storage::deleteIfRun(session_.runId());
    const QString reason = session_.reason() == EndReason::Death ? "你倒下了" : "本局已结算";
    QString result = reason + "\n用时 " + formatTime(session_.time()) + "\n积分 " + QString::number(session_.score());
    if (const int bonus = session_.talismanBonus()) {
        result += QString("\n意识回归符咒折算 +%1 积分").arg(bonus);
    }
    resultText_->setText(result);
    resultPanel_->show();
    resultPanel_->raise();
    layoutOverlays();
}

void GameWidget::layoutOverlays() {
    constexpr int kGuideHeight = 280;
    if (touchUi_) {
        guideScroll_->setFixedHeight(std::max(120, height() - 16 - 36));
        pausePanel_->adjustSize();
    } else {
        guideScroll_->setFixedHeight(kGuideHeight);
        pausePanel_->adjustSize();
        const int overflow = pausePanel_->height() - (height() - 16);
        if (overflow > 0) {
            guideScroll_->setFixedHeight(std::max(80, kGuideHeight - overflow));
            pausePanel_->adjustSize();
        }
    }
    pausePanel_->move((width() - pausePanel_->width()) / 2, (height() - pausePanel_->height()) / 2);
    resultPanel_->adjustSize();
    resultPanel_->move((width() - resultPanel_->width()) / 2, (height() - resultPanel_->height()) / 2);
    confirmPanel_->adjustSize();
    confirmPanel_->move((width() - confirmPanel_->width()) / 2, (height() - confirmPanel_->height()) / 2);
    voidPanel_->adjustSize();
    voidPanel_->move((width() - voidPanel_->width()) / 2, (height() - voidPanel_->height()) / 2);
    revivePanel_->adjustSize();
    revivePanel_->move((width() - revivePanel_->width()) / 2, (height() - revivePanel_->height()) / 2);
}

void GameWidget::tick() {
    const qint64 elapsedNs = clock_.nsecsElapsed();
    clock_.restart();
    const float frame = std::min(0.05f, float(elapsedNs) / 1e9f);
    toastTime_ = std::max(0.f, toastTime_ - frame);
    flashT_ = std::max(0.f, flashT_ - frame);
    if (running_ && !session_.paused() && !session_.ended()) {
        updateAimTarget();
        const QPointF world = mouseWorld();
        // 按真实帧间隔推进：固定步长又不插值时，定时器与屏幕刷新错拍会出现 0 步 / 2 步交替的顿挫
        const int steps = std::max(1, int(std::ceil(frame / kSimDt - 0.05f)));
        const float dt = frame / float(steps);
        for (int i = 0; i < steps; ++i) {
            session_.update(dt, input_, float(world.x()), float(world.y()));
            for (SfxId id : session_.drainSfx()) {
                Audio::instance().play(id);
            }
            for (const VfxEvent& event : session_.drainVfx()) {
                spawnVfx(event);
            }
            if (session_.consumeRecoverBgm()) {
                Audio::instance().playRecoverBgm();
            }
            input_.clearEdges();
        }
        updateParticles(frame);
    }
    if (session_.ended()) {
        session_.consumeVoidPrompt();
        commitEnd();
    } else if (session_.consumeRevivePrompt()) {
        showRevivePrompt();
    } else if (session_.consumeVoidPrompt()) {
        showVoidPrompt();
    }
    const QString notice = session_.pullNotice();
    if (!notice.isEmpty()) {
        toast_ = notice;
        toastTime_ = 1.5f;
    }
    update();
}

float GameWidget::vfxRand() {
    vfxRng_ ^= vfxRng_ << 13;
    vfxRng_ ^= vfxRng_ >> 17;
    vfxRng_ ^= vfxRng_ << 5;
    return float(vfxRng_ & 0xFFFFFFu) / float(0x1000000);
}

void GameWidget::spawnVfx(const VfxEvent& event) {
    auto spray = [&](int count, float speedMin, float speedMax, float vzMin, float vzMax, float gravity,
                    float lifeMin, float lifeMax, float sizeMin, float sizeMax, float z, float spread,
                    const QColor* colors, int colorCount, bool glow) {
        for (int i = 0; i < count && particles_.size() < kMaxParticles; ++i) {
            Particle p;
            const float a = vfxRand() * kPi * 2.f;
            const float speed = speedMin + (speedMax - speedMin) * vfxRand();
            const float off = spread * std::sqrt(vfxRand());
            p.x = event.x + std::cos(a) * off;
            p.y = event.y + std::sin(a) * off * 0.6f;
            p.z = z;
            p.vx = std::cos(a) * speed;
            p.vy = std::sin(a) * speed * 0.6f;
            p.vz = vzMin + (vzMax - vzMin) * vfxRand();
            p.gravity = gravity;
            p.maxLife = p.life = lifeMin + (lifeMax - lifeMin) * vfxRand();
            p.size = sizeMin + (sizeMax - sizeMin) * vfxRand();
            p.color = colors[int(vfxRand() * float(colorCount)) % colorCount].rgb();
            p.glow = glow;
            particles_.push_back(p);
        }
    };
    const bool big = event.monster == MonsterKind::Eye;
    const float bodyZ = big ? 30.f : (event.monster == MonsterKind::Flyer ? 22.f : 10.f);
    switch (event.kind) {
    case VfxKind::Hit: {
        static const QColor normal[] = {QColor(255, 244, 214), QColor(255, 206, 120)};
        static const QColor crit[] = {QColor(255, 226, 90), QColor(255, 255, 220), QColor(255, 150, 60)};
        if (event.crit) {
            spray(7, 60.f, 150.f, 30.f, 110.f, 320.f, 0.25f, 0.45f, 2.f, 3.f, bodyZ, 3.f, crit, 3, true);
        } else {
            spray(3, 50.f, 110.f, 20.f, 80.f, 320.f, 0.2f, 0.35f, 1.5f, 2.5f, bodyZ, 3.f, normal, 2, true);
        }
        break;
    }
    case VfxKind::Kill: {
        QColor debris[2] = {QColor(110, 200, 90), QColor(60, 140, 50)};
        switch (event.monster) {
        case MonsterKind::Skeleton: debris[0] = QColor(228, 222, 204); debris[1] = QColor(150, 140, 120); break;
        case MonsterKind::Mushroom: debris[0] = QColor(222, 92, 70); debris[1] = QColor(240, 204, 160); break;
        case MonsterKind::Flyer: debris[0] = QColor(150, 110, 200); debris[1] = QColor(90, 60, 130); break;
        case MonsterKind::Caster: debris[0] = QColor(176, 84, 214); debris[1] = QColor(96, 40, 124); break;
        case MonsterKind::Killbot: debris[0] = QColor(150, 160, 172); debris[1] = QColor(255, 160, 60); break;
        case MonsterKind::Eye: debris[0] = QColor(224, 40, 52); debris[1] = QColor(255, 204, 204); break;
        default: break;
        }
        spray(big ? 28 : 10, 30.f, big ? 140.f : 90.f, 60.f, 150.f, 340.f, 0.5f, 0.9f, 2.f, 3.5f, bodyZ * 0.8f, big ? 10.f : 4.f, debris, 2, false);
        static const QColor soul[] = {QColor(236, 230, 255), QColor(190, 180, 230)};
        spray(big ? 10 : 3, 4.f, 14.f, 16.f, 34.f, -30.f, 0.6f, 1.0f, 1.5f, 2.5f, bodyZ, 5.f, soul, 2, true);
        break;
    }
    case VfxKind::Explode: {
        static const QColor fire[] = {QColor(255, 220, 110), QColor(255, 150, 50), QColor(255, 96, 40)};
        static const QColor smoke[] = {QColor(70, 64, 60), QColor(96, 88, 80)};
        const float r = std::max(10.f, event.radius);
        spray(8 + int(r / 4.f), r * 1.6f, r * 3.6f, 40.f, 130.f, 280.f, 0.3f, 0.6f, 2.f, 3.f, 12.f, r * 0.2f, fire, 3, true);
        spray(3, 6.f, 18.f, 14.f, 30.f, -12.f, 0.6f, 0.9f, 4.f, 6.f, 12.f, r * 0.3f, smoke, 2, false);
        break;
    }
    case VfxKind::Heal: {
        static const QColor green[] = {QColor(120, 255, 150), QColor(200, 255, 210)};
        spray(10, 2.f, 10.f, 18.f, 40.f, -40.f, 0.6f, 1.0f, 1.5f, 2.5f, 4.f, 12.f, green, 2, true);
        break;
    }
    case VfxKind::LevelUp: {
        static const QColor gold[] = {QColor(255, 216, 90), QColor(255, 246, 200), QColor(255, 180, 60)};
        spray(20, 4.f, 20.f, 40.f, 110.f, -50.f, 0.6f, 1.1f, 1.5f, 3.f, 2.f, 16.f, gold, 3, true);
        break;
    }
    case VfxKind::Rage: {
        // 狂化：贴地炸开一圈火星，再往上飘几缕余烬
        static const QColor ember[] = {QColor(255, 92, 58), QColor(255, 148, 60), QColor(255, 214, 130)};
        spray(14, 40.f, 130.f, 30.f, 90.f, 240.f, 0.3f, 0.55f, 2.f, 3.f, 6.f, 6.f, ember, 3, true);
        spray(8, 3.f, 16.f, 26.f, 60.f, -30.f, 0.6f, 1.0f, 1.5f, 2.5f, 6.f, 14.f, ember, 3, true);
        break;
    }
    case VfxKind::AtomicCharge: {
        // 蓄力：紫黑雾贴着地面往身上聚，配合脚下那圈紫色能量环
        static const QColor haze[] = {QColor(96, 44, 158), QColor(52, 20, 88), QColor(22, 12, 34)};
        spray(18, 6.f, 26.f, 24.f, 66.f, -16.f, 0.35f, 0.6f, 1.5f, 3.f, 2.f, 14.f, haze, 3, true);
        break;
    }
    case VfxKind::AtomicFlash: {
        // 引爆前那一拍：整个画面被白光吞掉
        flashT_ = kAtomicFlashFade;
        flashMax_ = kAtomicFlashFade;
        break;
    }
    case VfxKind::AtomicBlast: {
        // 引爆：紫黑火球掀开、大团烟尘升起、地被掀起的扬尘和崩出去的碎石
        static const QColor fire[] = {QColor(162, 68, 230), QColor(96, 32, 160), QColor(230, 192, 255)};
        static const QColor smoke[] = {QColor(30, 22, 44), QColor(14, 9, 20)};
        static const QColor dust[] = {QColor(150, 122, 84), QColor(104, 82, 56)};
        static const QColor debris[] = {QColor(58, 46, 70), QColor(96, 72, 40)};
        // 火球心：高速外掀，很快被重力拽回来
        spray(56, 120.f, 340.f, 50.f, 190.f, 210.f, 0.45f, 0.95f, 2.f, 4.5f, 10.f, 26.f, fire, 3, true);
        // 蘑菇云的大团烟：又慢又大，浮得最久
        spray(48, 10.f, 46.f, 34.f, 110.f, -30.f, 0.9f, 1.7f, 4.f, 8.f, 8.f, 48.f, smoke, 2, false);
        // 地表扬尘：贴着地面铺得更开的一层土色
        spray(34, 40.f, 150.f, 8.f, 40.f, 90.f, 0.7f, 1.3f, 3.f, 6.f, 2.f, 64.f, dust, 2, false);
        // 碎石：抛得高，落地还会弹一下
        spray(40, 150.f, 300.f, 90.f, 210.f, 430.f, 0.6f, 1.2f, 1.5f, 3.f, 12.f, 18.f, debris, 2, false);
        break;
    }
    }
}

void GameWidget::updateParticles(float dt) {
    const float damp = std::max(0.f, 1.f - 2.4f * dt);
    for (Particle& p : particles_) {
        p.life -= dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        p.vz -= p.gravity * dt;
        p.vx *= damp;
        p.vy *= damp;
        if (p.z < 0.f) {
            p.z = 0.f;
            p.vz = -p.vz * 0.3f;
            p.vx *= 0.5f;
            p.vy *= 0.5f;
        }
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life <= 0.f; }), particles_.end());
}

void GameWidget::drawParticles(QPainter& painter) {
    for (const Particle& p : particles_) {
        const float a = std::clamp(p.life / p.maxLife, 0.f, 1.f);
        QColor c = QColor::fromRgb(p.color);
        const float px = p.x;
        const float py = p.y - p.z;
        if (p.glow) {
            const float g = p.size * 2.6f;
            c.setAlpha(int(70.f * a));
            painter.fillRect(QRectF(px - g * 0.5f, py - g * 0.5f, g, g), c);
        }
        c.setAlpha(int(255.f * a));
        painter.fillRect(QRectF(px - p.size * 0.5f, py - p.size * 0.5f, p.size, p.size), c);
    }
}

QRect GameWidget::viewRect() const {
    int viewW = 0;
    int viewH = 0;
    if (touchUi_) {
        // 手机屏幕尺寸五花八门，按比例铺满，不强求整数倍像素
        const qreal scale = std::min(width() / qreal(kViewW), height() / qreal(kViewH));
        viewW = int(kViewW * scale);
        viewH = int(kViewH * scale);
    } else {
        const int scale = std::max(1, std::min(width() / kViewW, height() / kViewH));
        viewW = kViewW * scale;
        viewH = kViewH * scale;
    }
    return QRect((width() - viewW) / 2, (height() - viewH) / 2, viewW, viewH);
}

QPointF GameWidget::mouseWorld() const {
    const Player& player = session_.player();
    if (const Monster* target = aimTarget()) {
        return QPointF(target->x, target->y);
    }
    if (touchUi_) {
        // 只决定朝向（攻击 / 技能方向）；闪避方向仍取摇杆的移动方向
        return QPointF(player.x + aimDir_.x() * 48.0, player.y + aimDir_.y() * 48.0);
    }
    const QRect view = viewRect();
    const QPoint local = mapFromGlobal(QCursor::pos());
    const float canvasX = (local.x() - view.x()) * float(kViewW) / float(view.width());
    const float canvasY = (local.y() - view.y()) * float(kViewH) / float(view.height());
    const float cameraX = player.x - kViewW * 0.5f;
    const float cameraY = player.y - kViewH * 0.5f;
    return QPointF(cameraX + canvasX, cameraY + canvasY);
}

void GameWidget::syncKey(int key, bool down) {
    switch (key) {
    case Qt::Key_W:
        input_.w = down;
        break;
    case Qt::Key_A:
        input_.a = down;
        break;
    case Qt::Key_S:
        input_.s = down;
        break;
    case Qt::Key_D:
        input_.d = down;
        break;
    case Qt::Key_Shift:
        input_.shift = down;
        if (down) {
            input_.shiftEdge = true;
        }
        break;
    case Qt::Key_Space:
        if (down) {
            input_.spaceEdge = true;
        }
        break;
    case Qt::Key_Q:
        if (down) {
            input_.qEdge = true;
        }
        break;
    case Qt::Key_E:
        if (down) {
            input_.eEdge = true;
        }
        break;
    case Qt::Key_R:
        if (down) {
            input_.rEdge = true;
        }
        break;
    case Qt::Key_F:
        if (down) {
            input_.fEdge = true;
        }
        break;
    case Qt::Key_C:
        if (down) {
            input_.cEdge = true;
        }
        break;
    case Qt::Key_V:
        if (down) {
            input_.vEdge = true;
        }
        break;
    case Qt::Key_B:
        if (down) {
            input_.bEdge = true;
        }
        break;
    case Qt::Key_G:
        if (down) {
            input_.gEdge = true;
        }
        break;
    default:
        break;
    }
}

void GameWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Back) {
        // Android 返回键：游戏中当作暂停，避免直接退出应用
        if (running_ && !session_.ended()) {
            togglePause();
        }
        event->accept();
        return;
    }
    if (event->isAutoRepeat()) {
        return;
    }
    syncKey(event->key(), true);
}

void GameWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return;
    }
    syncKey(event->key(), false);
}

void GameWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (event->pointingDevice() && event->pointingDevice()->type() == QInputDevice::DeviceType::TouchScreen) {
        return;
    }
#ifndef Q_OS_ANDROID
    if (touchUi_ && !Platform::touchUi()) {
        touchUi_ = false;
        releaseAllTouches();
    }
#endif
    if (event->button() == Qt::LeftButton) {
        input_.lmb = true;
        input_.lmbEdge = true;
    } else if (event->button() == Qt::RightButton) {
        input_.rmb = true;
        input_.rmbEdge = true;
    }
#if (defined(Q_OS_WIN) || defined(Q_OS_LINUX)) && !defined(Q_OS_ANDROID)
    else if (event->button() == Qt::MiddleButton && running_ && !session_.ended() && !session_.paused()) {
        toggleAutoAim();
    }
#endif
}

void GameWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->pointingDevice() && event->pointingDevice()->type() == QInputDevice::DeviceType::TouchScreen) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        input_.lmb = false;
        input_.lmbUp = true;
    } else if (event->button() == Qt::RightButton) {
        input_.rmb = false;
    }
}

bool GameWidget::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        auto* touch = static_cast<QTouchEvent*>(event);
        if (event->type() == QEvent::TouchBegin && !touch->points().isEmpty()
            && childAt(touch->points().first().position().toPoint())) {
            // 落在暂停 / 结算面板上：交给 Qt 转成鼠标事件给按钮
            event->ignore();
            return false;
        }
        touchUi_ = true;
        handleTouch(touch);
        event->accept();
        return true;
    }
    case QEvent::TouchCancel:
        releaseAllTouches();
        event->accept();
        return true;
    default:
        return QWidget::event(event);
    }
}

qreal GameWidget::touchUnit() const {
    return std::min<qreal>(height(), width() * 0.5625);
}

QPointF GameWidget::stickHome() const {
    const qreal u = touchUnit();
    return QPointF(u * 0.22, height() - u * 0.22);
}

QVector<GameWidget::TouchButton> GameWidget::touchButtons() const {
    const Player& player = session_.player();
    const bool mage = player.hero == HeroClass::Mage;
    const qreal u = touchUnit();
    const qreal w = width();
    const qreal h = height();
    QVector<TouchButton> buttons;

    // 右下：攻击居角，内圈闪避 / 防御 / 跳跃，外圈技能与恢复
    const QPointF attack(w - u * 0.19, h - u * 0.19);
    buttons.push_back({TouchControl::Attack, attack, u * 0.105, QStringLiteral("攻击")});
    auto around = [&](qreal dist, qreal degrees) {
        const qreal rad = qDegreesToRadians(degrees);
        return QPointF(attack.x() + std::cos(rad) * dist, attack.y() + std::sin(rad) * dist);
    };
    const qreal small = u * 0.058;
    const qreal inner = u * 0.215;
    buttons.push_back({TouchControl::Dodge, around(inner, 180.0), small, mage ? QStringLiteral("闪现") : QStringLiteral("闪避")});
    buttons.push_back({TouchControl::Guard, around(inner, 225.0), small, guardSkillText().name});
    buttons.push_back({TouchControl::Jump, around(inner, 270.0), small, QStringLiteral("跳跃")});

    QVector<QPair<TouchControl, QString>> outer = {
        {TouchControl::SkillR, skillText(player.skillD).name},
        {TouchControl::SkillF, skillText(player.skillF).name},
        {TouchControl::SkillC, skillText(player.skillC).name},
    };
    if (mage && player.skillV >= 0) {
        outer.push_back({TouchControl::SkillV, skillText(player.skillV).name});
    }
    outer.push_back({TouchControl::Heal, healSkillText().name});
    for (int i = 0; i < outer.size(); ++i) {
        const qreal degrees = 180.0 + 90.0 * i / (outer.size() - 1);
        buttons.push_back({outer[i].first, around(u * 0.37, degrees), small, outer[i].second});
    }

    // 右上：Tab（技能与天赋说明）/ Esc（暂停）
    const qreal top = u * 0.045;
    buttons.push_back({TouchControl::Pause, QPointF(w - u * 0.075, u * 0.075), top, QStringLiteral("暂停")});
    buttons.push_back({TouchControl::Guide, QPointF(w - u * 0.185, u * 0.075), top, QStringLiteral("说明")});
    buttons.push_back({TouchControl::AutoAim, QPointF(w - u * 0.295, u * 0.075), top, QStringLiteral("索敌")});
    if (player.talentGuide) {
        // 天赋「世界指引」解锁后才有的开关，桌面版是 G 键
        buttons.push_back({TouchControl::Seek, QPointF(w - u * 0.405, u * 0.075), top, QStringLiteral("寻路")});
    }
    return buttons;
}

QAbstractButton* GameWidget::overlayButtonAt(const QPointF& pos) const {
    for (QWidget* widget = childAt(pos.toPoint()); widget && widget != this; widget = widget->parentWidget()) {
        if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
            return button->isEnabled() ? button : nullptr;
        }
    }
    return nullptr;
}

void GameWidget::handleTouch(QTouchEvent* event) {
    const bool playing = running_ && !session_.ended();
    QSet<int> alive;
    for (const QEventPoint& point : event->points()) {
        const int id = point.id();
        const QPointF pos = point.position();
        if (point.state() != QEventPoint::Released) {
            alive.insert(id);
        }
        switch (point.state()) {
        case QEventPoint::Pressed: {
            // 同一 id 再次按下说明上一次的抬起没收到
            releaseBinding(id);
            overlayPresses_.remove(id);
            // 已有其他手指按着时，新触点不会被 Qt 转成鼠标事件，面板按钮只能在这里自己点
            if (QAbstractButton* button = overlayButtonAt(pos)) {
                overlayPresses_.insert(id, button);
                break;
            }
            if (!playing) {
                break;
            }
            // 左半屏只归摇杆，右半屏只归按键：两只手的触点互不抢占
            const bool stickZone = pos.x() < width() * 0.5;
            TouchControl hit = TouchControl::None;
            if (!stickZone) {
                for (const TouchButton& button : touchButtons()) {
                    if (QLineF(pos, button.center).length() <= button.radius * 1.2) {
                        hit = button.control;
                        break;
                    }
                }
            }
            if (hit == TouchControl::Guide || hit == TouchControl::Pause) {
                togglePause();
                touchBindings_.insert(id, hit);
            } else if (session_.paused()) {
                break;
            } else if (hit == TouchControl::AutoAim) {
                touchBindings_.insert(id, hit);
                toggleAutoAim();
            } else if (hit != TouchControl::None) {
                touchBindings_.insert(id, hit);
                pressTouch(hit, true);
            } else if (stickZone && stickTouchId_ < 0) {
                // 左半屏任意位置按下即出现摇杆；摇杆已被占用时左半屏的其他手指一律忽略
                stickTouchId_ = id;
                touchBindings_.insert(id, TouchControl::Stick);
                stickCenter_ = clampStickCenter(pos);
                updateStick(pos);
            }
            break;
        }
        case QEventPoint::Updated:
        case QEventPoint::Stationary:
            // 安卓只拿本次事件内的历史采样判断是否移动：别的手指按下 / 抬起 / 滑动时，
            // 摇杆手指常被标成 Stationary，但坐标已经是新的，不跟上摇杆就会卡住或跳变
            if (id == stickTouchId_) {
                updateStick(pos);
            }
            break;
        case QEventPoint::Released: {
            if (overlayPresses_.contains(id)) {
                const QPointer<QAbstractButton> button = overlayPresses_.take(id);
                if (button && button == overlayButtonAt(pos)) {
                    button->click();
                }
                break;
            }
            releaseBinding(id);
            break;
        }
        default:
            break;
        }
    }

    // 抬起事件在安卓上偶尔会丢（系统手势抢走触点等），摇杆就会一直朝一个方向走。
    // 每个触摸事件都带着所有仍按住的触点（含静止的），不在其中的绑定按已抬起处理
    const QList<int> bound = touchBindings_.keys();
    for (int id : bound) {
        if (!alive.contains(id)) {
            releaseBinding(id);
        }
    }
    if (stickTouchId_ >= 0 && !touchBindings_.contains(stickTouchId_)) {
        releaseStick();
    }
    const QList<int> pressed = overlayPresses_.keys();
    for (int id : pressed) {
        if (!alive.contains(id)) {
            overlayPresses_.remove(id);
        }
    }
}

void GameWidget::releaseBinding(int id) {
    const TouchControl control = touchBindings_.take(id);
    if (control == TouchControl::Stick) {
        releaseStick();
    } else if (control != TouchControl::None && control != TouchControl::Guide && control != TouchControl::Pause) {
        pressTouch(control, false);
    }
}

QPointF GameWidget::clampStickCenter(const QPointF& pos) const {
    const qreal radius = touchUnit() * 0.12;
    return QPointF(std::clamp(pos.x(), radius + 8.0, std::max(radius + 8.0, width() * 0.5)),
        std::clamp(pos.y(), radius + 8.0, std::max(radius + 8.0, height() - radius - 8.0)));
}

void GameWidget::updateStick(const QPointF& pos) {
    const qreal radius = touchUnit() * 0.12;
    QPointF delta = pos - stickCenter_;
    qreal dist = std::hypot(delta.x(), delta.y());
    if (dist > radius) {
        // 手指拖出底座时底座跟着走，反向时不必先划回原来的圆心
        stickCenter_ = clampStickCenter(pos - delta * (radius / dist));
        delta = pos - stickCenter_;
        dist = std::hypot(delta.x(), delta.y());
    }
    stickOffset_ = dist > radius ? delta * (radius / dist) : delta;
    const qreal dead = radius * 0.18;
    if (dist <= dead) {
        input_.moveX = 0.f;
        input_.moveY = 0.f;
        return;
    }
    const QPointF dir = delta / dist;
    const qreal strength = std::clamp((dist - dead) / (radius * 0.7 - dead), 0.0, 1.0);
    input_.moveX = float(dir.x() * strength);
    input_.moveY = float(dir.y() * strength);
    aimDir_ = dir;
}

void GameWidget::releaseStick() {
    stickTouchId_ = -1;
    stickOffset_ = QPointF();
    input_.moveX = 0.f;
    input_.moveY = 0.f;
}

void GameWidget::pressTouch(TouchControl control, bool down) {
    switch (control) {
    case TouchControl::Attack:
        input_.lmb = down;
        if (down) {
            input_.lmbEdge = true;
        } else {
            input_.lmbUp = true;
        }
        break;
    case TouchControl::Dodge:
        input_.rmb = down;
        if (down) {
            input_.rmbEdge = true;
        }
        break;
    case TouchControl::Jump:
        input_.spaceEdge = input_.spaceEdge || down;
        break;
    case TouchControl::Guard:
        input_.qEdge = input_.qEdge || down;
        break;
    case TouchControl::Heal:
        input_.eEdge = input_.eEdge || down;
        break;
    case TouchControl::SkillR:
        input_.rEdge = input_.rEdge || down;
        break;
    case TouchControl::SkillF:
        input_.fEdge = input_.fEdge || down;
        break;
    case TouchControl::SkillC:
        input_.cEdge = input_.cEdge || down;
        break;
    case TouchControl::SkillV:
        input_.vEdge = input_.vEdge || down;
        break;
    case TouchControl::Seek:
        input_.gEdge = input_.gEdge || down;
        break;
    default:
        break;
    }
}

void GameWidget::releaseAllTouches() {
    touchBindings_.clear();
    overlayPresses_.clear();
    releaseStick();
    input_.clearHeld();
}

void GameWidget::toggleAutoAim() {
    autoAim_ = !autoAim_;
    aimTargetId_ = -1;
    AppSettings settings = Storage::loadSettings();
#if (defined(Q_OS_WIN) || defined(Q_OS_LINUX)) && !defined(Q_OS_ANDROID)
    if (!touchUi_) {
        settings.autoAimDesktop = autoAim_;
    } else {
        settings.autoAim = autoAim_;
    }
#else
    settings.autoAim = autoAim_;
#endif
    Storage::saveSettings(settings);
    toast_ = autoAim_ ? QStringLiteral("自动索敌：开") : QStringLiteral("自动索敌：关");
    toastTime_ = 1.2f;
}

bool GameWidget::aimLockEnabled() const {
#if defined(Q_OS_ANDROID)
    return touchUi_ && autoAim_;
#elif (defined(Q_OS_WIN) || defined(Q_OS_LINUX))
    return autoAim_;
#else
    return touchUi_ && autoAim_;
#endif
}

void GameWidget::updateAimTarget() {
    constexpr float kLockRange = 160.f;
    if (!aimLockEnabled()) {
        aimTargetId_ = -1;
        return;
    }
    const Player& player = session_.player();
    const Monster* current = nullptr;
    float currentDist = 0.f;
    const Monster* nearest = nullptr;
    float nearestDist = kLockRange;
    for (const Monster& monster : session_.monsters()) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        if (monster.kind == MonsterKind::Eye && session_.ruin().arrive >= 0.f) {
            continue;
        }
        const float dist = std::hypot(monster.x - player.x, monster.y - player.y);
        if (monster.id == aimTargetId_) {
            current = &monster;
            currentDist = dist;
        }
        if (dist < nearestDist) {
            nearest = &monster;
            nearestDist = dist;
        }
    }
    // 两个敌人距离相近时不来回跳：新目标要明显更近才换
    if (current && currentDist <= kLockRange * 1.15f && (!nearest || nearestDist > currentDist * 0.8f)) {
        return;
    }
    aimTargetId_ = nearest ? nearest->id : -1;
}

const Monster* GameWidget::aimTarget() const {
    if (!aimLockEnabled() || aimTargetId_ < 0) {
        return nullptr;
    }
    for (const Monster& monster : session_.monsters()) {
        if (monster.id == aimTargetId_) {
            return monster.state == ActorState::Dead ? nullptr : &monster;
        }
    }
    return nullptr;
}

const QImage& GameWidget::touchSprite(qreal radius, const QColor& rim, const QColor& fill, const QString& label, int fontPx, const QColor& textColor) {
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(qRound(radius * 4.0)).arg(rim.rgba()).arg(fill.rgba()).arg(fontPx).arg(textColor.rgba()).arg(label);
    auto it = touchSprites_.find(key);
    if (it != touchSprites_.end()) {
        return it.value();
    }
    const qreal dpr = devicePixelRatioF();
    const qreal side = std::ceil((radius + 2.0) * 2.0);
    QImage image(QSize(int(std::ceil(side * dpr)), int(std::ceil(side * dpr))), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF center(side * 0.5, side * 0.5);
        p.setPen(rim.alpha() > 0 ? QPen(rim, 2) : QPen(Qt::NoPen));
        p.setBrush(fill);
        p.drawEllipse(center, radius, radius);
        if (!label.isEmpty()) {
            QFont font(Platform::uiFontFamily());
            font.setBold(true);
            font.setPixelSize(fontPx);
            p.setFont(font);
            p.setPen(textColor);
            p.drawText(QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0), Qt::AlignCenter, label);
        }
    }
    return touchSprites_.insert(key, image).value();
}

void GameWidget::drawTouchControls(QPainter& painter) {
    const Player& player = session_.player();
    const qreal u = touchUnit();
    const float mul = session_.cooldownMul();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // 圆底和文字预渲染成贴图：每帧重新光栅化十几个抗锯齿半透明圆在手机上要占掉大半帧时间
    auto blit = [&painter](const QImage& image, const QPointF& center) {
        const QSizeF size = image.deviceIndependentSize();
        painter.drawImage(QPointF(center.x() - size.width() * 0.5, center.y() - size.height() * 0.5), image);
    };

    const bool stickActive = stickTouchId_ >= 0;
    const QPointF base = stickActive ? stickCenter_ : stickHome();
    const qreal baseRadius = u * 0.12;
    const qreal knobRadius = u * 0.055;
    blit(touchSprite(baseRadius, QColor(228, 212, 188, stickActive ? 120 : 70), QColor(12, 10, 9, stickActive ? 110 : 70), {}, 0, {}), base);
    blit(touchSprite(knobRadius, QColor(0, 0, 0, 0), QColor(196, 92, 72, stickActive ? 200 : 120), {}, 0, {}), base + stickOffset_);

    const QList<TouchControl> held = touchBindings_.values();
    for (const TouchButton& button : touchButtons()) {
        float remain = 0.f;
        float maxCd = 0.f;
        QString sub;
        auto slot = [&](int skill, float cd) {
            if (skill == kSkillSwordQi || skill == kSkillThrust) {
                const int stacks = skill == kSkillSwordQi ? player.stacksQi : player.stacksThrust;
                sub = QString("%1/3").arg(stacks);
                if (stacks == 0) {
                    remain = skill == kSkillSwordQi ? player.cdQiStack : player.cdThrustStack;
                    maxCd = skillCooldownMax(skill, mul);
                }
            } else if (skill == kSkillFlight || skill == kSkillJetpack) {
                sub = player.flying ? QStringLiteral("开") : QStringLiteral("关");
            } else {
                remain = cd;
                maxCd = skillCooldownMax(skill, mul, player.atomicCd);
            }
        };
        switch (button.control) {
        case TouchControl::Guard:
            remain = player.cdGuard;
            maxCd = 3.6f * mul;
            break;
        case TouchControl::Heal:
            remain = player.cdHeal;
            maxCd = 5.5f * mul;
            break;
        case TouchControl::SkillR:
            slot(player.skillD, player.cdD);
            break;
        case TouchControl::SkillF:
            slot(player.skillF, player.cdF);
            break;
        case TouchControl::SkillC:
            slot(player.skillC, player.cdC);
            break;
        case TouchControl::SkillV:
            slot(player.skillV, player.cdV);
            break;
        case TouchControl::Seek:
            sub = player.seekOn ? QStringLiteral("开") : QStringLiteral("关");
            break;
        case TouchControl::Attack:
            if (player.hero == HeroClass::Robot) {
                sub = player.ammo > 0 ? QString("%1/%2").arg(player.ammo).arg(kRobotMagazine) : QStringLiteral("长按换弹");
            }
            break;
        default:
            break;
        }

        const bool pressed = held.contains(button.control) || (button.control == TouchControl::AutoAim && autoAim_)
            || (button.control == TouchControl::Seek && player.seekOn);
        const QRectF circle(button.center.x() - button.radius, button.center.y() - button.radius,
            button.radius * 2.0, button.radius * 2.0);
        const bool cooling = remain > 0.05f && maxCd > 0.01f;
        const bool withSub = !sub.isEmpty() && !cooling;
        const double labelScale = button.label.size() > 4 ? 0.34 : (button.label.size() > 2 ? 0.42 : 0.52);
        const int labelPx = std::max(9, int(button.radius * labelScale));
        const QColor rim = pressed ? QColor(236, 170, 140, 230) : QColor(150, 100, 86, 200);
        const QColor fill = pressed ? QColor(196, 92, 72, 180) : QColor(12, 10, 9, 150);
        if (cooling || withSub) {
            blit(touchSprite(button.radius, rim, fill, {}, 0, {}), button.center);
        } else {
            blit(touchSprite(button.radius, rim, fill, button.label, labelPx, QColor(240, 228, 212)), button.center);
        }

        if (button.control == TouchControl::Attack && player.heavyCharge > 0.f) {
            const float charge = std::clamp(player.heavyCharge / 0.42f, 0.f, 1.f);
            painter.setPen(QPen(QColor(255, int(120 + 100 * charge), 60, 230), 4));
            painter.setBrush(Qt::NoBrush);
            painter.drawArc(circle.adjusted(3, 3, -3, -3), 90 * 16, -int(360 * 16 * charge));
        }
        if (!cooling && !withSub) {
            continue;
        }
        if (cooling) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 150));
            painter.drawPie(circle, 90 * 16, int(360 * 16 * std::clamp(remain / maxCd, 0.f, 1.f)));
        }

        QFont font(Platform::uiFontFamily());
        font.setBold(true);
        font.setPixelSize(labelPx);
        painter.setFont(font);
        painter.setPen(cooling ? QColor(170, 158, 146) : QColor(240, 228, 212));
        const QString text = cooling ? QString::number(remain, 'f', 1) : button.label;
        painter.drawText(withSub ? circle.adjusted(0, 0, 0, -button.radius * 0.5) : circle, Qt::AlignCenter, text);
        if (withSub) {
            font.setPixelSize(std::max(8, int(button.radius * 0.34)));
            painter.setFont(font);
            painter.setPen(QColor(176, 190, 146));
            painter.drawText(circle.adjusted(0, button.radius * 1.12, 0, 0), Qt::AlignHCenter | Qt::AlignTop, sub);
        }
    }
    painter.restore();
}

void GameWidget::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    input_.clearHeld();
}

void GameWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    touchSprites_.clear();
    layoutOverlays();
}

void GameWidget::drawShadow(QPainter& painter, float x, float y) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 90));
    painter.drawEllipse(QRectF(x - 10.0, y - 4.0, 20.0, 8.0));
}

void GameWidget::drawAnim(QPainter& painter, const SpriteAnim& anim, ActorState state, float animT, float x, float y, bool flip, bool moving) {
    Q_UNUSED(moving);
    if (!anim.ok()) {
        painter.fillRect(QRectF(x - 6, y - 16, 12, 16), QColor(220, 220, 220));
        return;
    }
    const bool loop = state != ActorState::Attack && state != ActorState::Hurt && state != ActorState::Dead;
    const float fps = state == ActorState::Attack ? 18.f : state == ActorState::Run || state == ActorState::Dodge ? 12.f : 8.f;
    anim.draw(painter, frameIndex(anim, animT, loop, fps), x, y, flip);
}

void GameWidget::drawWorld(QPainter& painter) {
    const Player& player = session_.player();
    float shakeX = 0.f;
    float shakeY = 0.f;
    session_.cameraShake(shakeX, shakeY);
    const float cameraX = player.x - kViewW * 0.5f + shakeX;
    const float cameraY = player.y - kViewH * 0.5f + shakeY;
    painter.translate(-cameraX, -cameraY);

    const int x0 = tileOf(cameraX) - 1;
    const int y0 = tileOf(cameraY) - 1;
    const int x1 = tileOf(cameraX + kViewW) + 1;
    const int y1 = tileOf(cameraY + kViewH) + 1;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Tile tile = session_.map().at(x, y);
            const QRect dest(x * kTile, y * kTile, kTile, kTile);
            if (tile == Tile::MazeFloor) {
                painter.fillRect(dest, QColor(228, 206, 156));
                painter.setPen(QColor(186, 154, 104));
                painter.drawLine(dest.topLeft(), dest.topRight());
                painter.drawLine(dest.topLeft(), dest.bottomLeft());
            } else if (tile == Tile::Plaza) {
                painter.fillRect(dest, QColor(176, 72, 62));
                painter.setPen(QColor(120, 40, 38));
                painter.drawLine(dest.topLeft(), dest.bottomRight());
            } else if (tile == Tile::MazeWall) {
                painter.fillRect(dest, QColor(16, 18, 28));
            } else if (tile == Tile::Rock || tile == Tile::Bush) {
                if (tile == Tile::Rock && session_.ruin().inPlaza(x, y)) {
                    painter.fillRect(dest, QColor(176, 72, 62));
                } else {
                    drawGround(painter, sprites_.tiles, spritesOk_, Tile::Grass, x, y, session_.map());
                }
            } else {
                drawGround(painter, sprites_.tiles, spritesOk_, tile, x, y, session_.map());
            }
        }
    }
    // 墙体与树木不再统一先画一层，而是和角色一起按脚下深度排序：
    // 屏幕更靠下（y 更大）的后画，压住更靠上的目标
    auto paintMazeBlock = [&](int x, int y) {
        const QRect block(x * kTile, y * kTile - 12, kTile, kTile + 12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 10, 16, 170));
        painter.drawRect(QRect(x * kTile + 1, y * kTile + 10, kTile - 2, 5));
        painter.setBrush(QColor(22, 26, 40));
        painter.setPen(QPen(QColor(6, 8, 14), 1));
        painter.drawRect(block);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(168, 188, 214));
        painter.drawRect(QRect(x * kTile, y * kTile - 12, kTile, 4));
        painter.setBrush(QColor(70, 82, 108));
        painter.drawRect(QRect(x * kTile, y * kTile - 8, kTile, 2));
    };
    auto paintProp = [&](int x, int y, Tile tile) {
        const QRect prop(x * kTile - 8, y * kTile - 14, 32, 32);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tile == Tile::Rock ? QColor(20, 16, 16, 160) : QColor(12, 36, 12, 150));
        painter.drawEllipse(QRectF(x * kTile + 1, y * kTile + 6, 14, 8));
        if (spritesOk_) {
            const QRect source = tile == Tile::Rock ? QRect(112, 48, 32, 32) : QRect(64, 48, 32, 32);
            painter.drawImage(prop, sprites_.tiles, source);
        } else {
            painter.setBrush(tile == Tile::Rock ? QColor(40, 40, 44) : QColor(30, 110, 40));
            painter.drawEllipse(prop);
        }
    };

    if (const MazeRuin& ruin = session_.ruin(); ruin.active) {
        int outX = 0;
        int outY = 0;
        if (ruin.entranceX == 0) {
            outX = -1;
        } else if (ruin.entranceX == MazeRuin::kSize - 1) {
            outX = 1;
        } else if (ruin.entranceY == 0) {
            outY = -1;
        } else {
            outY = 1;
        }
        const int doorX = ruin.originX + ruin.entranceX;
        const int doorY = ruin.originY + ruin.entranceY;
        auto paintApproach = [&](int tx, int ty, const QColor& fill) {
            const QRect dest(tx * kTile, ty * kTile, kTile, kTile);
            painter.fillRect(dest, fill);
            painter.setPen(QColor(150, 110, 60));
            painter.drawLine(dest.topLeft(), dest.topRight());
            painter.drawLine(dest.topLeft(), dest.bottomLeft());
        };
        for (int step = 1; step <= 4; ++step) {
            paintApproach(doorX + outX * step, doorY + outY * step, QColor(214, 176, 104));
        }
        paintApproach(doorX, doorY, QColor(255, 228, 150));
        const int sideX = outY;
        const int sideY = -outX;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 196, 70));
        for (int side = -1; side <= 1; side += 2) {
            const float px = (doorX + outX + sideX * side) * kTile + kTile * 0.5f;
            const float py = (doorY + outY + sideY * side) * kTile + kTile * 0.5f;
            painter.drawRect(QRectF(px - 2.f, py - 8.f, 4.f, 12.f));
            painter.drawEllipse(QRectF(px - 3.f, py - 11.f, 6.f, 6.f));
        }
    }

    if (const Monster* target = aimTarget()) {
        // 画在角色之前，落在脚下不挡贴图
        const float pulse = 0.5f + 0.5f * std::sin(player.animT * 8.f);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 90, 60, int(140 + 100 * pulse)), 1));
        painter.drawEllipse(QRectF(target->x - 11.f, target->y - 5.f, 22.f, 10.f));
    }

    // 深度取脚下位置：墙体/树木用所在格子的顶边，角色用脚底 y。
    // y 越大越靠近镜头、越后画，于是下方的物体遮住上方的物体；
    // 同深度时角色后画，避免站在墙边被相邻墙体切掉
    struct DrawItem {
        enum class Kind { Block, Prop, Player, Monster };
        float y;
        Kind kind;
        const Monster* monster = nullptr;
        int tx = 0;
        int ty = 0;
        Tile tile = Tile::Grass;
        // 角色已经高过岩石 / 灌木，这类可翻越的地表物不再挡住它
        bool overProp = false;
    };
    // 岩石 / 灌木踩得上去、也飞得过去，遮不遮人按角色高低判断：
    // 角色离地抬升够高，或脚下正踩着这类地表物（落在障碍上）时都算比它高。
    // 迷宫墙翻不过去，下面排序时遇到墙就停，无论角色多高都照样遮挡。
    constexpr float kPropOccludeHeight = 10.f;
    auto onProp = [&](float x, float y) {
        const Tile tile = session_.map().at(tileOf(x), tileOf(y));
        return tile == Tile::Rock || tile == Tile::Bush;
    };
    const float playerLift = (player.jumpT > 0.f ? std::sin(player.jumpT / 0.34f * 3.14159f) * 14.f : 0.f)
        + (player.flying ? 12.f : 0.f);
    auto monsterLift = [](const Monster& monster) {
        return monster.kind == MonsterKind::Flyer ? 12.f : monster.kind == MonsterKind::Eye ? 8.f : 0.f;
    };
    auto solidRank = [](DrawItem::Kind kind) { return kind == DrawItem::Kind::Player || kind == DrawItem::Kind::Monster ? 1 : 0; };
    std::vector<DrawItem> items;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Tile tile = session_.map().at(x, y);
            if (tile == Tile::MazeWall) {
                items.push_back({float(y * kTile), DrawItem::Kind::Block, nullptr, x, y, tile});
            } else if (tile == Tile::Rock || tile == Tile::Bush) {
                items.push_back({float(y * kTile), DrawItem::Kind::Prop, nullptr, x, y, tile});
            }
        }
    }
    items.push_back({player.y, DrawItem::Kind::Player, nullptr, 0, 0, Tile::Grass,
        playerLift >= kPropOccludeHeight || onProp(player.x, player.y)});
    for (const Monster& monster : session_.monsters()) {
        items.push_back({monster.y, DrawItem::Kind::Monster, &monster, 0, 0, Tile::Grass,
            monsterLift(monster) >= kPropOccludeHeight || onProp(monster.x, monster.y)});
    }
    std::stable_sort(items.begin(), items.end(), [&](const DrawItem& a, const DrawItem& b) {
        if (a.y != b.y) {
            return a.y < b.y;
        }
        return solidRank(a.kind) < solidRank(b.kind);
    });
    // 比岩石 / 灌木高的角色：从深度顺序里往后挪，压住它后面那一串地表物，
    // 撞上迷宫墙就停下（墙不可翻越，一律遮挡）。从后往前处理，挪完的角色不会再被处理一次。
    for (std::size_t i = items.size(); i-- > 0;) {
        if (!items[i].overProp) {
            continue;
        }
        std::size_t j = i;
        while (j + 1 < items.size() && items[j + 1].kind != DrawItem::Kind::Block) {
            ++j;
        }
        if (j > i) {
            std::rotate(items.begin() + i, items.begin() + i + 1, items.begin() + j + 1);
        }
    }

    for (const DrawItem& item : items) {
        if (item.kind == DrawItem::Kind::Block) {
            paintMazeBlock(item.tx, item.ty);
            continue;
        }
        if (item.kind == DrawItem::Kind::Prop) {
            paintProp(item.tx, item.ty, item.tile);
            continue;
        }
        if (item.kind == DrawItem::Kind::Player) {
            drawShadow(painter, player.x, player.y);
            if (player.invuln > 0.f && int(player.animT * 24.f) % 2 == 0 && player.state != ActorState::Dead) {
                continue;
            }
            struct HeroAnims {
                const SpriteAnim* idle;
                const SpriteAnim* run;
                const SpriteAnim* attack;
                const SpriteAnim* hurt;
                const SpriteAnim* death;
            };
            HeroAnims set{&sprites_.warriorIdle, &sprites_.warriorRun, &sprites_.warriorAttack, &sprites_.warriorHurt, &sprites_.warriorDeath};
            if (player.hero == HeroClass::Sword) {
                set = {&sprites_.swordIdle, &sprites_.swordRun, &sprites_.swordAttack, &sprites_.swordHurt, &sprites_.swordDeath};
            } else if (player.hero == HeroClass::Mage) {
                set = {&sprites_.mageIdle, &sprites_.mageRun, &sprites_.mageAttack, &sprites_.mageHurt, &sprites_.mageDeath};
            } else if (player.hero == HeroClass::Robot) {
                set = {&sprites_.robotIdle, &sprites_.robotRun, &sprites_.robotAttack, &sprites_.robotHurt, &sprites_.robotDeath};
            }
            const SpriteAnim* anim = set.idle;
            if (player.state == ActorState::Dead) {
                anim = set.death;
            } else if (player.state == ActorState::Attack) {
                anim = set.attack;
            } else if (player.state == ActorState::Hurt) {
                anim = set.hurt;
            } else if (player.state == ActorState::Run || player.state == ActorState::Dodge) {
                anim = set.run;
            }
            const float lift = playerLift;
            const float guardCx = player.x;
            const float guardCy = player.y - kGuardCenterAboveFoot - lift;
            if (player.burialStage > 0) {
                const float cx = player.x;
                const float cy = player.y;
                const float spin = player.animT * 2.8f;
                if (player.burialStage == 1) {
                    // 第一段：法阵铺开，0.25 秒后才出伤
                    const float p = std::clamp(1.f - player.burialT / kBurialStage1Life, 0.f, 1.f);
                    const float R = player.burialR * (0.55f + 0.45f * std::min(1.f, p * 1.4f));
                    const float fade = std::min(1.f, player.burialT / 0.18f);
                    if (p < 0.3f) {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(QColor(170, 60, 255, int(90.f * (1.f - p / 0.3f))));
                        painter.drawEllipse(QRectF(cx - R, cy - R, R * 2.f, R * 2.f));
                    }
                    painter.setBrush(Qt::NoBrush);
                    for (int ring = 1; ring <= 2; ++ring) {
                        const float rr = R * (0.35f + 0.22f * ring);
                        painter.setPen(QPen(QColor(160, 40, 220, int((90 + ring * 30) * fade)), 1));
                        painter.drawEllipse(QRectF(cx - rr, cy - rr, rr * 2.f, rr * 2.f));
                    }
                    const float rOuter = R * 0.99f;
                    strokeGlow(painter, QColor(200, 90, 255, int(210 * fade)), 1.5f, [&] {
                        painter.drawEllipse(QRectF(cx - rOuter, cy - rOuter, rOuter * 2.f, rOuter * 2.f));
                    });
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(230, 190, 255, int(220 * fade)));
                    for (int i = 0; i < 16; ++i) {
                        const float a = -spin * 0.8f + i * kPi / 8.f;
                        const float rx = cx + std::cos(a) * R * 0.89f;
                        const float ry = cy + std::sin(a) * R * 0.89f;
                        const float s = 2.6f;
                        const float ca = std::cos(a + kPi * 0.25f) * s;
                        const float sa = std::sin(a + kPi * 0.25f) * s;
                        const QPointF rune[4] = {QPointF(rx + ca, ry + sa), QPointF(rx - sa, ry + ca), QPointF(rx - ca, ry - sa), QPointF(rx + sa, ry - ca)};
                        painter.drawPolygon(rune, 4);
                    }
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(QColor(120, 30, 180, int(70 * fade)), 1));
                    painter.drawEllipse(QRectF(cx - player.burialR, cy - player.burialR, player.burialR * 2.f, player.burialR * 2.f));
                } else {
                    // 第二段：法阵收势后六芒星落下，出伤处炸开白光并散去
                    const float p = std::clamp(1.f - player.burialT / kBurialStage2Life, 0.f, 1.f);
                    constexpr float hitP = kBurialStage2Hit / kBurialStage2Life;
                    const bool landed = player.burialNext <= 0.f;
                    const float k = landed ? std::min(1.f, (p - hitP) / (1.f - hitP)) : std::min(1.f, p / hitP);
                    const float fade = landed ? 1.f - k : 1.f;
                    const float starR = player.burialR * (landed ? 0.8f + 0.35f * k : 0.98f - 0.18f * k);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(170, 60, 255, int(70 * fade)));
                    painter.drawEllipse(QRectF(cx - player.burialR, cy - player.burialR, player.burialR * 2.f, player.burialR * 2.f));
                    painter.setBrush(Qt::NoBrush);
                    QPointF star[6];
                    for (int i = 0; i < 6; ++i) {
                        const float a = spin * 0.5f + i * kPi / 3.f;
                        star[i] = QPointF(cx + std::cos(a) * starR, cy + std::sin(a) * starR);
                    }
                    strokeGlow(painter, QColor(255, 110, 230, int((landed ? 220.f : 130.f + 90.f * k) * fade)),
                        landed ? 4.f - 2.4f * k : 1.6f + 1.4f * k, [&] {
                            for (int i = 0; i < 6; ++i) {
                                painter.drawLine(star[i], star[(i + 2) % 6]);
                            }
                        });
                    if (landed) {
                        const float flare = std::max(0.f, 1.f - (p - hitP) / 0.18f);
                        if (flare > 0.f) {
                            painter.setPen(Qt::NoPen);
                            painter.setBrush(QColor(255, 236, 255, int(190.f * flare)));
                            painter.drawEllipse(QRectF(cx - player.burialR * 0.5f, cy - player.burialR * 0.5f, player.burialR, player.burialR));
                        }
                        const float shock = player.burialR * (0.75f + 0.45f * k);
                        strokeGlow(painter, QColor(255, 130, 235, int(200.f * (1.f - k))), 2.f, [&] {
                            painter.drawEllipse(QRectF(cx - shock, cy - shock, shock * 2.f, shock * 2.f));
                        });
                    }
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(QColor(120, 30, 180, int(70 * fade)), 1));
                    painter.drawEllipse(QRectF(cx - player.burialR, cy - player.burialR, player.burialR * 2.f, player.burialR * 2.f));
                }
            }
            if (anim->ok()) {
                const bool loop = player.state != ActorState::Attack && player.state != ActorState::Hurt && player.state != ActorState::Dead;
                const float fps = player.state == ActorState::Attack ? 18.f : 12.f;
                const int dir = anim->dirs() >= 4 ? facingDir(player.facingX, player.facingY, anim->dirs()) : 0;
                const bool flip = anim->dirs() < 4 && player.facingX < 0.f;
                const float heroScale = anim->dirs() >= 8 ? 1.f : 1.25f;
                // 机甲人贴图整体下沉 5% 帧高，枪口高度见 Session 的 kRobotMuzzleLift
                const float sink = player.hero == HeroClass::Robot ? anim->size() * 0.05f : 0.f;
                anim->draw(painter, frameIndex(*anim, player.animT, loop, fps), player.x, player.y + sink, flip, heroScale, lift, QColor(), dir);
                if (player.hero == HeroClass::Robot && player.flying) {
                    const float footY = player.y + sink - lift;
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(255, 150, 50, 50));
                    painter.drawEllipse(QRectF(player.x - 9.f, player.y - 3.f, 18.f, 6.f));
                    for (int side = -1; side <= 1; side += 2) {
                        const float fx = player.x + float(side) * 4.f;
                        const float len = 8.f + 5.f * (0.5f + 0.5f * std::sin(player.animT * 40.f + float(side) * 1.7f));
                        const float top = footY - 4.f;
                        QPolygonF outer;
                        outer << QPointF(fx - 4.f, top) << QPointF(fx + 4.f, top) << QPointF(fx + 1.5f, top + len * 0.7f) << QPointF(fx, top + len)
                              << QPointF(fx - 1.5f, top + len * 0.7f);
                        painter.setBrush(QColor(255, 120, 30, 210));
                        painter.drawPolygon(outer);
                        QPolygonF inner;
                        inner << QPointF(fx - 2.f, top) << QPointF(fx + 2.f, top) << QPointF(fx, top + len * 0.6f);
                        painter.setBrush(QColor(255, 240, 170, 235));
                        painter.drawPolygon(inner);
                    }
                }
                // 头顶昵称（各职业同一逻辑）
                // 机甲人朝上举枪时枪管会顶到默认高度的昵称
                const float nameTop = player.y + sink - lift - (player.hero == HeroClass::Robot ? 50.f : 40.f * heroScale);
                QFont nameFont(Platform::uiFontFamily(), 8);
                nameFont.setBold(true);
                painter.setFont(nameFont);
                const QFontMetrics fm(nameFont);
                const QString nick = QStringLiteral("玩家");
                const int tw = fm.horizontalAdvance(nick);
                const QRectF nameBox(player.x - tw * 0.5f - 3.f, nameTop - fm.height() - 1.f, tw + 6.f, fm.height() + 2.f);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(8, 6, 5, 150));
                painter.drawRoundedRect(nameBox, 2, 2);
                painter.setPen(QColor(242, 230, 216));
                painter.drawText(nameBox, Qt::AlignCenter, nick);
            }
            if (player.guardT > 0.f) {
                painter.setPen(QPen(QColor(64, 148, 255, 220), 2));
                painter.setBrush(QColor(64, 148, 255, 36));
                painter.drawEllipse(QRectF(guardCx - kGuardR, guardCy - kGuardR, kGuardR * 2.f, kGuardR * 2.f));
            }
            if (player.mirrorT > 0.f) {
                const float pulse = 0.5f + 0.5f * std::sin(player.animT * 10.f);
                painter.setPen(QPen(QColor(180, 60, 255, int(140 + 80 * pulse)), 2));
                painter.setBrush(QColor(120, 40, 200, int(28 + 20 * pulse)));
                painter.drawEllipse(QRectF(guardCx - 18, guardCy - 18, 36, 36));
                painter.setPen(QPen(QColor(255, 120, 255, 160), 1));
                painter.drawEllipse(QRectF(guardCx - 12, guardCy - 12, 24, 24));
            }
            if (player.overloadT > 0.f) {
                const float pulse = 0.5f + 0.5f * std::sin(player.animT * 14.f);
                painter.setPen(QPen(QColor(255, 140, 40, int(120 + 100 * pulse)), 2));
                painter.setBrush(QColor(255, 90, 20, int(30 + 30 * pulse)));
                painter.drawEllipse(QRectF(player.x - 16.f, player.y - 5.f, 32.f, 10.f));
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 200, 80, 220));
                for (int i = 0; i < 4; ++i) {
                    const float t = std::fmod(player.animT * 1.4f + float(i) * 0.25f, 1.f);
                    const float sx = player.x + std::sin(float(i) * 2.1f + player.animT * 3.f) * 11.f;
                    painter.drawRect(QRectF(sx - 1.f, player.y - 4.f - t * 30.f, 2.f, 2.f));
                }
            }
            if (player.fieldT > 0.f && (player.fieldT > 1.f || int(player.fieldT * 10.f) % 2 == 0)) {
                constexpr float kFieldR = 28.f;
                const float spin = player.animT * 240.f;
                painter.setPen(QPen(QColor(90, 200, 255, 200), 2));
                painter.setBrush(QColor(60, 150, 255, 40));
                painter.drawEllipse(QRectF(guardCx - kFieldR, guardCy - kFieldR, kFieldR * 2.f, kFieldR * 2.f));
                painter.setPen(QPen(QColor(200, 245, 255, 230), 2));
                painter.setBrush(Qt::NoBrush);
                const QRectF arcBox(guardCx - kFieldR + 3.f, guardCy - kFieldR + 3.f, kFieldR * 2.f - 6.f, kFieldR * 2.f - 6.f);
                for (int i = 0; i < 3; ++i) {
                    painter.drawArc(arcBox, int((spin + i * 120.f) * 16.f), 40 * 16);
                }
            }
            if (player.medkitT > 0.f) {
                const float rise = std::fmod(player.animT * 1.2f, 1.f);
                const float cy = guardCy - 24.f - rise * 12.f;
                const int alpha = int(230 * (1.f - rise));
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(90, 230, 110, alpha));
                painter.drawRect(QRectF(guardCx - 1.5f, cy - 5.f, 3.f, 10.f));
                painter.drawRect(QRectF(guardCx - 5.f, cy - 1.5f, 10.f, 3.f));
                painter.setPen(QPen(QColor(90, 230, 110, 120), 1));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QRectF(player.x - 14.f, player.y - 4.f, 28.f, 8.f));
            }
            drawFacingMarker(painter, guardCx, guardCy, player.facingX, player.facingY, kGuardR, attackMarkerColor(player));
        } else {
            const Monster& monster = *item.monster;
            if (monster.kind == MonsterKind::Eye && session_.ruin().arrive == 0.f) {
                continue;
            }
            drawShadow(painter, monster.x, monster.y);
            const SpriteAnim* anim = &sprites_.slimeIdle;
            float scale = 1.f;
            float lift = 0.f;
            bool custom = false;
            if (monster.kind == MonsterKind::Eye) {
                custom = true;
                const MazeRuin& ruin = session_.ruin();
                const float bob = monster.state == ActorState::Dead ? 0.f : std::sin(monster.animT * 1.6f) * 3.f;
                float fade = monster.state == ActorState::Dead ? 0.35f : 1.f;
                float descend = 0.f;
                if (ruin.arrive > 0.f) {
                    const float u = 1.f - std::clamp(ruin.arrive / 1.7f, 0.f, 1.f);
                    const float eased = u * u * (3.f - 2.f * u);
                    descend = (1.f - eased) * 120.f;
                    fade *= 0.2f + 0.8f * eased;
                    painter.setPen(QPen(QColor(120, 12, 18, int(160 * (1.f - eased))), 3));
                    painter.drawLine(QPointF(monster.x, monster.y - 170.f), QPointF(monster.x, monster.y - 20.f));
                    painter.setBrush(Qt::NoBrush);
                    const float ring = 16.f + (1.f - eased) * 74.f;
                    painter.setPen(QPen(QColor(150, 24, 30, int(200 * eased)), 2));
                    painter.drawEllipse(QRectF(monster.x - ring, monster.y - ring * 0.32f, ring * 2.f, ring * 0.64f));
                }
                const float rx = 42.f;
                const float ry = 30.f;
                const float cy = monster.y - 30.f + bob - descend;
                const bool flash = monster.hurtT > 0.1f;
                const bool broken = monster.stunT > 0.f;
                float gazeX = monster.facingX;
                float gazeY = monster.facingY;
                if (ruin.arrive > 0.f) {
                    gazeX = 0.f;
                    gazeY = 0.7f;
                } else if (broken) {
                    gazeX *= 0.15f;
                    gazeY = 0.45f;
                }
                const float eyeX = monster.x + gazeX * 12.f;
                const float eyeY = cy + gazeY * 8.f;
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(28, 0, 6, int(90 * fade)));
                painter.drawEllipse(QRectF(monster.x - 58.f, cy - 44.f, 116.f, 88.f));
                painter.setPen(QPen(QColor(48, 6, 10, int(230 * fade)), 3));
                painter.setBrush(flash ? QColor(255, 236, 220, int(235 * fade)) : QColor(214, 186, 164, int(240 * fade)));
                painter.drawEllipse(QRectF(monster.x - rx, cy - ry, rx * 2.f, ry * 2.f));
                painter.setPen(QPen(QColor(120, 16, 22, int(170 * fade)), 1.4f));
                painter.drawLine(QPointF(monster.x - 34.f, cy - 6.f), QPointF(monster.x - 16.f, cy + 2.f));
                painter.drawLine(QPointF(monster.x - 30.f, cy + 8.f), QPointF(monster.x - 14.f, cy + 4.f));
                painter.drawLine(QPointF(monster.x + 34.f, cy - 6.f), QPointF(monster.x + 16.f, cy + 2.f));
                painter.drawLine(QPointF(monster.x + 30.f, cy + 8.f), QPointF(monster.x + 14.f, cy + 4.f));
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(110, 8, 16, int((broken ? 140 : 245) * fade)));
                painter.drawEllipse(QRectF(eyeX - 16.f, eyeY - 16.f, 32.f, 32.f));
                painter.setBrush(QColor(12, 0, 4, int(250 * fade)));
                painter.drawRoundedRect(QRectF(eyeX - 2.6f, eyeY - 13.f, 5.2f, broken ? 16.f : 26.f), 2.2, 2.2);
                if (broken) {
                    painter.setPen(QPen(QColor(40, 8, 10, int(200 * fade)), 2));
                    painter.drawLine(QPointF(monster.x - 28.f, cy - 4.f), QPointF(monster.x + 28.f, cy + 2.f));
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(180, 20, 28, int(200 * fade)));
                painter.drawEllipse(QRectF(monster.x - 46.f, cy + 8.f, 10.f, 7.f));
                painter.drawEllipse(QRectF(monster.x + 36.f, cy + 10.f, 8.f, 6.f));
                const float barW = 118.f;
                const float barH = 9.f;
                const float barX = monster.x - barW * 0.5f;
                const float barY = cy - ry - 28.f;
                const QRectF plate(barX - 8.f, barY - 20.f, barW + 16.f, 18.f);
                painter.setPen(QPen(QColor(168, 36, 32, int(230 * fade)), 1));
                painter.setBrush(QColor(28, 6, 8, int(220 * fade)));
                painter.drawRoundedRect(plate, 3, 3);
                painter.setFont(QFont(Platform::uiFontFamily(), 11, QFont::Bold));
                painter.setPen(QColor(0, 0, 0, int(210 * fade)));
                painter.drawText(plate.adjusted(0, 1, -36, 1), Qt::AlignCenter, QStringLiteral("克苏鲁之眼"));
                painter.setPen(QColor(255, 196, 150, int(255 * fade)));
                painter.drawText(plate.adjusted(0, 0, -36, 0), Qt::AlignCenter, QStringLiteral("克苏鲁之眼"));
                const QRectF tag(plate.right() - 34.f, plate.y() + 2.f, 30.f, 14.f);
                painter.setPen(QPen(QColor(92, 28, 32, int(220 * fade)), 1));
                painter.setBrush(QColor(18, 6, 8, int(230 * fade)));
                painter.drawRoundedRect(tag, 2, 2);
                painter.setFont(QFont(Platform::uiFontFamily(), 8, QFont::Bold));
                painter.setPen(QColor(176, 96, 88, int(255 * fade)));
                painter.drawText(tag, Qt::AlignCenter, QStringLiteral("投影"));
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 220));
                painter.drawRoundedRect(QRectF(barX - 2, barY - 2, barW + 4, barH + 4), 2, 2);
                painter.setBrush(QColor(48, 10, 12));
                painter.drawRect(QRectF(barX, barY, barW, barH));
                const float ratio = monster.maxHp <= 0.f ? 0.f : std::clamp(monster.hp / monster.maxHp, 0.f, 1.f);
                painter.setBrush(QColor(210, 28, 32));
                painter.drawRect(QRectF(barX, barY, barW * ratio, barH));
                if (monster.maxShield > 0.f) {
                    painter.setBrush(QColor(150, 196, 230));
                    painter.drawRect(QRectF(barX, barY - 4, barW * std::clamp(monster.shield / monster.maxShield, 0.f, 1.f), 3));
                }
                const float poiseRatio = monster.maxPoise <= 0.f ? 0.f : std::clamp(monster.poise / monster.maxPoise, 0.f, 1.f);
                painter.setBrush(QColor(24, 16, 8));
                painter.drawRect(QRectF(barX, barY + barH + 2, barW, 4));
                if (broken) {
                    painter.setBrush(QColor(90, 42, 28));
                    painter.drawRect(QRectF(barX, barY + barH + 2, barW * std::clamp(monster.stunT / 1.6f, 0.f, 1.f), 4));
                } else {
                    painter.setBrush(QColor(196, 148, 48));
                    painter.drawRect(QRectF(barX, barY + barH + 2, barW * poiseRatio, 4));
                }
                painter.setFont(QFont(Platform::uiFontFamily(), 8, QFont::Bold));
                const int hpShown = monster.hp <= 0.f ? 0 : int(std::ceil(monster.hp));
                const QString hpText = QString::number(hpShown);
                painter.setPen(QColor(255, 236, 220));
                painter.drawText(QRectF(barX, barY - 1, barW, barH + 2), Qt::AlignCenter, hpText);
                lift = 8.f;
            } else if (monster.kind == MonsterKind::Slime) {
                anim = monster.state == ActorState::Dead ? &sprites_.slimeDeath : monster.state == ActorState::Run ? &sprites_.slimeWalk : &sprites_.slimeIdle;
            } else if (monster.kind == MonsterKind::Mushroom) {
                scale = 1.25f;
                if (monster.state == ActorState::Dead) {
                    anim = &sprites_.mushroomDeath;
                } else if (monster.state == ActorState::Attack) {
                    anim = &sprites_.mushroomJump;
                } else {
                    anim = &sprites_.mushroomIdle;
                }
            } else if (monster.kind == MonsterKind::Caster) {
                scale = 1.15f;
                if (monster.state == ActorState::Dead) {
                    anim = &sprites_.mushroomDeath;
                } else if (monster.state == ActorState::Attack) {
                    anim = &sprites_.mushroomJump;
                } else {
                    anim = &sprites_.mushroomIdle;
                }
            } else if (monster.kind == MonsterKind::Killbot) {
                scale = 1.2f;
                if (monster.state == ActorState::Dead) {
                    anim = &sprites_.killbotDeath;
                } else if (monster.state == ActorState::Attack) {
                    anim = &sprites_.killbotAttack;
                } else {
                    anim = &sprites_.killbotWalk;
                }
            } else if (monster.kind == MonsterKind::Flyer) {
                lift = 12.f;
                if (monster.state == ActorState::Dead) {
                    anim = &sprites_.flyerDeath;
                } else if (monster.state == ActorState::Attack) {
                    anim = &sprites_.flyerAttack;
                } else if (monster.state == ActorState::Hurt) {
                    anim = &sprites_.flyerHurt;
                } else if (monster.state == ActorState::Run) {
                    anim = &sprites_.flyerFly;
                } else {
                    anim = &sprites_.flyerIdle;
                }
            } else if (monster.state == ActorState::Dead) {
                anim = &sprites_.skeletonDeath;
            } else if (monster.defenseT > 0.f) {
                anim = &sprites_.skeletonDefense;
            } else if (monster.state == ActorState::Attack) {
                anim = &sprites_.skeletonAttack;
            } else if (monster.state == ActorState::Hurt) {
                anim = &sprites_.skeletonHurt;
            } else if (monster.state == ActorState::Run) {
                anim = &sprites_.skeletonWalk;
            } else {
                anim = &sprites_.skeletonIdle;
            }
            if (monster.elite && monster.state != ActorState::Dead) {
                // 精英：脚下一圈紫色余光，便于在杂兵里认出来
                painter.setPen(QPen(QColor(158, 66, 210, 180), 1.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QRectF(monster.x - 15.f, monster.y - 7.f, 30.f, 12.f));
                painter.setPen(Qt::NoPen);
            }
            if (!custom && anim->ok()) {
                const bool loop = monster.state != ActorState::Attack && monster.state != ActorState::Hurt && monster.state != ActorState::Dead;
                QColor tint = monster.kind == MonsterKind::Caster ? QColor(88, 42, 112) : QColor();
                if (monster.elite) {
                    tint = QColor(132, 62, 196);
                }
                if (monster.hurtT > 0.1f) {
                    tint = QColor(255, 220, 80);  // 暴击/重创闪白黄
                }
                const int dir = anim->dirs() >= 4 ? facingDir(monster.facingX, monster.facingY, anim->dirs()) : 0;
                const bool flip = anim->dirs() < 4 && monster.flip;
                anim->draw(painter, frameIndex(*anim, monster.animT, loop, 10.f), monster.x, monster.y, flip, scale, lift, tint, dir);
            }
            if (monster.kind == MonsterKind::Eye) {
                continue;
            }
            // 清晰血条：黑底描边 + 亮红 + 等级
            const float barW = 30.f;
            const float barH = 5.f;
            const float barY = monster.y - 40 - lift;
            const float barX = monster.x - barW * 0.5f;
            painter.fillRect(QRectF(barX - 1, barY - 1, barW + 2, barH + 2), QColor(0, 0, 0, 220));
            painter.fillRect(QRectF(barX, barY, barW, barH), QColor(40, 12, 12));
            const float ratio = monster.maxHp <= 0.f ? 0.f : std::clamp(monster.hp / monster.maxHp, 0.f, 1.f);
            painter.fillRect(QRectF(barX, barY, barW * ratio, barH), QColor(230, 48, 42));
            if (monster.maxShield > 0.f) {
                painter.fillRect(QRectF(barX, barY - 3, barW * std::clamp(monster.shield / monster.maxShield, 0.f, 1.f), 2), QColor(140, 180, 220));
            }
            painter.fillRect(QRectF(barX, barY + barH + 1, barW * std::clamp(monster.poise / std::max(1.f, monster.maxPoise), 0.f, 1.f), 2), QColor(210, 170, 70));
            painter.setFont(QFont(Platform::uiFontFamily(), 8, QFont::Bold));
            const int hpShown = monster.hp <= 0.f ? 0 : int(std::ceil(monster.hp));
            const QString label = monster.elite
                ? QString("精英 Lv%1  %2").arg(monster.level).arg(hpShown)
                : QString("Lv%1  %2").arg(monster.level).arg(hpShown);
            painter.setPen(QColor(0, 0, 0, 200));
            painter.drawText(QRectF(barX - 10, barY - 13, barW + 20, 12), Qt::AlignCenter, label);
            painter.setPen(QColor(255, 240, 220));
            painter.drawText(QRectF(barX - 10, barY - 14, barW + 20, 12), Qt::AlignCenter, label);
        }
    }
    if (session_.ruin().active && session_.ruin().pulseR >= 0.f) {
        const float r = session_.ruin().pulseR;
        const float cx = session_.ruin().centerX();
        const float cy = session_.ruin().centerY();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(170, 24, 32, 200), 2));
        painter.drawEllipse(QRectF(cx - r, cy - r, r * 2.f, r * 2.f));
        painter.setPen(QPen(QColor(255, 80, 70, 120), 1));
        painter.drawEllipse(QRectF(cx - r * 0.82f, cy - r * 0.82f, r * 1.64f, r * 1.64f));
    }
    painter.setPen(Qt::NoPen);
    for (const AttackFx& fx : session_.attackFx()) {
        const float u = std::clamp(fx.life / std::max(0.01f, fx.maxLife), 0.f, 1.f);
        const float grow = 1.f - u;
        if (fx.kind == AttackFxKind::Ring || fx.kind == AttackFxKind::Pulse) {
            const bool pulse = fx.kind == AttackFxKind::Pulse;
            const QColor base = fxColor(fx, QColor(255, 210, 80));
            const float r = fx.radius * (pulse ? (0.35f + 0.65f * grow) : (0.85f + 0.15f * grow));
            // 判定范围仍是俯视圆；脚下再铺一圈压扁的冲击波作地面透视
            const float gr = fx.radius * (0.3f + 0.8f * grow);
            const QRectF ground(fx.x - gr, fx.y + 8.f - gr * 0.45f, gr * 2.f, gr * 0.9f);
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(base, int(70 * u)));
            painter.drawEllipse(ground);
            strokeGlow(painter, withAlpha(base, int(200 * u)), 1.5f, [&] { painter.drawEllipse(ground); });
            if (pulse) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(withAlpha(base, int(36 * u)));
                painter.drawEllipse(QRectF(fx.x - r, fx.y - r, r * 2.f, r * 2.f));
            }
            strokeGlow(painter, withAlpha(base, int(60 + 160 * u)), 2.f, [&] {
                painter.drawEllipse(QRectF(fx.x - r, fx.y - r, r * 2.f, r * 2.f));
            });
            painter.setPen(QPen(QColor(255, 255, 220, int(40 + 100 * u)), 1));
            painter.drawEllipse(QRectF(fx.x - r * 0.7f, fx.y - r * 0.7f, r * 1.4f, r * 1.4f));
        } else if (fx.kind == AttackFxKind::Spin) {
            // 回旋斩：一段旋转的刀环，脚下再压扁画一遍当地面透视
            const float cx = player.x;
            const float cy = player.y - 10.f;
            const float r = fx.radius * (0.6f + 0.4f * grow);
            const float head = grow * kPi * 3.f;
            const QColor base = fxColor(fx, QColor(255, 224, 194));
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(base, int(34 * u)));
            painter.drawEllipse(QRectF(cx - fx.radius * 0.9f, cy - fx.radius * 0.9f, fx.radius * 1.8f, fx.radius * 1.8f));
            painter.save();
            painter.translate(cx, cy);
            painter.scale(1.f, 0.6f);
            const QPainterPath ground = arcPath(0.f, 0.f, r, head - kPi * 1.4f, head);
            strokeGlow(painter, withAlpha(base, int(150 * u)), 3.f, [&] { painter.drawPath(ground); });
            painter.restore();
            const QPainterPath blade = arcPath(cx, cy, r, head - kPi * 1.4f, head);
            strokeGlow(painter, withAlpha(base, int(230 * u)), 5.f * u + 1.f, [&] { painter.drawPath(blade); });
            painter.setPen(QPen(withAlpha(QColor(255, 255, 255), int(160 * u)), 1.2f, Qt::SolidLine, Qt::RoundCap));
            painter.drawPath(arcPath(cx, cy, r * 0.72f, head - kPi * 1.1f, head));
        } else if (fx.kind == AttackFxKind::Qi) {
            // 剑气：判定是释放瞬间的一整片扇形，所以按同一张角画一道推满射程的大月牙，并提前散去
            float nx = 1.f;
            float ny = 0.f;
            fxDir(fx, nx, ny);
            const float ang = std::atan2(ny, nx);
            const float half = fx.halfAngle;
            // 波前很快推到满射程，之后只剩余光
            const float front = fx.radius * std::min(1.f, grow / 0.35f);
            const float thick = std::clamp(front * 0.3f, 12.f, 34.f);
            const float inner = std::max(6.f, front - thick);
            const float fade = std::clamp((1.f - grow) / 0.4f, 0.f, 1.f);
            const QColor base = fxColor(fx, QColor(191, 224, 255));
            constexpr int kSegs = 20;
            QPainterPath band;
            for (int i = 0; i <= kSegs; ++i) {
                const float a = ang - half + (half * 2.f) * float(i) / float(kSegs);
                const QPointF p(fx.x + std::cos(a) * front, fx.y + std::sin(a) * front);
                if (i == 0) {
                    band.moveTo(p);
                } else {
                    band.lineTo(p);
                }
            }
            for (int i = kSegs; i >= 0; --i) {
                const float a = ang - half + (half * 2.f) * float(i) / float(kSegs);
                band.lineTo(fx.x + std::cos(a) * inner, fx.y + std::sin(a) * inner);
            }
            band.closeSubpath();
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(base, int(72.f * fade)));
            painter.drawPath(band);
            strokeGlow(painter, withAlpha(base, int(230.f * fade)), 3.6f, [&] {
                painter.drawPath(arcPath(fx.x, fx.y, front, ang - half, ang + half));
            });
            painter.setPen(QPen(withAlpha(QColor(255, 255, 255), int(210.f * fade)), 1.4f, Qt::SolidLine, Qt::RoundCap));
            painter.drawPath(arcPath(fx.x, fx.y, inner, ang - half * 0.92f, ang + half * 0.92f));
        } else if (fx.kind == AttackFxKind::Lunge) {
            // 突刺：人在位移，所以残影跟着角色往身后拖
            float nx = 1.f;
            float ny = 0.f;
            fxDir(fx, nx, ny);
            const float len = fx.radius * (0.5f + 0.5f * grow);
            const float ox = player.x;
            const float oy = player.y - 8.f;
            const QColor base = fxColor(fx, QColor(143, 184, 255));
            painter.setPen(Qt::NoPen);
            for (int i = 3; i >= 1; --i) {
                const float t = float(i) / 3.f;
                const float gx = ox - nx * len * t;
                const float gy = oy - ny * len * t;
                const float half = 9.f * (1.f - t * 0.55f);
                const QPointF ghost[4] = {
                    QPointF(gx + nx * 10.f, gy + ny * 10.f),
                    QPointF(gx - ny * half, gy + nx * half),
                    QPointF(gx - nx * 22.f, gy - ny * 22.f),
                    QPointF(gx + ny * half, gy - nx * half)};
                painter.setBrush(withAlpha(base, int(90.f * u * (1.f - t * 0.5f))));
                painter.drawPolygon(ghost, 4);
            }
            const QPointF tail(ox - nx * len, oy - ny * len);
            const QPointF tip(ox + nx * 8.f, oy + ny * 8.f);
            strokeGlow(painter, withAlpha(base, int(220 * u)), 2.5f, [&] { painter.drawLine(tail, tip); });
            painter.setPen(QPen(withAlpha(QColor(255, 255, 255), int(230 * u)), 1.2f, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QLineF(tail + (tip - tail) * 0.45f, tip));
            const QPointF head0[3] = {
                tip + QPointF(nx * 8.f, ny * 8.f),
                tip + QPointF(-ny * 5.f, nx * 5.f),
                tip + QPointF(ny * 5.f, -nx * 5.f)};
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(QColor(255, 255, 255), int(240 * u)));
            painter.drawPolygon(head0, 3);
        } else if (fx.kind == AttackFxKind::Slash) {
            const float cx = player.x;
            const float cy = player.y - 8.f;
            const float ang = std::atan2(fx.fy, fx.fx);
            const float half = fx.halfAngle;
            const float reveal = std::min(1.f, grow / 0.55f);
            const float r = fx.radius * (0.75f + 0.25f * grow);
            const QPainterPath arc = arcPath(cx, cy, r, ang - half, ang - half + half * 2.f * reveal);
            const QColor base = fxColor(fx, QColor(255, 246, 222));
            strokeGlow(painter, withAlpha(base, int(240 * u)), 3.5f * u + 1.f, [&] { painter.drawPath(arc); });
        } else if (fx.kind == AttackFxKind::Burst) {
            const float p = grow;
            const float r = std::max(2.f, fx.radius * std::sqrt(p));
            QRadialGradient fire(fx.x, fx.y, r);
            fire.setColorAt(0.0, QColor(255, 244, 190, int(230 * u)));
            fire.setColorAt(0.45, QColor(255, 150, 40, int(200 * u)));
            fire.setColorAt(1.0, QColor(200, 50, 20, 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(fire);
            painter.drawEllipse(QRectF(fx.x - r, fx.y - r, r * 2.f, r * 2.f));
            const float core = fx.radius * 0.5f * std::sqrt(p) * (1.f - p);
            painter.setBrush(QColor(255, 255, 255, int(230 * u)));
            painter.drawEllipse(QRectF(fx.x - core, fx.y - core, core * 2.f, core * 2.f));
            const float shock = fx.radius * (0.6f + 0.5f * p);
            strokeGlow(painter, QColor(255, 190, 90, int(160 * u)), 1.5f, [&] {
                painter.drawEllipse(QRectF(fx.x - shock, fx.y - shock, shock * 2.f, shock * 2.f));
            });
        } else if (fx.kind == AttackFxKind::Pillar) {
            const float footY = player.y;
            const float h = fx.radius * (0.6f + 0.4f * std::min(1.f, grow * 3.f));
            const float w = 14.f * (1.f - grow * 0.5f);
            const QColor base = fxColor(fx, QColor(255, 210, 80));
            QLinearGradient beam(0.f, footY - h, 0.f, footY);
            beam.setColorAt(0.0, withAlpha(base, 0));
            beam.setColorAt(0.6, withAlpha(base, int(150 * u)));
            beam.setColorAt(1.0, QColor(255, 255, 230, int(220 * u)));
            painter.setPen(Qt::NoPen);
            painter.setBrush(beam);
            painter.drawRect(QRectF(player.x - w * 0.5f, footY - h, w, h));
            painter.setBrush(QColor(255, 255, 240, int(200 * u)));
            painter.drawRect(QRectF(player.x - w * 0.15f, footY - h * 0.9f, w * 0.3f, h * 0.9f));
        } else if (fx.kind == AttackFxKind::Cone) {
            const float d = std::sqrt(fx.fx * fx.fx + fx.fy * fx.fy);
            const float nx = d > 0.001f ? fx.fx / d : 1.f;
            const float ny = d > 0.001f ? fx.fy / d : 0.f;
            const float ang = std::atan2(ny, nx);
            QPainterPath path;
            path.moveTo(fx.x, fx.y);
            path.lineTo(fx.x + std::cos(ang - fx.halfAngle) * fx.radius,
                fx.y + std::sin(ang - fx.halfAngle) * fx.radius);
            path.arcTo(QRectF(fx.x - fx.radius, fx.y - fx.radius, fx.radius * 2.f, fx.radius * 2.f),
                (-ang - fx.halfAngle) * 180.f / 3.14159265f, (fx.halfAngle * 2.f) * 180.f / 3.14159265f);
            path.closeSubpath();
            painter.setPen(QPen(QColor(255, 120, 40, int(50 + 140 * u)), 2));
            painter.setBrush(QColor(255, 160, 40, int(30 + 70 * u)));
            painter.drawPath(path);
        } else if (fx.kind == AttackFxKind::Crescent) {
            const float d = std::sqrt(fx.fx * fx.fx + fx.fy * fx.fy);
            const float nx = d > 0.001f ? fx.fx / d : 1.f;
            const float ny = d > 0.001f ? fx.fy / d : 0.f;
            const float ang = std::atan2(ny, nx);
            const float rOuter = fx.radius * (0.82f + 0.18f * grow);
            const float rInner = rOuter * 0.52f;
            const float half = fx.halfAngle;
            constexpr int kSegs = 18;
            QPainterPath path;
            for (int i = 0; i <= kSegs; ++i) {
                const float t = float(i) / float(kSegs);
                const float a = ang - half + t * (half * 2.f);
                const float px = fx.x + std::cos(a) * rOuter;
                const float py = fx.y + std::sin(a) * rOuter;
                if (i == 0) {
                    path.moveTo(px, py);
                } else {
                    path.lineTo(px, py);
                }
            }
            for (int i = kSegs; i >= 0; --i) {
                const float t = float(i) / float(kSegs);
                const float a = ang - half + t * (half * 2.f);
                path.lineTo(fx.x + std::cos(a) * rInner, fx.y + std::sin(a) * rInner);
            }
            path.closeSubpath();
            painter.setPen(QPen(QColor(255, 220, 120, int(90 + 140 * u)), 2));
            painter.setBrush(QColor(255, 180, 60, int(40 + 90 * u)));
            painter.drawPath(path);
            strokeGlow(painter, QColor(255, 255, 210, int(50 + 120 * u)), 1.5f, [&] { painter.drawPath(path); });
        } else if (fx.kind == AttackFxKind::Dash) {
            const float d = std::sqrt(fx.fx * fx.fx + fx.fy * fx.fy);
            const float nx = d > 0.001f ? fx.fx / d : 1.f;
            const float ny = d > 0.001f ? fx.fy / d : 0.f;
            const float len = fx.radius * (0.55f + 0.45f * grow);
            const float half = 7.f;
            const QPointF tip(fx.x + nx * len, fx.y + ny * len);
            QPainterPath path;
            path.moveTo(tip);
            path.lineTo(QPointF(fx.x - ny * half, fx.y + nx * half));
            path.lineTo(QPointF(fx.x + ny * half, fx.y - nx * half));
            path.closeSubpath();
            painter.setPen(QPen(QColor(120, 220, 255, int(80 + 140 * u)), 2));
            painter.setBrush(QColor(80, 180, 255, int(40 + 80 * u)));
            painter.drawPath(path);
            painter.drawLine(QPointF(fx.x, fx.y), tip);
        } else if (fx.kind == AttackFxKind::Blink) {
            const float r = fx.radius * (0.55f + 0.9f * grow);
            painter.setBrush(QColor(140, 90, 255, int(30 + 70 * u)));
            painter.setPen(QPen(QColor(200, 160, 255, int(90 + 140 * u)), 2));
            painter.drawEllipse(QRectF(fx.x - r, fx.y - r, r * 2.f, r * 2.f));
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 240, 255, int(40 + 100 * u)), 1));
            painter.drawEllipse(QRectF(fx.x - r * 0.45f, fx.y - r * 0.45f, r * 0.9f, r * 0.9f));
        } else if (fx.kind == AttackFxKind::Laser) {
            const float d = std::sqrt(fx.fx * fx.fx + fx.fy * fx.fy);
            const float nx = d > 0.001f ? fx.fx / d : 1.f;
            const float ny = d > 0.001f ? fx.fy / d : 0.f;
            const float len = fx.radius * (0.75f + 0.25f * u);
            const QPointF a(fx.x, fx.y);
            const QPointF b(fx.x + nx * len, fx.y + ny * len);
            painter.setPen(QPen(QColor(120, 255, 255, int(40 + 80 * u)), 10.f, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(a, b);
            painter.setPen(QPen(QColor(255, 120, 80, int(90 + 140 * u)), 4.5f, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(a, b);
            painter.setPen(QPen(QColor(255, 255, 230, int(120 + 100 * u)), 1.8f, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(a, b);
            const float er = 10.f + 16.f * grow;
            painter.setBrush(QColor(255, 160, 60, int(50 + 100 * u)));
            painter.setPen(QPen(QColor(255, 220, 120, int(80 + 120 * u)), 2));
            painter.drawEllipse(QRectF(b.x() - er, b.y() - er, er * 2.f, er * 2.f));
        } else if (fx.kind == AttackFxKind::Mirror) {
            const float r = fx.radius * (0.7f + 0.5f * grow);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(220, 80, 255, int(80 + 140 * u)), 2));
            painter.drawEllipse(QRectF(fx.x - r, fx.y - r, r * 2.f, r * 2.f));
        } else if (fx.kind == AttackFxKind::Mushroom) {
            // 核级引爆：紫黑烟柱把一圈不断涨大的伞盖顶上去，尾段整体散掉
            const QColor base = fxColor(fx, QColor(150, 60, 220));
            const float rise = std::min(1.f, grow * 2.2f);
            const float capR = fx.radius * (0.25f + 0.75f * rise);
            const float capY = fx.y - 40.f - 96.f * rise;
            const float stemW = fx.radius * 0.30f * (1.f - 0.35f * rise);
            const float fade = std::min(1.f, u * 2.4f);
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(QColor(18, 10, 28), int(190 * fade)));
            painter.drawRect(QRectF(fx.x - stemW * 0.5f, capY, stemW, fx.y - capY + 8.f));
            painter.setBrush(withAlpha(QColor(22, 12, 34), int(220 * fade)));
            painter.drawEllipse(QRectF(fx.x - capR, capY - capR * 0.62f, capR * 2.f, capR * 1.24f));
            painter.setBrush(withAlpha(QColor(52, 22, 84), int(150 * fade)));
            painter.drawEllipse(QRectF(fx.x - capR * 0.72f, capY - capR * 0.44f, capR * 1.44f, capR * 0.88f));
            strokeGlow(painter, withAlpha(base, int(200 * fade)), 2.5f * fade + 0.6f, [&] {
                painter.drawEllipse(QRectF(fx.x - capR, capY - capR * 0.62f, capR * 2.f, capR * 1.24f));
            });
        } else if (fx.kind == AttackFxKind::Grid) {
            // 蓄力：一道扫描环由内向外推开，紫网格跟着亮起来，扫过之后再暗回去
            const float step = float(kTile) * 2.f;
            const float scan = fx.radius * grow;
            const int lines = int(fx.radius / step);
            painter.setPen(Qt::NoPen);
            for (int i = -lines; i <= lines; ++i) {
                const float off = float(i) * step;
                const float dist = std::abs(off);
                if (dist > fx.radius) {
                    continue;
                }
                const float lit = 1.f - std::min(1.f, std::abs(dist - scan) / (fx.radius * 0.4f + 1.f));
                if (lit <= 0.03f) {
                    continue;
                }
                const QColor line = withAlpha(fxColor(fx, QColor(166, 96, 248)),
                    int(190.f * lit * std::min(1.f, u * 2.2f)));
                painter.fillRect(QRectF(fx.x + off - 0.75f, fx.y - fx.radius, 1.5f, fx.radius * 2.f), line);
                painter.fillRect(QRectF(fx.x - fx.radius, fx.y + off - 0.75f, fx.radius * 2.f, 1.5f), line);
            }
        }
    }
    painter.setPen(Qt::NoPen);
    for (const Bolt& bolt : session_.bolts()) {
        if (bolt.mage) {
            const bool flash = int(bolt.age * 14.f) % 2 == 0;
            const QColor a(60, 255, 220);   // 青绿
            const QColor b(255, 80, 255);   // 品红
            const QColor head = flash ? a : b;
            const QColor tail = flash ? b : a;
            for (int i = 0; i < bolt.trailLen; ++i) {
                const float t = bolt.trailLen <= 1 ? 1.f : float(i) / float(bolt.trailLen - 1);
                const float r = 2.f + t * 3.5f;
                const int alpha = int(40 + 180 * t);
                QColor c = t > 0.55f ? head : tail;
                c.setAlpha(alpha);
                painter.setBrush(c);
                painter.drawEllipse(QRectF(bolt.trailX[i] - r, bolt.trailY[i] - r, r * 2.f, r * 2.f));
            }
            // 拖尾连线
            if (bolt.trailLen >= 2) {
                QPen pen(tail);
                pen.setWidthF(2.4f);
                pen.setCapStyle(Qt::RoundCap);
                painter.setPen(pen);
                for (int i = 1; i < bolt.trailLen; ++i) {
                    QColor c = i > bolt.trailLen / 2 ? head : tail;
                    c.setAlpha(50 + i * 20);
                    pen.setColor(c);
                    painter.setPen(pen);
                    painter.drawLine(QPointF(bolt.trailX[i - 1], bolt.trailY[i - 1]), QPointF(bolt.trailX[i], bolt.trailY[i]));
                }
                painter.setPen(Qt::NoPen);
            }
            painter.setBrush(QColor(255, 255, 255, 220));
            painter.drawEllipse(QRectF(bolt.x - 2.5, bolt.y - 2.5, 5, 5));
            painter.setBrush(head);
            painter.drawEllipse(QRectF(bolt.x - 5, bolt.y - 5, 10, 10));
            painter.setBrush(QColor(head.red(), head.green(), head.blue(), 80));
            painter.drawEllipse(QRectF(bolt.x - 8, bolt.y - 8, 16, 16));
        } else if (bolt.robot) {
            const float speed = std::max(1.f, std::sqrt(bolt.vx * bolt.vx + bolt.vy * bolt.vy));
            const float by = bolt.y - bolt.lift;
            const QColor core = bolt.hostile ? QColor(255, 70, 60) : (bolt.crit ? QColor(255, 220, 80) : QColor(110, 230, 255));
            if (bolt.blast > 0.f) {
                const float pulse = 0.5f + 0.5f * std::sin(bolt.age * 30.f);
                painter.setBrush(QColor(255, 140, 40, int(90 + 80 * pulse)));
                painter.drawEllipse(QRectF(bolt.x - 6, by - 6, 12, 12));
                painter.setBrush(QColor(255, 235, 160));
                painter.drawEllipse(QRectF(bolt.x - 3, by - 3, 6, 6));
            } else {
                const bool big = !bolt.hostile;
                const float tail = big ? 14.f : 9.f;
                const float head = big ? 3.f : 1.5f;
                const QPointF from(bolt.x - bolt.vx / speed * tail, by - bolt.vy / speed * tail);
                QPen pen(core);
                pen.setCapStyle(Qt::RoundCap);
                if (big) {
                    pen.setColor(QColor(core.red(), core.green(), core.blue(), 90));
                    pen.setWidthF(7.f);
                    painter.setPen(pen);
                    painter.drawLine(from, QPointF(bolt.x, by));
                    pen.setColor(core);
                }
                pen.setWidthF(big ? 3.5f : 2.f);
                painter.setPen(pen);
                painter.drawLine(from, QPointF(bolt.x, by));
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 255, 255, 230));
                painter.drawEllipse(QRectF(bolt.x - head, by - head, head * 2.f, head * 2.f));
            }
        } else {
            painter.setBrush(bolt.hostile ? QColor(126, 72, 148) : (bolt.crit ? QColor(255, 200, 60) : QColor(214, 196, 160)));
            painter.drawEllipse(QRectF(bolt.x - 3, bolt.y - 3, 6, 6));
        }
    }
    constexpr float kDroneLift = 16.f;
    constexpr int kDroneFrame = 16;
    for (const Drone& drone : session_.drones()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 70));
        painter.drawEllipse(QRectF(drone.x - 5.f, drone.y - 2.f, 10.f, 4.f));
        // 自毁前一秒半闪烁提示
        if (drone.life < 1.5f && int(drone.life * 10.f) % 2 == 0) {
            continue;
        }
        const float dy = drone.y - kDroneLift + std::sin(drone.age * 9.f + float(drone.slot)) * 1.5f;
        if (!sprites_.drone.isNull()) {
            const int frames = std::max(1, sprites_.drone.width() / kDroneFrame);
            const int frame = int(drone.age * 24.f) % frames;
            painter.drawImage(QRectF(drone.x - kDroneFrame / 2, dy - kDroneFrame / 2, kDroneFrame, kDroneFrame),
                sprites_.drone, QRect(frame * kDroneFrame, 0, kDroneFrame, kDroneFrame));
        } else {
            painter.setBrush(QColor(96, 108, 124));
            painter.drawRect(QRectF(drone.x - 3.f, dy - 3.f, 6.f, 6.f));
            painter.setBrush(QColor(110, 230, 255));
            painter.drawRect(QRectF(drone.x - 1.f, dy, 2.f, 1.f));
        }
    }
    drawParticles(painter);
    for (const FloatText& text : session_.floatTexts()) {
        const int shown = text.amount <= 0.f ? 0 : int(std::ceil(text.amount));
        const QString label = text.crit ? QString("暴击 %1").arg(shown) : QString::number(shown);
        painter.setFont(QFont(Platform::uiFontFamily(), text.crit ? 11 : 8, text.crit ? QFont::Bold : QFont::Normal));
        painter.setPen(QColor(0, 0, 0, 180));
        painter.drawText(QRectF(text.x - 28, text.y - 7, 56, 14), Qt::AlignCenter, label);
        painter.setPen(text.crit ? QColor(255, 220, 40) : QColor(255, 245, 230));
        painter.drawText(QRectF(text.x - 28, text.y - 8, 56, 14), Qt::AlignCenter, label);
    }
}

void GameWidget::drawSanBar(QPainter& painter, const QRect& view) {
    if (!session_.sanActive()) {
        return;
    }
    const float ratio = session_.maxSan() <= 0.f ? 0.f : std::clamp(session_.san() / session_.maxSan(), 0.f, 1.f);
    const bool weak = session_.sanWeak();
    // 细长的一条：与 boss 血条（plate 内缩 8px 的轨道）左右都没差
    const QRect boss = bossBarPlate(view);
    const int bx = boss.x() + 8;
    const int barW = boss.width() - 16;
    const int barH = 6;
    const int by = view.y() + 108;   // 紧贴 boss 血条下方
    const float cy = float(by) + float(barH) * 0.5f;
    // 填充色：高=青蓝，中=琥珀，低=红（低时呼吸）
    QColor fill;
    int pulse = 255;
    if (ratio > 0.5f) {
        fill = QColor(120, 210, 220);
    } else if (ratio > 0.1f) {
        fill = QColor(230, 186, 80);
    } else {
        pulse = int(140 + 100 * (0.5 + 0.5 * std::sin(session_.time() * 6.f)));
        fill = QColor(230, 70, 60);
    }
    // 槽底 + 背板
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 8, 14, 210));
    painter.drawRect(QRect(bx - 4, by - 15, barW + 8, barH + 28));
    painter.setBrush(QColor(0, 0, 0, 130));
    painter.drawRect(QRect(bx, by, barW, barH));
    // 进度填充
    const int fillW = int(barW * ratio);
    painter.setBrush(fill);
    painter.drawRect(QRect(bx, by, fillW, barH));
    // 10% 处刻度线
    painter.setPen(QColor(255, 255, 255, 140));
    const int markX = bx + int(barW * 0.1f);
    painter.drawLine(markX, by - 1, markX, by + barH + 1);

    // 末端粒子消散：理智正在流失，边缘持续往外飘落
    if (ratio > 0.002f && ratio < 0.998f) {
        const float fx = float(bx + fillW);
        constexpr int kCount = 16;
        for (int i = 0; i < kCount; ++i) {
            // 用下标做确定性散列，避免每帧随机数导致闪烁
            const uint32_t h = (uint32_t(i + 1) * 2654435761u) ^ (uint32_t(i + 1) * 40503u);
            const float seedA = float(h % 1000u) / 1000.f;
            const uint32_t h2 = h * 2654435761u;
            const float seedB = float(h2 % 1000u) / 1000.f;
            const float speed = 0.35f + seedA * 0.55f;
            const float travel = std::fmod(session_.time() * speed + seedB, 1.f);
            const float reach = 6.f + seedA * 20.f;
            const float px = fx + travel * reach;
            // 略微上下散开，越飘越淡
            const float rise = (seedB - 0.5f) * 2.f * (2.f + travel * 5.f);
            const float py = cy - rise - travel * 3.f;
            const int alpha = int(225.f * (1.f - travel * travel));
            const float size = travel < 0.45f ? 2.f : 1.f;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(fill.red(), fill.green(), fill.blue(), alpha));
            painter.drawEllipse(QRectF(px, py, size, size));
        }
        // 末端亮线，让流失边缘更清楚
        painter.setPen(QPen(QColor(255, 255, 255, int(150 + 80 * (0.5 + 0.5 * std::sin(session_.time() * 8.f)))), 1));
        painter.drawLine(int(fx), by - 1, int(fx), by + barH + 1);
    }

    // 标签（注明是玩家的 SAN）+ 警示 + 剩余时间（mm:ss）
    painter.setFont(QFont(Platform::uiFontFamily(), 9, QFont::Bold));
    painter.setPen(weak ? QColor(244, 130, 118) : QColor(176, 214, 226));
    const QString label = QStringLiteral("玩家 SAN");
    painter.drawText(QRect(bx, by - 14, barW, 12), Qt::AlignLeft | Qt::AlignVCenter, label);
    const int remain = int(std::ceil(session_.sanRemaining()));
    const int mm = remain / 60, ss = remain % 60;
    painter.setPen(QColor(228, 212, 188));
    painter.drawText(QRect(bx, by - 14, barW, 12), Qt::AlignRight | Qt::AlignVCenter,
        QString("%1:%2").arg(mm, 2, 10, QChar('0')).arg(ss, 2, 10, QChar('0')));
    // 警示：越低越狠，文字随时间呼吸
    QString warn;
    QColor warnColor;
    if (weak) {
        warn = QStringLiteral("！警告：虚弱！理智耗尽即死，不可复活，立刻击杀投影");
        warnColor = QColor(242, 96, 84);
    } else if (ratio <= 0.3f) {
        warn = QStringLiteral("！警告：理智正在快速流失，尽快击败投影");
        warnColor = QColor(238, 178, 74);
    }
    if (!warn.isEmpty()) {
        const int warnPulse = int(160 + 90 * (0.5 + 0.5 * std::sin(session_.time() * 6.f)));
        QColor c = warnColor;
        c.setAlpha(warnPulse);
        painter.setPen(c);
        painter.drawText(QRect(bx, by + barH + 1, barW, 12), Qt::AlignLeft | Qt::AlignVCenter, warn);
    }
    // 低理智时整框呼吸描边
    painter.setPen(QPen(QColor(230, 90, 78, pulse), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(bx - 4, by - 15, barW + 8, barH + 28));
}

void GameWidget::drawRadar(QPainter& painter, const QRect& view) {
    const Player& player = session_.player();
    constexpr int kRadar = kRadarSide;
    constexpr float kRange = 320.f;
    const int radarX = view.x() + view.width() - kRadar - 12;
    int radarY = view.y() + 34;
    if (touchUi_) {
        // 让出右上角的「说明 / 暂停」按钮
        radarY = std::max(radarY, int(touchUnit() * 0.13) + 8);
    }
    const QRectF plate(radarX, radarY, kRadar, kRadar);
    const QPointF center(plate.center());
    const float radius = kRadar * 0.5f - 4.f;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 9, 8, 210));
    painter.drawEllipse(plate);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(90, 58, 50), 1.5));
    painter.drawEllipse(plate.adjusted(1, 1, -1, -1));
    painter.setPen(QPen(QColor(70, 52, 44, 160), 1));
    painter.drawEllipse(QRectF(center.x() - radius * 0.66f, center.y() - radius * 0.66f, radius * 1.32f, radius * 1.32f));
    painter.drawEllipse(QRectF(center.x() - radius * 0.33f, center.y() - radius * 0.33f, radius * 0.66f, radius * 0.66f));
    painter.drawLine(QPointF(center.x() - radius, center.y()), QPointF(center.x() + radius, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - radius), QPointF(center.x(), center.y() + radius));

    auto toRadar = [&](float wx, float wy, bool* onRim) -> QPointF {
        float dx = wx - player.x;
        float dy = wy - player.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float t = dist / kRange;
        if (t > 1.f) {
            dx = dx / dist * kRange;
            dy = dy / dist * kRange;
            t = 1.f;
            if (onRim) {
                *onRim = true;
            }
        } else if (onRim) {
            *onRim = false;
        }
        return QPointF(center.x() + dx / kRange * radius, center.y() + dy / kRange * radius);
    };

    auto monsterColor = [](MonsterKind kind) -> QColor {
        switch (kind) {
        case MonsterKind::Skeleton:
            return QColor(210, 190, 160);
        case MonsterKind::Mushroom:
            return QColor(200, 90, 70);
        case MonsterKind::Flyer:
            return QColor(120, 190, 255);
        case MonsterKind::Caster:
            return QColor(180, 110, 220);
        case MonsterKind::Killbot:
            return QColor(255, 150, 60);
        case MonsterKind::Eye:
            return QColor(220, 40, 50);
        default:
            return QColor(220, 80, 70);
        }
    };

    for (const Monster& monster : session_.monsters()) {
        if (monster.state == ActorState::Dead) {
            continue;
        }
        if (monster.kind == MonsterKind::Eye && session_.ruin().arrive == 0.f) {
            continue;
        }
        bool onRim = false;
        const QPointF p = toRadar(monster.x, monster.y, &onRim);
        const QColor col = monsterColor(monster.kind);
        painter.setPen(Qt::NoPen);
        painter.setBrush(col);
        const float dot = onRim ? 2.4f : 3.2f;
        painter.drawEllipse(QRectF(p.x() - dot, p.y() - dot, dot * 2.f, dot * 2.f));
        // 动向短线
        float fx = monster.facingX;
        float fy = monster.facingY;
        const float fl = std::sqrt(fx * fx + fy * fy);
        if (fl > 0.01f) {
            fx /= fl;
            fy /= fl;
            painter.setPen(QPen(QColor(col.red(), col.green(), col.blue(), 220), 1.4));
            painter.drawLine(p, QPointF(p.x() + fx * 7.f, p.y() + fy * 7.f));
        }
    }

    // 角色：朝向三角
    float pfx = player.facingX;
    float pfy = player.facingY;
    const float pl = std::sqrt(pfx * pfx + pfy * pfy);
    if (pl > 0.01f) {
        pfx /= pl;
        pfy /= pl;
    } else {
        pfx = 1.f;
        pfy = 0.f;
    }
    const float px = -pfy;
    const float py = pfx;
    QPainterPath tri;
    tri.moveTo(center.x() + pfx * 7.f, center.y() + pfy * 7.f);
    tri.lineTo(center.x() - pfx * 4.f + px * 4.5f, center.y() - pfy * 4.f + py * 4.5f);
    tri.lineTo(center.x() - pfx * 4.f - px * 4.5f, center.y() - pfy * 4.f - py * 4.5f);
    tri.closeSubpath();
    painter.setPen(QPen(QColor(255, 240, 210), 1));
    painter.setBrush(QColor(90, 200, 255));
    painter.drawPath(tri);

    const MazeRuin& ruin = session_.ruin();
    if (player.seekOn && ruin.active) {
        bool onRim = false;
        const QPointF p = toRadar(ruin.entranceWorldX(), ruin.entranceWorldY(), &onRim);
        painter.setPen(QPen(QColor(255, 220, 120), 1.4));
        painter.setBrush(QColor(210, 170, 60));
        QPainterPath mark;
        mark.moveTo(p.x(), p.y() - 5.f);
        mark.lineTo(p.x() + 4.f, p.y());
        mark.lineTo(p.x(), p.y() + 5.f);
        mark.lineTo(p.x() - 4.f, p.y());
        mark.closeSubpath();
        painter.drawPath(mark);
        if (onRim) {
            const float dx = ruin.entranceWorldX() - player.x;
            const float dy = ruin.entranceWorldY() - player.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 1.f) {
                painter.setPen(QPen(QColor(255, 210, 90), 1.6));
                painter.drawLine(p, QPointF(p.x() + dx / d * 8.f, p.y() + dy / d * 8.f));
            }
        }
        painter.setFont(QFont(Platform::uiFontFamily(), 8));
        painter.setPen(QColor(255, 214, 120));
        painter.drawText(QRectF(p.x() - 16.f, p.y() + 4.f, 32.f, 12.f), Qt::AlignCenter, QStringLiteral("遗迹"));
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(228, 212, 188));
    painter.setFont(QFont(Platform::uiFontFamily(), 11));
    painter.setRenderHint(QPainter::Antialiasing, false);
}

QRect GameWidget::mazeMapRect(const QRect& view) const {
    const int radarX = view.x() + view.width() - kRadarSide - 12;
    int radarY = view.y() + 34;
    if (touchUi_) {
        radarY = std::max(radarY, int(touchUnit() * 0.13) + 8);
    }
    int mapX = radarX + (kRadarSide - kMazeMapSide) / 2;
    int mapY = radarY + kRadarSide + 16;
    // 触屏时右下角是攻击与技能键，地图压在雷达下方会被按键盖住，一律挪到雷达左侧
    if (touchUi_ || mapY + kMazeMapSide > view.bottom() - 8) {
        mapX = radarX - kMazeMapSide - 8;
        mapY = radarY;
    }
    if (mapX < view.x() + 4) {
        mapX = view.x() + 4;
    }
    return QRect(mapX, mapY, kMazeMapSide, kMazeMapSide);
}

void GameWidget::drawMazeMap(QPainter& painter, const QRect& view) {
    const Player& player = session_.player();
    const MazeRuin& ruin = session_.ruin();
    // 走进迷宫就显示地图；只有那条进出中央广场的路线要靠寻路技能
    if (!ruin.active) {
        return;
    }
    if (!ruin.contains(tileOf(player.x), tileOf(player.y))) {
        return;
    }
    const QRect plate = mazeMapRect(view);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(12, 10, 9, 220));
    painter.drawRect(plate);
    painter.setPen(QPen(QColor(120, 72, 58), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(plate.adjusted(0, 0, -1, -1));
    const float cell = float(plate.width() - 8) / float(MazeRuin::kSize);
    const float ox = plate.x() + 4.f;
    const float oy = plate.y() + 4.f;
    for (int y = 0; y < MazeRuin::kSize; ++y) {
        for (int x = 0; x < MazeRuin::kSize; ++x) {
            const QRectF tile(ox + x * cell, oy + y * cell, cell, cell);
            const bool plaza = x >= MazeRuin::kPlaza0 && x <= MazeRuin::kPlaza1 && y >= MazeRuin::kPlaza0 && y <= MazeRuin::kPlaza1;
            const bool entrance = x == ruin.entranceX && y == ruin.entranceY;
            if (ruin.isWallAt(ruin.originX + x, ruin.originY + y)) {
                painter.fillRect(tile, QColor(28, 32, 48));
            } else if (entrance) {
                painter.fillRect(tile, QColor(255, 220, 80));
            } else if (MazeRuin::isPlazaRock(x, y)) {
                painter.fillRect(tile, QColor(52, 50, 58));
            } else if (player.seekOn && ruin.onRoute(x, y)) {
                painter.fillRect(tile, QColor(220, 36, 32));
            } else if (plaza) {
                painter.fillRect(tile, QColor(176, 64, 56));
            } else {
                painter.fillRect(tile, QColor(214, 190, 140));
            }
        }
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(220, 40, 48));
    const float ex = ox + (float(MazeRuin::kCenter) + 0.5f) * cell;
    const float ey = oy + (float(MazeRuin::kCenter) + 0.5f) * cell;
    painter.drawEllipse(QRectF(ex - 2.f, ey - 2.f, 4.f, 4.f));
    const float px = ox + (player.x / float(kTile) - float(ruin.originX)) * cell;
    const float py = oy + (player.y / float(kTile) - float(ruin.originY)) * cell;
    painter.setPen(QPen(QColor(20, 16, 14), 1));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawEllipse(QRectF(px - 2.5f, py - 2.5f, 5.f, 5.f));
    painter.setPen(QColor(228, 212, 188));
    painter.setFont(QFont(Platform::uiFontFamily(), 8));
    painter.drawText(QRect(plate.x(), plate.y() - 14, plate.width(), 12), Qt::AlignCenter,
        player.seekOn ? QStringLiteral("迷宫路线") : QStringLiteral("迷宫地图"));
    if (!player.seekOn) {
        // 没开寻路时补一条提示，免得玩家以为是地图坏了；放在地图外面，别压住地图本身
        constexpr int kTipH = 13;
        const int below = plate.bottom() + 2;
        const int tipY = below + kTipH <= view.bottom() ? below : plate.top() - kTipH - 2;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(12, 10, 9, 210));
        painter.drawRect(QRect(plate.x(), tipY, plate.width(), kTipH));
        painter.setPen(QColor(198, 188, 174));
        painter.setFont(QFont(Platform::uiFontFamily(), 7));
        painter.drawText(QRect(plate.x(), tipY, plate.width(), kTipH), Qt::AlignCenter,
            QStringLiteral("开启寻路显示路线"));
    }
}

QRect GameWidget::bossBarPlate(const QRect& view) const {
    const int left = view.x() + 232;
    int right = view.right() - (touchUi_ ? 148 : 96);
    if (touchUi_) {
        // 触屏时迷宫地图摆在雷达左侧，血条别压上去
        right = std::min(right, mazeMapRect(view).left() - 8);
    }
    const int barW = std::min(420, std::max(200, right - left));
    const int barX = left + std::max(0, (right - left - barW) / 2);
    return QRect(barX, view.y() + 30, barW, 64);
}

void GameWidget::drawBossBar(QPainter& painter, const QRect& view) {
    const MazeRuin& ruin = session_.ruin();
    if (!ruin.active || ruin.phase != MazeRuin::Phase::Live || ruin.bossDead) {
        return;
    }
    if (!ruin.enteredPlaza || !ruin.contains(tileOf(session_.player().x), tileOf(session_.player().y))) {
        return;
    }
    const Monster* eye = nullptr;
    for (const Monster& monster : session_.monsters()) {
        if (monster.kind == MonsterKind::Eye && monster.state != ActorState::Dead) {
            eye = &monster;
            break;
        }
    }
    if (!eye) {
        return;
    }

    // 与 drawSanBar 共用同一套几何，保证左侧对齐
    const QRect plate = bossBarPlate(view);
    painter.setPen(QPen(QColor(120, 60, 56), 1));
    painter.setBrush(QColor(34, 18, 20, 235));
    painter.drawRect(plate);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 90));
    painter.drawRect(QRect(plate.x(), plate.y(), plate.width(), 8));
    painter.drawRect(QRect(plate.x(), plate.bottom() - 7, plate.width(), 8));

    painter.setFont(QFont(Platform::uiFontFamily(), 8, QFont::Bold));
    painter.setPen(QColor(232, 130, 118));
    painter.drawText(QRect(plate.x() + 8, plate.y() + 3, 36, 14), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("BOSS"));
    painter.setFont(QFont(Platform::uiFontFamily(), 11, QFont::Bold));
    painter.setPen(QColor(236, 214, 200));
    const QString bossName = QStringLiteral("克苏鲁之眼");
    painter.drawText(QRect(plate.x() + 44, plate.y() + 2, plate.width() - 150, 16), Qt::AlignVCenter | Qt::AlignLeft, bossName);
    const int nameW = painter.fontMetrics().horizontalAdvance(bossName);
    const QRect tag(plate.x() + 48 + nameW, plate.y() + 3, 34, 14);
    painter.setPen(QPen(QColor(92, 28, 32), 1));
    painter.setBrush(QColor(16, 6, 8, 230));
    painter.drawRoundedRect(tag, 2, 2);
    painter.setFont(QFont(Platform::uiFontFamily(), 8, QFont::Bold));
    painter.setPen(QColor(176, 96, 88));
    painter.drawText(tag, Qt::AlignCenter, QStringLiteral("投影"));
    painter.setPen(QColor(132, 104, 96));
    painter.drawText(QRect(plate.x() + 8, plate.y() + 2, plate.width() - 16, 16), Qt::AlignVCenter | Qt::AlignRight,
        QString("Lv%1").arg(eye->level));
    const int hpShown = eye->hp <= 0.f ? 0 : int(std::ceil(eye->hp));

    const float dist = std::hypot(session_.player().x - eye->x, session_.player().y - eye->y);
    QString status;
    if (ruin.arrive > 0.f) {
        status = QStringLiteral("状态  投影正在降临");
    } else if (eye->stunT > 0.f) {
        status = QStringLiteral("状态  韧性崩溃，攻击中断");
    } else if (ruin.pulseR >= 0.f) {
        status = QStringLiteral("状态  血环正在向外扩张");
    } else if (dist > 320.f) {
        status = QStringLiteral("状态  盘踞广场中央，尚未锁定");
    } else {
        switch (ruin.attackStep % 3) {
        case 0:
            status = QStringLiteral("状态  盯住你，下一击是单发飞弹");
            break;
        case 1:
            status = QStringLiteral("状态  瞳孔散开，下一击是五发扇形");
            break;
        default:
            status = QStringLiteral("状态  眼裂张开，下一击是扩散血环");
            break;
        }
    }
    painter.setPen(QColor(104, 74, 70));
    painter.drawText(QRect(plate.x() + 8, plate.y() + 18, plate.width() - 16, 14), Qt::AlignVCenter | Qt::AlignLeft, status);

    const int trackX = plate.x() + 8;
    const int trackW = plate.width() - 16;
    const int trackY = plate.y() + 36;
    const float hpRatio = eye->maxHp <= 0.f ? 0.f : std::clamp(eye->hp / eye->maxHp, 0.f, 1.f);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(52, 24, 26));
    painter.drawRect(QRect(trackX, trackY, trackW, 8));
    painter.setBrush(QColor(220, 52, 54));
    painter.drawRect(QRect(trackX, trackY, int(trackW * hpRatio), 8));
    painter.setPen(QColor(250, 226, 218));
    painter.setFont(QFont(Platform::uiFontFamily(), 7, QFont::Bold));
    painter.drawText(QRect(trackX, trackY - 1, trackW, 10), Qt::AlignCenter,
        QString("%1 / %2").arg(hpShown).arg(int(std::lround(eye->maxHp))));
    painter.setPen(Qt::NoPen);
    if (eye->maxShield > 0.f) {
        const float shieldRatio = std::clamp(eye->shield / eye->maxShield, 0.f, 1.f);
        painter.setBrush(QColor(18, 16, 20));
        painter.drawRect(QRect(trackX, trackY + 9, trackW, 3));
        painter.setBrush(QColor(58, 66, 76));
        painter.drawRect(QRect(trackX, trackY + 9, int(trackW * shieldRatio), 3));
    }
    const int poiseY = trackY + 14;
    painter.setBrush(QColor(20, 14, 8));
    painter.drawRect(QRect(trackX, poiseY, trackW, 5));
    if (eye->stunT > 0.f) {
        painter.setBrush(QColor(110, 48, 28));
        painter.drawRect(QRect(trackX, poiseY, int(trackW * std::clamp(eye->stunT / 1.6f, 0.f, 1.f)), 5));
        painter.setPen(QColor(210, 160, 120));
        painter.setFont(QFont(Platform::uiFontFamily(), 7, QFont::Bold));
        painter.drawText(QRect(trackX, poiseY - 1, trackW, 7), Qt::AlignCenter, QStringLiteral("韧性崩溃"));
    } else {
        const float poiseRatio = eye->maxPoise <= 0.f ? 0.f : std::clamp(eye->poise / eye->maxPoise, 0.f, 1.f);
        painter.setBrush(QColor(168, 124, 42));
        painter.drawRect(QRect(trackX, poiseY, int(trackW * poiseRatio), 5));
    }
}

void GameWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    const QRect view = viewRect();
    for (const QRect& bar : QRegion(rect()).subtracted(view)) {
        painter.fillRect(bar, QColor(8, 7, 6));
    }
    const int originX = view.x();
    const int originY = view.y();
    const int viewW = view.width();
    const int viewH = view.height();

    canvas_.fill(QColor(16, 14, 12));
    {
        QPainter world(&canvas_);
        world.setRenderHint(QPainter::SmoothPixmapTransform, false);
        drawWorld(world);
        world.resetTransform();
        world.setCompositionMode(QPainter::CompositionMode_Multiply);
        world.fillRect(canvas_.rect(), QColor(128, 112, 98));
        world.setCompositionMode(QPainter::CompositionMode_SourceOver);
        world.drawImage(0, 0, vignette_);
        const float plazaRed = session_.plazaRed();
        if (plazaRed > 0.01f) {
            QRadialGradient red(kViewW * 0.5, kViewH * 0.5, kViewW * 0.72);
            red.setColorAt(0.28, QColor(120, 0, 0, 0));
            red.setColorAt(1.0, QColor(150, 0, 0, int(190.f * plazaRed)));
            world.setPen(Qt::NoPen);
            world.setBrush(red);
            world.drawRect(canvas_.rect());
        }
        // I am atomic 的紫色滤镜：蓄力时整块画面往紫里压，越靠边缘越浓
        const float violet = session_.atomicViolet();
        if (violet > 0.01f) {
            QRadialGradient purple(kViewW * 0.5, kViewH * 0.5, kViewW * 0.75);
            purple.setColorAt(0.0, QColor(124, 44, 206, int(96.f * violet)));
            purple.setColorAt(0.55, QColor(98, 26, 184, int(132.f * violet)));
            purple.setColorAt(1.0, QColor(56, 12, 118, int(215.f * violet)));
            world.setPen(Qt::NoPen);
            world.setBrush(purple);
            world.drawRect(canvas_.rect());
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(view, canvas_);
    if (flashT_ > 0.f && flashMax_ > 0.f) {
        const int alpha = int(255.f * std::min(1.f, flashT_ / flashMax_));
        painter.fillRect(view, QColor(255, 255, 255, alpha));
    }

    painter.setPen(QColor(90, 58, 50));
    painter.drawRect(view.adjusted(0, 0, -1, -1));
    const Player& player = session_.player();
    auto shown = [](float value) { return value <= 0.f ? 0 : int(std::ceil(value)); };
    painter.setFont(QFont(Platform::uiFontFamily(), 11));
    auto stat = [&](int row, const QString& name, float value, float maximum, const QColor& color) {
        const QRect plate(originX + 14, originY + 12 + row * 20, 210, 18);
        painter.fillRect(plate, QColor(12, 10, 9, 190));
        const float ratio = maximum <= 0.f ? 0.f : std::clamp(value / maximum, 0.f, 1.f);
        painter.fillRect(QRect(plate.x(), plate.bottom() - 2, int(plate.width() * ratio), 2), color);
        painter.setPen(color);
        painter.drawText(QRect(plate.x() + 6, plate.y(), 42, 16), Qt::AlignVCenter | Qt::AlignLeft, name);
        painter.setPen(QColor(228, 212, 188));
        painter.drawText(QRect(plate.x() + 48, plate.y(), plate.width() - 56, 16), Qt::AlignVCenter | Qt::AlignRight,
            QString("%1 / %2").arg(shown(value)).arg(int(std::lround(maximum))));
    };
    stat(0, "HP", player.hp, player.maxHp, QColor(168, 64, 52));
    stat(1, "SHD", player.shield, player.maxShield, QColor(122, 132, 138));
    stat(2, "STA", player.stamina, player.maxStamina, QColor(122, 118, 64));
    stat(3, "MP", player.mp, player.maxMp, QColor(108, 96, 148));
    const QRect meta(originX + 14, originY + 96, 210, 36);
    painter.fillRect(meta, QColor(12, 10, 9, 190));
    painter.setPen(QColor(228, 212, 188));
    painter.drawText(QRect(meta.x() + 6, meta.y() + 2, 198, 16), Qt::AlignVCenter | Qt::AlignLeft,
        QString("LV %1    XP %2 / %3").arg(player.level).arg(shown(player.xp)).arg(session_.xpToNext()));
    painter.drawText(QRect(meta.x() + 6, meta.y() + 18, 198, 16), Qt::AlignVCenter | Qt::AlignLeft,
        QString("ARM %1    CRT %2%").arg(int(std::lround(player.armor))).arg(12 + player.critBonus));
    auto talentLine = [&](int index, const QString& name, float current, float need) {
        painter.drawText(QRect(originX + 20, originY + 138 + index * 13, 200, 14), Qt::AlignLeft | Qt::AlignVCenter,
            QString("%1  %2 / %3").arg(name).arg(int(std::min(current, need))).arg(int(need)));
    };
    painter.fillRect(QRect(originX + 14, originY + 136, 210, 84), QColor(12, 10, 9, 190));
    painter.setPen(QColor(168, 148, 128));
    talentLine(0, "重手", player.damageDealt, 250.f);
    talentLine(1, "远行", player.distanceMoved, 900.f);
    talentLine(2, "轻身", float(player.dodgeCount), 6.f);
    talentLine(3, "熟练", float(player.skillCasts), 12.f);
    talentLine(4, "指引", float(player.worldKills), 20.f);
    talentLine(5, "以小博大", float(player.underdogKills), 10.f);
    painter.setPen(QColor(228, 212, 188));
    // 意识回归符咒按获取个数挂在 SCORE 旁边
    const int charms = session_.talismanCount();
    const QString scoreText = charms > 0
        ? QString("符咒 x%1   SCORE %2").arg(charms).arg(session_.score())
        : QString("SCORE %1").arg(session_.score());
    painter.drawText(QRect(originX, originY + 10, viewW, 22), Qt::AlignHCenter | Qt::AlignTop, formatTime(session_.time()));
    if (touchUi_) {
        // 右上角留给「索敌 / 说明 / 暂停」按钮
        painter.drawText(QRect(originX, originY + 30, viewW, 22), Qt::AlignHCenter | Qt::AlignTop, scoreText);
    } else {
        painter.drawText(QRect(originX, originY + 10, viewW - 16, 22), Qt::AlignRight | Qt::AlignTop, scoreText);
    }
    if (session_.cursed()) {
        // 屏幕底部居中提示条，避开顶部所有 HUD 与底部技能 Chip 行
        const QString curseText = QStringLiteral("诅咒：存在被克苏鲁余光注意！");
        painter.setFont(QFont(Platform::uiFontFamily(), 11, QFont::Bold));
        const QFontMetrics fm(painter.font());
        const int padX = 10, padY = 5;
        const int textW = fm.horizontalAdvance(curseText);
        const int bw = textW + padX * 2;
        const int bh = fm.height() + padY * 2;
        const int bx = originX + (viewW - bw) / 2;
        // 浮在底部技能 Chip 行与「Tab 提示」上方，避免与底部技能栏重叠
        const int by = originY + viewH - 64 - bh - 6;
        const int pulse = int(190 + 55 * (0.5 + 0.5 * std::sin(session_.time() * 4.f)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 10, 46, 205));
        painter.drawRoundedRect(QRect(bx, by, bw, bh), 7, 7);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(220, 120, 232, pulse));
        painter.drawText(QRect(bx + padX, by, textW, bh), Qt::AlignLeft | Qt::AlignVCenter, curseText);
        painter.setPen(QColor(228, 212, 188));
    }
    drawBossBar(painter, view);
    drawSanBar(painter, view);
    drawRadar(painter, view);
    drawMazeMap(painter, view);
    struct Chip {
        QString key;
        QString name;
        QString sub;
        float remain = 0.f;
        float maxCd = 0.f;
    };
    QVector<Chip> chips;
    const float mul = session_.cooldownMul();
    auto skillCdMax = [&](int skill) { return skillCooldownMax(skill, mul, player.atomicCd); };
    auto pushSkillChip = [&](const QString& key, int skill, float slotCd) {
        Chip chip;
        chip.key = key;
        chip.name = skillText(skill).name;
        if (skill == kSkillSwordQi) {
            chip.remain = player.stacksQi < 3 ? player.cdQiStack : 0.f;
            chip.maxCd = skillCdMax(skill);
            chip.sub = player.stacksQi < 3
                ? QString("%1/3 %2s").arg(player.stacksQi).arg(std::max(0.f, player.cdQiStack), 0, 'f', 1)
                : QString("%1/3").arg(player.stacksQi);
        } else if (skill == kSkillThrust) {
            chip.remain = player.stacksThrust < 3 ? player.cdThrustStack : 0.f;
            chip.maxCd = skillCdMax(skill);
            chip.sub = player.stacksThrust < 3
                ? QString("%1/3 %2s").arg(player.stacksThrust).arg(std::max(0.f, player.cdThrustStack), 0, 'f', 1)
                : QString("%1/3").arg(player.stacksThrust);
        } else if (skill == kSkillFlight || skill == kSkillJetpack) {
            chip.sub = player.flying ? QString("飞行中") : QString("关");
        } else if (skill == kSkillSeek) {
            chip.sub = player.seekOn ? QString("开") : QString("关");
        } else if (skill == kSkillMirror && player.mirrorT > 0.f) {
            chip.sub = QString("%1/%2").arg(int(player.mirrorAbsorbed)).arg(int(player.mirrorCap));
            chip.remain = player.mirrorT;
            chip.maxCd = 10.f;
        } else if (skill == kSkillBerserk && player.berserkT > 0.f) {
            chip.sub = QString("%1 %2s").arg(skillText(skill).name).arg(player.berserkT, 0, 'f', 1);
            chip.remain = player.berserkT;
            chip.maxCd = 3.f;
        } else if ((skill == kSkillOverload && player.overloadT > 0.f) || (skill == kSkillMagField && player.fieldT > 0.f)
            || (skill == kSkillMedkit && player.medkitT > 0.f)) {
            const float active = skill == kSkillOverload ? player.overloadT : skill == kSkillMagField ? player.fieldT : player.medkitT;
            chip.remain = std::max(0.f, slotCd);
            chip.maxCd = skillCdMax(skill);
            chip.sub = QString("生效 %1s").arg(active, 0, 'f', 1);
        } else if (skill == kSkillSwarm && !session_.drones().empty()) {
            chip.remain = std::max(0.f, slotCd);
            chip.maxCd = skillCdMax(skill);
            chip.sub = QString("%1架 %2s").arg(session_.drones().size()).arg(chip.remain, 0, 'f', 1);
        } else {
            chip.remain = std::max(0.f, slotCd);
            chip.maxCd = skillCdMax(skill);
            chip.sub = QString("%1s").arg(chip.remain, 0, 'f', 1);
        }
        chips.push_back(chip);
    };
    if (player.hero == HeroClass::Robot) {
        Chip ammo;
        ammo.key = "弹";
        ammo.name = "弹匣";
        ammo.remain = float(kRobotMagazine - player.ammo);
        ammo.maxCd = float(kRobotMagazine);
        ammo.sub = player.ammo > 0 ? QString("%1/%2").arg(player.ammo).arg(kRobotMagazine) : QString("空 长按换弹");
        chips.push_back(ammo);
    }
    {
        Chip q;
        q.key = "Q";
        q.name = guardSkillText().name;
        q.remain = std::max(0.f, player.cdGuard);
        q.maxCd = 3.6f * mul;
        q.sub = QString("%1s").arg(q.remain, 0, 'f', 1);
        chips.push_back(q);
    }
    {
        Chip e;
        e.key = "E";
        e.name = healSkillText().name;
        e.remain = std::max(0.f, player.cdHeal);
        e.maxCd = 5.5f * mul;
        e.sub = QString("%1s").arg(e.remain, 0, 'f', 1);
        chips.push_back(e);
    }
    pushSkillChip("R", player.skillD, player.cdD);
    pushSkillChip("F", player.skillF, player.cdF);
    pushSkillChip("C", player.skillC, player.cdC);
    if (player.hero == HeroClass::Mage && player.skillV >= 0) {
        pushSkillChip("V", player.skillV, player.cdV);
    }
    if (player.talentGuide) {
        pushSkillChip("G", kSkillSeek, 0.f);
    }
    if (touchUi_) {
        chips.clear();
    }
    const int gap = 6;
    int chipW = 84;
    const int chipH = 38;
    if (!chips.isEmpty()) {
        const int maxW = viewW - 8;
        const int gaps = (chips.size() - 1) * gap;
        if (chips.size() * chipW + gaps > maxW) {
            chipW = std::max(56, (maxW - gaps) / int(chips.size()));
        }
    }
    const int rowW = chips.isEmpty() ? 0 : int(chips.size()) * chipW + int(chips.size() - 1) * gap;
    int chipX = originX + (viewW - rowW) / 2;
    const int chipY = originY + viewH - chipH - 10;
    painter.setFont(QFont(Platform::uiFontFamily(), 10));
    painter.setBrush(Qt::NoBrush);
    for (const Chip& chip : chips) {
        painter.fillRect(QRect(chipX, chipY, chipW, chipH), QColor(12, 10, 9, 200));
        painter.setPen(QColor(90, 58, 50));
        painter.drawRect(QRect(chipX, chipY, chipW - 1, chipH - 1));
        painter.setPen(QColor(196, 92, 72));
        painter.drawText(QRect(chipX + 4, chipY + 2, 18, 14), Qt::AlignLeft | Qt::AlignVCenter, chip.key);
        painter.setPen(QColor(228, 212, 188));
        const bool longName = painter.fontMetrics().horizontalAdvance(chip.name) > chipW - 26;
        if (longName) {
            painter.setFont(QFont(Platform::uiFontFamily(), 7));
        }
        painter.drawText(QRect(chipX + 22, chipY + 2, chipW - 26, 14), Qt::AlignLeft | Qt::AlignVCenter, chip.name);
        if (longName) {
            painter.setFont(QFont(Platform::uiFontFamily(), 10));
        }
        painter.setPen(QColor(138, 148, 120));
        painter.drawText(QRect(chipX + 4, chipY + 16, chipW - 8, 14), Qt::AlignLeft | Qt::AlignVCenter, chip.sub);
        if (chip.maxCd > 0.01f) {
            const float ready = chip.remain <= 0.01f ? 1.f : 1.f - std::clamp(chip.remain / chip.maxCd, 0.f, 1.f);
            painter.fillRect(QRect(chipX + 2, chipY + chipH - 4, chipW - 4, 2), QColor(40, 32, 28, 200));
            painter.fillRect(QRect(chipX + 2, chipY + chipH - 4, int((chipW - 4) * ready), 2), QColor(196, 92, 72, 200));
        }
        chipX += chipW + gap;
    }
    painter.setPen(QColor(138, 122, 108));
    if (!touchUi_) {
        painter.drawText(QRect(originX, chipY - 16, viewW, 14), Qt::AlignHCenter, "Tab 查看技能与天赋");
    }
    if (toastTime_ > 0.f) {
        painter.drawText(QRect(originX, originY + 40, viewW, 24), Qt::AlignHCenter, toast_);
    }
    // 吟唱字幕挂在角色头顶，一个词一个词往外蹦；画在这里是为了压在白闪之上
    const QString chant = session_.atomicChant();
    if (!chant.isEmpty()) {
        float shakeX = 0.f;
        float shakeY = 0.f;
        session_.cameraShake(shakeX, shakeY);
        const float scale = float(viewW) / float(kViewW);
        // 相机始终把角色摆在画面正中，唯一的偏移来自震屏
        const float heroX = originX + (kViewW * 0.5f - shakeX) * scale;
        const float heroY = originY + (kViewH * 0.5f - shakeY) * scale;
        QFont font(Platform::uiFontFamily(), int(10.f * scale));
        font.setBold(true);
        painter.setFont(font);
        const QRectF box(heroX - 150.f * scale, heroY - 64.f * scale, 300.f * scale, 24.f * scale);
        painter.setPen(QColor(10, 4, 18, 230));
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dy = -2; dy <= 2; ++dy) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                painter.drawText(box.translated(dx * scale, dy * scale), Qt::AlignCenter, chant);
            }
        }
        painter.setPen(QColor(232, 204, 255));
        painter.drawText(box, Qt::AlignCenter, chant);
    }
    if (!spritesOk_) {
        painter.drawText(QRect(originX, originY + viewH - 28, viewW, 20), Qt::AlignCenter, "未找到像素图，当前用色块代替");
    }
    if (touchUi_ && running_ && !session_.ended()) {
        drawTouchControls(painter);
    }
}
