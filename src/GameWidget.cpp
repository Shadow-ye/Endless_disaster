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
    case kSkillSwordQi:
        return 1.5f * mul;
    case kSkillThrust:
        return 1.0f * mul;
    default:
        return 0.f;
    }
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
        if (skill == kSkillFlight) {
            return player.flying ? QString("飞行中") : QString("关");
        }
        if (skill == kSkillMirror) {
            return player.mirrorT > 0.f
                ? QString("吸收 %1/%2　剩余 %3s").arg(int(player.mirrorAbsorbed)).arg(int(player.mirrorCap)).arg(player.mirrorT, 0, 'f', 1)
                : "冷却 " + cdText(cd);
        }
        if (skill == kSkillBerserk) {
            return player.berserkT > 0.f
                ? QString("狂化中　剩余 %1s").arg(player.berserkT, 0, 'f', 1)
                : "冷却 " + cdText(cd);
        }
        return "冷却 " + cdText(cd);
    };
    html += block(touch ? "技能" : "R", skillText(player.skillD), skillExtra(player.skillD, player.cdD));
    html += block(touch ? "技能" : "F", skillText(player.skillF), skillExtra(player.skillF, player.cdF));
    html += block(touch ? "技能" : "C", skillText(player.skillC), skillExtra(player.skillC, player.cdC));
    if (player.hero == HeroClass::Mage && player.skillV >= 0) {
        html += block(touch ? "技能" : "V", skillText(player.skillV), skillExtra(player.skillV, player.cdV));
    }
    html += "<p style='color:#e4d4c4; margin:12px 0 4px 0;'><b>天赋</b></p>";
    html += progress("重手", player.damageDealt, 250.f, player.talentMight, talentMightDetail());
    html += progress("远行", player.distanceMoved, 900.f, player.talentStride, talentStrideDetail());
    html += progress("轻身", float(player.dodgeCount), 6.f, player.talentLight, talentLightDetail());
    html += progress("熟练", float(player.skillCasts), 12.f, player.talentMastery, talentMasteryDetail());
    guideText_->setText(html);
    guideText_->adjustSize();
}

void GameWidget::startNew(HeroClass hero, int skillD, int skillF, int skillC, int skillV) {
    running_ = true;
    const uint32_t seed = uint32_t(QRandomGenerator::global()->generate());
    const uint32_t runId = uint32_t(QRandomGenerator::global()->generate());
    session_.newGame(seed == 0 ? 1u : seed, runId == 0 ? 1u : runId, hero, skillD, skillF, skillC, skillV);
    endCommitted_ = false;
    clock_.restart();
    releaseAllTouches();
    pausePanel_->hide();
    resultPanel_->hide();
    confirmPanel_->hide();
    Audio::instance().startBgmLoop();
    setFocus();
}

void GameWidget::startContinue(const QJsonObject& game) {
    running_ = true;
    session_.loadFrom(game);
    endCommitted_ = false;
    clock_.restart();
    releaseAllTouches();
    pausePanel_->hide();
    resultPanel_->hide();
    confirmPanel_->hide();
    Audio::instance().startBgmLoop();
    setFocus();
}

