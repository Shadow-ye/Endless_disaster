#pragma once

#include "Session.h"
#include "Sprites.h"

#include <QVBoxLayout>
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QIcon>
#include <QJsonObject>
#include <QPointer>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QPainter;

class QAbstractButton;
class QCheckBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSlider;
class QTimer;
class QTouchEvent;

class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    void startNew(HeroClass hero, int skillD, int skillF, int skillC, int skillV = -1);
    void startContinue(const QJsonObject& game);

signals:
    void returnedToMenu();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class TouchControl {
        None,
        Stick,
        Attack,
        Dodge,
        Jump,
        Guard,
        Heal,
        SkillR,
        SkillF,
        SkillC,
        SkillV,
        Guide,
        Pause,
        AutoAim,
        Seek,
        Mimic,
        MimicSkill
    };
    struct TouchButton {
        TouchControl control = TouchControl::None;
        QPointF center;
        qreal radius = 0.0;
        QString label;
    };

    void tick();
    void setPaused(bool paused);
    void togglePause();
    void refreshGuide();
    void leaveToMenu();
    void saveGame();
    void askSettle();
    void showVoidPrompt();
    void showRevivePrompt();
    void commitEnd();
    // 史莱姆之躯的拟态图鉴：T 键 / 触屏按钮开关，打开时时停
    void toggleMimicPanel();
    void closeMimicPanel();
    void refreshMimicPanel();
    void layoutOverlays();
    void syncKey(int key, bool down);
    QRect viewRect() const;
    QPointF mouseWorld() const;
    void drawWorld(QPainter& painter);
    void drawBossBar(QPainter& painter, const QRect& view);
    // 浅水 boss 房的血条：与克苏鲁之眼共用底板几何，配色换成腐蚀绿
    void drawSlimeBossBar(QPainter& painter, const QRect& view, const Monster& boss);
    // boss 血条的底板几何：drawBossBar 与 drawSanBar 共用，保证理智条与血条严格对齐
    QRect bossBarPlate(const QRect& view) const;
    // 理智（SAN）条：boss 激活后显示，距耗尽时间 / 虚弱提示
    void drawSanBar(QPainter& painter, const QRect& view);
    void drawRadar(QPainter& painter, const QRect& view);
    // 迷宫地图的落点：触屏放雷达左侧，避开右下角的攻击与技能键
    QRect mazeMapRect(const QRect& view) const;
    void drawMazeMap(QPainter& painter, const QRect& view);

    qreal touchUnit() const;
    QPointF stickHome() const;
    QVector<TouchButton> touchButtons() const;
    void handleTouch(QTouchEvent* event);
    QAbstractButton* overlayButtonAt(const QPointF& pos) const;
    QPointF clampStickCenter(const QPointF& pos) const;
    void updateStick(const QPointF& pos);
    void releaseStick();
    void releaseBinding(int id);
    void pressTouch(TouchControl control, bool down);
    void releaseAllTouches();
    void toggleAutoAim();
    bool aimLockEnabled() const;
    void updateAimTarget();
    const Monster* aimTarget() const;
    const QImage& touchSprite(qreal radius, const QColor& rim, const QColor& fill, const QString& label, int fontPx, const QColor& textColor);
    void drawTouchControls(QPainter& painter);
    void spawnVfx(const VfxEvent& event);
    void updateParticles(float dt);
    void drawParticles(QPainter& painter);
    float vfxRand();
    void drawShadow(QPainter& painter, float x, float y);
    // 拟态成 boss 时的本体（比原 boss 小一圈、不带血条铭牌）
    void drawBossMimic(QPainter& painter, MimicForm form, float x, float y, float animT, float scale, bool hurt);
    // 图鉴按钮上的小图标
    QIcon mimicIcon(MimicForm form);
    void drawAnim(QPainter& painter, const SpriteAnim& anim, ActorState state, float animT, float x, float y, bool flip, bool moving);
    static int frameIndex(const SpriteAnim& anim, float time, bool loop, float fps);

    // z 为离地高度；落地后弹跳减速
    struct Particle {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float vz = 0.f;
        float gravity = 0.f;
        float life = 0.f;
        float maxLife = 1.f;
        float size = 2.f;
        QRgb color = 0;
        bool glow = false;
    };

    Session session_;
    std::vector<Particle> particles_;
    // 独立于 Session 的随机数，避免粒子扰动玩法随机序列
    uint32_t vfxRng_ = 0x9E3779B9u;
    SpriteSet sprites_;
    bool spritesOk_ = false;
    QImage canvas_;
    QImage vignette_;
    QTimer* timer_ = nullptr;
    QElapsedTimer clock_;
    float toastTime_ = 0.f;
    QString toast_;
    // I am atomic 引爆前的全屏白闪：只叠在画面上，不参与任何判定
    float flashT_ = 0.f;
    float flashMax_ = 1.f;
    InputState input_;
    bool endCommitted_ = false;
    bool running_ = false;

    bool touchUi_ = false;
    QHash<int, TouchControl> touchBindings_;
    QHash<int, QPointer<QAbstractButton>> overlayPresses_;
    int stickTouchId_ = -1;
    QPointF stickCenter_;
    QPointF stickOffset_;
    QPointF aimDir_{1.0, 0.0};
    bool autoAim_ = true;
    int aimTargetId_ = -1;
    QHash<QString, QImage> touchSprites_;

    QWidget* pausePanel_ = nullptr;
    QCheckBox* pauseSfxCheck_ = nullptr;
    QSlider* pauseSfxSlider_ = nullptr;
    QLabel* pauseSfxValue_ = nullptr;
    QCheckBox* pauseBgmCheck_ = nullptr;
    QSlider* pauseBgmSlider_ = nullptr;
    QLabel* pauseBgmValue_ = nullptr;
    QLabel* guideText_ = nullptr;
    QScrollArea* guideScroll_ = nullptr;
    QWidget* resultPanel_ = nullptr;
    QLabel* resultText_ = nullptr;
    QWidget* confirmPanel_ = nullptr;
    QWidget* voidPanel_ = nullptr;
    QWidget* revivePanel_ = nullptr;
    QLabel* reviveText_ = nullptr;
    QPushButton* reviveCore_ = nullptr;
    QWidget* mimicPanel_ = nullptr;
    QLabel* mimicLabel_ = nullptr;
    QVector<QPushButton*> mimicButtons_;
    bool mimicOpen_ = false;
};
