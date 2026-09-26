#include "GameWidget.h"

#include "Audio.h"
#include "Codex.h"
#include "Storage.h"
#include "TileMap.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QScrollArea>
#include <QRandomGenerator>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>

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
    setMinimumSize(960, 540);
    spritesOk_ = sprites_.load(findAssets());
    {
        const AppSettings settings = Storage::loadSettings();
        Audio::instance().load(findAssets());
        Audio::instance().setSfxEnabled(settings.sfxEnabled);
        Audio::instance().setSfxVolume(settings.sfxVolume);
        Audio::instance().setBgmEnabled(settings.bgmEnabled);
        Audio::instance().setBgmVolume(settings.bgmVolume);
    }

    pausePanel_ = new QWidget(this);
    auto* pauseLayout = new QVBoxLayout(pausePanel_);
    pauseLayout->setContentsMargins(18, 18, 18, 18);
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
    auto* scroll = new QScrollArea(pausePanel_);
    scroll->setWidget(guideText_);
    scroll->setWidgetResizable(false);
    scroll->setFixedSize(420, 280);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    pauseLayout->addWidget(scroll);
    pausePanel_->setObjectName("panel");
    pausePanel_->setStyleSheet("QWidget#panel { background: #14110f; border: 1px solid #5c3a32; } QLabel { background: transparent; color: #d7c7b4; border: none; }");
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

    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, [this] {
        if (session_.ended()) {
            return;
        }
        setPaused(!session_.paused());
        if (!session_.paused()) {
            setFocus();
        }
    });
    auto* tab = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    tab->setContext(Qt::WidgetWithChildrenShortcut);
    connect(tab, &QShortcut::activated, this, [this] {
        if (session_.ended()) {
            return;
        }
        setPaused(!session_.paused());
        if (!session_.paused()) {
            setFocus();
        }
    });

    timer_ = new QTimer(this);
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
    html += block("鼠标", lightAttackText(), "");
    html += block("长按", heavyAttackText(), "");
    html += block("Q", guardSkillText(), "冷却 " + cdText(player.cdGuard));
    html += block("E", healSkillText(), "冷却 " + cdText(player.cdHeal));
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
    html += block("R", skillText(player.skillD), skillExtra(player.skillD, player.cdD));
    html += block("F", skillText(player.skillF), skillExtra(player.skillF, player.cdF));
    html += block("C", skillText(player.skillC), skillExtra(player.skillC, player.cdC));
    if (player.hero == HeroClass::Mage && player.skillV >= 0) {
        html += block("V", skillText(player.skillV), skillExtra(player.skillV, player.cdV));
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
    accumulator_ = 0.f;
    input_.clearHeld();
    pausePanel_->hide();
    resultPanel_->hide();
    Audio::instance().startBgmLoop();
    setFocus();
}

void GameWidget::startContinue(const QJsonObject& game) {
    running_ = true;
    session_.loadFrom(game);
    endCommitted_ = false;
    accumulator_ = 0.f;
    input_.clearHeld();
    pausePanel_->hide();
    resultPanel_->hide();
    Audio::instance().startBgmLoop();
    setFocus();
}

void GameWidget::setPaused(bool paused) {
    if (session_.ended()) {
        return;
    }
    session_.setPaused(paused);
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
    const auto answer = QMessageBox::question(this, "结算", "结束本局并记下用时和积分？");
    if (answer != QMessageBox::Yes) {
        return;
    }
    session_.settle();
    commitEnd();
}

void GameWidget::commitEnd() {
    if (!session_.ended() || endCommitted_) {
        return;
    }
    endCommitted_ = true;
    session_.setPaused(false);
    pausePanel_->hide();
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
    pausePanel_->adjustSize();
    pausePanel_->move((width() - pausePanel_->width()) / 2, (height() - pausePanel_->height()) / 2);
    resultPanel_->adjustSize();
    resultPanel_->move((width() - resultPanel_->width()) / 2, (height() - resultPanel_->height()) / 2);
}

void GameWidget::tick() {
    const float frame = std::min(0.05f, float(clock_.restart()) / 1000.f);
    toastTime_ = std::max(0.f, toastTime_ - frame);
    if (running_ && !session_.paused() && !session_.ended()) {
        const QPointF world = mouseWorld();
        accumulator_ += frame;
        int steps = 0;
        while (accumulator_ >= kSimDt && steps < 5) {
            session_.update(kSimDt, input_, float(world.x()), float(world.y()));
            for (SfxId id : session_.drainSfx()) {
                Audio::instance().play(id);
            }
            if (session_.consumeRecoverBgm()) {
                Audio::instance().playRecoverBgm();
            }
            accumulator_ -= kSimDt;
            input_.clearEdges();
            steps += 1;
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

int GameWidget::viewScale(int& originX, int& originY) const {
    const int scale = std::max(1, std::min(width() / kViewW, height() / kViewH));
    originX = (width() - kViewW * scale) / 2;
    originY = (height() - kViewH * scale) / 2;
    return scale;
}

QPointF GameWidget::mouseWorld() const {
    int originX = 0;
    int originY = 0;
    const int scale = viewScale(originX, originY);
    const QPoint local = mapFromGlobal(QCursor::pos());
    const float canvasX = (local.x() - originX) / float(scale);
    const float canvasY = (local.y() - originY) / float(scale);
    const float cameraX = session_.player().x - kViewW * 0.5f;
    const float cameraY = session_.player().y - kViewH * 0.5f;
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
    if (event->button() == Qt::LeftButton) {
        input_.lmb = true;
        input_.lmbEdge = true;
    } else if (event->button() == Qt::RightButton) {
        input_.rmb = true;
        input_.rmbEdge = true;
    }
}

void GameWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        input_.lmb = false;
        input_.lmbUp = true;
    } else if (event->button() == Qt::RightButton) {
        input_.rmb = false;
    }
}

void GameWidget::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    input_.clearHeld();
}

void GameWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
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
            painter.setFont(QFont("Microsoft YaHei UI", 8, QFont::Bold));
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
        painter.setFont(QFont("Microsoft YaHei UI", text.crit ? 11 : 8, text.crit ? QFont::Bold : QFont::Normal));
        painter.setPen(QColor(0, 0, 0, 180));
        painter.drawText(QRectF(text.x - 28, text.y - 7, 56, 14), Qt::AlignCenter, label);
        painter.setPen(text.crit ? QColor(255, 220, 40) : QColor(255, 245, 230));
        painter.drawText(QRectF(text.x - 28, text.y - 8, 56, 14), Qt::AlignCenter, label);
    }
}

void GameWidget::drawRadar(QPainter& painter, int originX, int originY, int scale) {
    const Player& player = session_.player();
    constexpr int kRadar = 112;
    constexpr float kRange = 320.f;
    const int radarX = originX + kViewW * scale - kRadar - 12;
    const int radarY = originY + 34;
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
    painter.setFont(QFont("Microsoft YaHei UI", 11));
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void GameWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(8, 7, 6));
    int originX = 0;
    int originY = 0;
    const int scale = viewScale(originX, originY);

    QImage canvas(kViewW, kViewH, QImage::Format_ARGB32);
    canvas.fill(QColor(16, 14, 12));
    {
        QPainter world(&canvas);
        world.setRenderHint(QPainter::SmoothPixmapTransform, false);
        drawWorld(world);
    }
    {
        QPainter grade(&canvas);
        grade.setCompositionMode(QPainter::CompositionMode_Multiply);
        grade.fillRect(canvas.rect(), QColor(128, 112, 98));
        QRadialGradient vig(kViewW * 0.5, kViewH * 0.5, kViewW * 0.72);
        vig.setColorAt(0.42, QColor(0, 0, 0, 0));
        vig.setColorAt(1.0, QColor(0, 0, 0, 150));
        grade.setCompositionMode(QPainter::CompositionMode_SourceOver);
        grade.fillRect(canvas.rect(), vig);
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRect(originX, originY, kViewW * scale, kViewH * scale), canvas);

    painter.setPen(QColor(90, 58, 50));
    painter.drawRect(QRect(originX, originY, kViewW * scale - 1, kViewH * scale - 1));
    const Player& player = session_.player();
    auto shown = [](float value) { return value <= 0.f ? 0 : int(std::ceil(value)); };
    painter.setFont(QFont("Microsoft YaHei UI", 11));
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
    painter.drawText(QRect(originX, originY + 10, kViewW * scale, 22), Qt::AlignHCenter | Qt::AlignTop, formatTime(session_.time()));
    painter.drawText(QRect(originX, originY + 10, kViewW * scale - 16, 22), Qt::AlignRight | Qt::AlignTop, QString("SCORE %1").arg(session_.score()));
    drawRadar(painter, originX, originY, scale);
    struct Chip {
        QString key;
        QString name;
        QString sub;
        float remain = 0.f;
        float maxCd = 0.f;
    };
    QVector<Chip> chips;
    const float mul = session_.cooldownMul();
    auto skillCdMax = [&](int skill) -> float {
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
    };
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
    const int chipW = 84;
    const int chipH = 38;
    const int gap = 6;
    const int rowW = int(chips.size()) * chipW + int(chips.size() - 1) * gap;
    int chipX = originX + (kViewW * scale - rowW) / 2;
    const int chipY = originY + kViewH * scale - chipH - 10;
    painter.setFont(QFont("Microsoft YaHei UI", 10));
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
    painter.drawText(QRect(originX, chipY - 16, kViewW * scale, 14), Qt::AlignHCenter, "Tab 查看技能与天赋");
    if (toastTime_ > 0.f) {
        painter.drawText(QRect(originX, originY + 40, kViewW * scale, 24), Qt::AlignHCenter, toast_);
    }
    if (!spritesOk_) {
        painter.drawText(QRect(originX, originY + kViewH * scale - 28, kViewW * scale, 20), Qt::AlignCenter, "未找到像素图，当前用色块代替");
    }
}