void GameWidget::setPaused(bool paused) {
    if (session_.ended()) {
        return;
    }
    session_.setPaused(paused);
    confirmPanel_->hide();
    pausePanel_->setVisible(paused);
    Audio::instance().setBgmPaused(paused);
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
    if (session_.ended()) {
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
    if (session_.ended()) {
        return;
    }
    pausePanel_->hide();
    confirmPanel_->show();
    confirmPanel_->raise();
    layoutOverlays();
}

void GameWidget::commitEnd() {
    if (!session_.ended() || endCommitted_) {
        return;
    }
    endCommitted_ = true;
    session_.setPaused(false);
    pausePanel_->hide();
    confirmPanel_->hide();
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
    resultText_->setText(reason + "\n用时 " + formatTime(session_.time()) + "\n积分 " + QString::number(session_.score()));
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
}

void GameWidget::tick() {
    const qint64 elapsedNs = clock_.nsecsElapsed();
    clock_.restart();
    const float frame = std::min(0.05f, float(elapsedNs) / 1e9f);
    toastTime_ = std::max(0.f, toastTime_ - frame);
    if (running_ && !session_.paused() && !session_.ended()) {
        const QPointF world = mouseWorld();
        // 按真实帧间隔推进：固定步长又不插值时，定时器与屏幕刷新错拍会出现 0 步 / 2 步交替的顿挫
        const int steps = std::max(1, int(std::ceil(frame / kSimDt - 0.05f)));
        const float dt = frame / float(steps);
        for (int i = 0; i < steps; ++i) {
            session_.update(dt, input_, float(world.x()), float(world.y()));
            for (SfxId id : session_.drainSfx()) {
                Audio::instance().play(id);
            }
            if (session_.consumeRecoverBgm()) {
                Audio::instance().playRecoverBgm();
            }
            input_.clearEdges();
        }
    }
    if (session_.ended()) {
        commitEnd();
    }
    const QString notice = session_.pullNotice();
    if (!notice.isEmpty()) {
        toast_ = notice;
        toastTime_ = 1.5f;
    }
    update();
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
    if (touchUi_) {
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
            } else if (skill == kSkillFlight) {
                sub = player.flying ? QStringLiteral("开") : QStringLiteral("关");
            } else {
                remain = cd;
                maxCd = skillCooldownMax(skill, mul);
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
        default:
            break;
        }

        const bool pressed = held.contains(button.control);
        const QRectF circle(button.center.x() - button.radius, button.center.y() - button.radius,
            button.radius * 2.0, button.radius * 2.0);
        const bool cooling = remain > 0.05f && maxCd > 0.01f;
        const bool withSub = !sub.isEmpty() && !cooling;
        const int labelPx = std::max(9, int(button.radius * (button.label.size() > 2 ? 0.42 : 0.52)));
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
    const float cameraX = player.x - kViewW * 0.5f;
    const float cameraY = player.y - kViewH * 0.5f;
    painter.translate(-cameraX, -cameraY);

    const int x0 = tileOf(cameraX) - 1;
    const int y0 = tileOf(cameraY) - 1;
    const int x1 = tileOf(cameraX + kViewW) + 1;
    const int y1 = tileOf(cameraY + kViewH) + 1;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Tile tile = session_.map().at(x, y);
            if (tile == Tile::Rock || tile == Tile::Bush) {
                drawGround(painter, sprites_.tiles, spritesOk_, Tile::Grass, x, y, session_.map());
            } else {
                drawGround(painter, sprites_.tiles, spritesOk_, tile, x, y, session_.map());
            }
        }
    }
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Tile tile = session_.map().at(x, y);
            if (tile != Tile::Rock && tile != Tile::Bush) {
                continue;
            }
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
        }
    }

    struct DrawItem {
        float y;
        bool player;
        const Monster* monster;
    };
    std::vector<DrawItem> items;
    items.push_back({player.y, true, nullptr});
    for (const Monster& monster : session_.monsters()) {
        items.push_back({monster.y, false, &monster});
    }
    std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) { return a.y < b.y; });

    for (const DrawItem& item : items) {
        if (item.player) {
            drawShadow(painter, player.x, player.y);
            if (player.invuln > 0.f && int(player.animT * 24.f) % 2 == 0 && player.state != ActorState::Dead) {
                continue;
            }
            const SpriteAnim* anim = &sprites_.warriorIdle;
            if (player.hero == HeroClass::Sword) {
                anim = &sprites_.swordIdle;
            } else if (player.hero == HeroClass::Mage) {
                anim = &sprites_.mageIdle;
            }
            if (player.state == ActorState::Dead) {
                anim = player.hero == HeroClass::Sword ? &sprites_.swordDeath
                    : player.hero == HeroClass::Mage ? &sprites_.mageDeath
                                                     : &sprites_.warriorDeath;
            } else if (player.state == ActorState::Attack) {
                anim = player.hero == HeroClass::Sword ? &sprites_.swordAttack
                    : player.hero == HeroClass::Mage ? &sprites_.mageAttack
                                                     : &sprites_.warriorAttack;
            } else if (player.state == ActorState::Hurt) {
                anim = player.hero == HeroClass::Sword ? &sprites_.swordHurt
                    : player.hero == HeroClass::Mage ? &sprites_.mageHurt
                                                     : &sprites_.warriorHurt;
            } else if (player.state == ActorState::Run || player.state == ActorState::Dodge) {
                anim = player.hero == HeroClass::Sword ? &sprites_.swordRun
                    : player.hero == HeroClass::Mage ? &sprites_.mageRun
                                                     : &sprites_.warriorRun;
            }
            const float lift = (player.jumpT > 0.f ? std::sin(player.jumpT / 0.34f * 3.14159f) * 14.f : 0.f)
                + (player.flying ? 12.f : 0.f);
            const float guardCx = player.x;
            const float guardCy = player.y - kGuardCenterAboveFoot - lift;
            if (player.burialT > 0.f) {
                const float t = 1.f - player.burialT / 1.15f;
                const float R = player.burialR * (0.55f + 0.45f * std::min(1.f, t * 1.4f));
                const float spin = player.animT * 2.8f;
                painter.setBrush(Qt::NoBrush);
                for (int ring = 1; ring <= 3; ++ring) {
                    const float rr = R * (0.35f + 0.22f * ring);
                    painter.setPen(QPen(QColor(160, 40, 220, 90 + ring * 30), 1 + (ring == 3 ? 1 : 0)));
                    painter.drawEllipse(QRectF(player.x - rr, player.y - rr, rr * 2.f, rr * 2.f));
                }
                painter.setPen(QPen(QColor(255, 80, 200, 180), 2));
                for (int i = 0; i < 8; ++i) {
                    const float a0 = spin + i * 0.785398f;
                    const float a1 = a0 + 0.35f;
                    painter.drawLine(QPointF(player.x + std::cos(a0) * R * 0.25f, player.y + std::sin(a0) * R * 0.25f),
                        QPointF(player.x + std::cos(a0) * R, player.y + std::sin(a0) * R));
                    painter.drawLine(QPointF(player.x + std::cos(a1) * R * 0.7f, player.y + std::sin(a1) * R * 0.7f),
                        QPointF(player.x + std::cos(a1 + 0.2f) * R * 0.85f, player.y + std::sin(a1 + 0.2f) * R * 0.85f));
                }
                for (int i = 0; i < 12; ++i) {
                    const float a = -spin * 0.7f + i * 0.523599f;
                    const float x0 = player.x + std::cos(a) * R;
                    const float y0 = player.y + std::sin(a) * R;
                    painter.setPen(QPen(QColor(220, 180, 255, 200), 2));
                    painter.drawPoint(QPointF(x0, y0));
                    painter.drawLine(QPointF(x0, y0), QPointF(x0 - std::cos(a) * 6.f, y0 - std::sin(a) * 6.f));
                }
                painter.setPen(QPen(QColor(120, 30, 180, 70), 1));
                painter.drawEllipse(QRectF(player.x - player.burialR, player.y - player.burialR, player.burialR * 2.f, player.burialR * 2.f));
            }
            if (anim->ok()) {
                const bool loop = player.state != ActorState::Attack && player.state != ActorState::Hurt && player.state != ActorState::Dead;
                const float fps = player.state == ActorState::Attack ? 18.f : 12.f;
                const int dir = anim->dirs() >= 4 ? facingDir(player.facingX, player.facingY, anim->dirs()) : 0;
                const bool flip = anim->dirs() < 4 && player.facingX < 0.f;
                const float heroScale = anim->dirs() >= 8 ? 1.f : 1.25f;
                anim->draw(painter, frameIndex(*anim, player.animT, loop, fps), player.x, player.y, flip, heroScale, lift, QColor(), dir);
                // 头顶昵称（三种职业同一逻辑）
                const float nameTop = player.y - lift - 40.f * heroScale;
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
            drawFacingMarker(painter, guardCx, guardCy, player.facingX, player.facingY, kGuardR, attackMarkerColor(player));
        } else {
            const Monster& monster = *item.monster;
            drawShadow(painter, monster.x, monster.y);
            const SpriteAnim* anim = &sprites_.slimeIdle;
            float scale = 1.f;
            float lift = 0.f;
            if (monster.kind == MonsterKind::Slime) {
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
            if (anim->ok()) {
                const bool loop = monster.state != ActorState::Attack && monster.state != ActorState::Hurt && monster.state != ActorState::Dead;
                QColor tint = monster.kind == MonsterKind::Caster ? QColor(88, 42, 112) : QColor();
                if (monster.hurtT > 0.1f) {
                    tint = QColor(255, 220, 80);  // 暴击/重创闪白黄
                }
                anim->draw(painter, frameIndex(*anim, monster.animT, loop, 10.f), monster.x, monster.y, monster.flip, scale, lift, tint);
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
            const QString label = QString("Lv%1  %2").arg(monster.level).arg(hpShown);
            painter.setPen(QColor(0, 0, 0, 200));
            painter.drawText(QRectF(barX - 10, barY - 13, barW + 20, 12), Qt::AlignCenter, label);
            painter.setPen(QColor(255, 240, 220));
            painter.drawText(QRectF(barX - 10, barY - 14, barW + 20, 12), Qt::AlignCenter, label);
        }
    }
    painter.setPen(Qt::NoPen);
    for (const AttackFx& fx : session_.attackFx()) {
        const float u = std::clamp(fx.life / std::max(0.01f, fx.maxLife), 0.f, 1.f);
        const float grow = 1.f - u;
        if (fx.kind == AttackFxKind::Ring || fx.kind == AttackFxKind::Pulse) {
            const float r = fx.radius * (fx.kind == AttackFxKind::Pulse ? (0.35f + 0.65f * grow) : 1.f);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 210, 80, int(60 + 160 * u)), 2));
            painter.drawEllipse(QRectF(fx.x - r, fx.y - r, r * 2.f, r * 2.f));
            painter.setPen(QPen(QColor(255, 255, 200, int(40 + 100 * u)), 1));
            painter.drawEllipse(QRectF(fx.x - r * 0.7f, fx.y - r * 0.7f, r * 1.4f, r * 1.4f));
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
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 210, int(50 + 120 * u)), 1.5));
            painter.drawPath(path);
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
        } else {
            painter.setBrush(bolt.hostile ? QColor(126, 72, 148) : (bolt.crit ? QColor(255, 200, 60) : QColor(214, 196, 160)));
            painter.drawEllipse(QRectF(bolt.x - 3, bolt.y - 3, 6, 6));
        }
    }
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

void GameWidget::drawRadar(QPainter& painter, const QRect& view) {
    const Player& player = session_.player();
    constexpr int kRadar = 112;
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
        default:
            return QColor(220, 80, 70);
        }
    };

    for (const Monster& monster : session_.monsters()) {
        if (monster.state == ActorState::Dead) {
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

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(228, 212, 188));
    painter.setFont(QFont(Platform::uiFontFamily(), 11));
    painter.setRenderHint(QPainter::Antialiasing, false);
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
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(view, canvas_);

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
        painter.drawText(QRect(originX + 20, originY + 138 + index * 16, 200, 16), Qt::AlignLeft | Qt::AlignVCenter,
            QString("%1  %2 / %3").arg(name).arg(int(std::min(current, need))).arg(int(need)));
    };
    painter.fillRect(QRect(originX + 14, originY + 136, 210, 66), QColor(12, 10, 9, 190));
    painter.setPen(QColor(168, 148, 128));
    talentLine(0, "重手", player.damageDealt, 250.f);
    talentLine(1, "远行", player.distanceMoved, 900.f);
    talentLine(2, "轻身", float(player.dodgeCount), 6.f);
    talentLine(3, "熟练", float(player.skillCasts), 12.f);
    painter.setPen(QColor(228, 212, 188));
    painter.drawText(QRect(originX, originY + 10, viewW, 22), Qt::AlignHCenter | Qt::AlignTop, formatTime(session_.time()));
    painter.drawText(QRect(originX, originY + 10, viewW - 16, 22), Qt::AlignRight | Qt::AlignTop, QString("SCORE %1").arg(session_.score()));
    drawRadar(painter, view);
    struct Chip {
        QString key;
        QString name;
        QString sub;
        float remain = 0.f;
        float maxCd = 0.f;
    };
    QVector<Chip> chips;
    const float mul = session_.cooldownMul();
    auto skillCdMax = [&](int skill) { return skillCooldownMax(skill, mul); };
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
        } else if (skill == kSkillFlight) {
            chip.sub = player.flying ? QString("飞行中") : QString("关");
        } else if (skill == kSkillMirror && player.mirrorT > 0.f) {
            chip.sub = QString("%1/%2").arg(int(player.mirrorAbsorbed)).arg(int(player.mirrorCap));
            chip.remain = player.mirrorT;
            chip.maxCd = 10.f;
        } else if (skill == kSkillBerserk && player.berserkT > 0.f) {
            chip.sub = QString("狂化 %1s").arg(player.berserkT, 0, 'f', 1);
            chip.remain = player.berserkT;
            chip.maxCd = 3.f;
        } else {
            chip.remain = std::max(0.f, slotCd);
            chip.maxCd = skillCdMax(skill);
            chip.sub = QString("%1s").arg(chip.remain, 0, 'f', 1);
        }
        chips.push_back(chip);
    };
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
    if (touchUi_) {
        chips.clear();
    }
    const int chipW = 84;
    const int chipH = 38;
    const int gap = 6;
    const int rowW = int(chips.size()) * chipW + int(chips.size() - 1) * gap;
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
        painter.drawText(QRect(chipX + 22, chipY + 2, chipW - 26, 14), Qt::AlignLeft | Qt::AlignVCenter, chip.name);
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
    if (!spritesOk_) {
        painter.drawText(QRect(originX, originY + viewH - 28, viewW, 20), Qt::AlignCenter, "未找到像素图，当前用色块代替");
    }
    if (touchUi_ && running_ && !session_.ended()) {
        drawTouchControls(painter);
    }
}
