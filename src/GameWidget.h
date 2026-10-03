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

    // 开局准备的分步接口：MainWindow 的加载页逐步调用，把开局重活摊到多帧执行。
    // prepareNew / prepareContinue 做环境准备（含切回 1 号 BGM），随后按
    // prepareNewStepReset → prepareNewStepSpawns → prepareNewStepRuin 推进，
    // 最后 prepareFinish 预热首帧并正式开始。startNew / startContinue 内部就是这一套。
    // 加载页一露头就调用：竖起准备守卫并复位界面，避免准备步骤执行前的那几帧
    // 还在跑上一局的模拟
    void beginPrepare();
    void prepareNew(HeroClass hero, int skillD, int skillF, int skillC, int skillV = -1);
    void prepareNewStepReset();
    void prepareNewStepSpawns();
    void prepareNewStepRuin();
    void prepareContinue(const QJsonObject& game);
    void prepareFinish();
    // 真正开始本局：切到游戏画面的那一刻调用，计时与模拟都从这里起
    void startRun();

signals:
    void returnedToMenu();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
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
    // 隐藏全部叠加面板并复位相关标志（暂停 / 结算 / 确认 / 虚空 / 回归 / 拟态）
    void resetOverlays();
    // paintEvent 里的世界渲染段：抽出来供加载页做首帧预热，把冷启动开销提前付掉
    void renderFrameToCanvas();
    // 预热 HUD 文字的字形光栅化缓存，避免进入游戏后第一帧掉一下
    void warmUpHud();
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
    // 有界面弹出时压低 BGM 音量（不停播），全部关掉后恢复；结算面板不算，它配的是 5 号终曲
    void updateBgmDuck();
    void syncKey(int key, bool down);
    QRect viewRect() const;
    // 记下指针（鼠标）位置：安卓接上鼠标后，朝向改跟指针走而不是摇杆的移动方向
    void notePointer(const QPoint& local);
    // 当前是否用指针方向瞄准（桌面一直是；安卓要收到过真实鼠标事件）
    bool pointerAim() const;
    // 相机的世界原点：平时跟着角色，次元斩期间锁在起手点（角色才能在画面里跑六芒星）
    QPointF cameraOrigin() const;
    QPointF mouseWorld() const;
    void drawWorld(QPainter& painter);
    // 次元斩的画面层：浅蓝滤镜 + 逐道斩出的蓝刃 + 沿刃线把画面切开错位
    void drawDimensionCuts(QPainter& painter);
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
    // 拟态成 boss 时的本体（比原 boss 小一圈、不带血条铭牌）；
    // gazeX / gazeY 是朝向（攻击方向）单位向量，仅供克苏鲁之眼形态的眼珠跟随
    void drawBossMimic(QPainter& painter, MimicForm form, float x, float y, float animT, float scale, bool hurt,
        float gazeX = 0.f, float gazeY = 0.f);
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
    // 加载页驱动的开局准备中：tick 直接返回，不跑模拟也不刷新画面
    bool preparing_ = false;
    uint32_t pendingSeed_ = 1;
    uint32_t pendingRunId_ = 1;
    HeroClass pendingHero_ = HeroClass::Warrior;
    int pendingSkillD_ = -1;
    int pendingSkillF_ = -1;
    int pendingSkillC_ = -1;
    int pendingSkillV_ = -1;

    bool touchUi_ = false;
    QHash<int, TouchControl> touchBindings_;
    QHash<int, QPointer<QAbstractButton>> overlayPresses_;
    int stickTouchId_ = -1;
    QPointF stickCenter_;
    QPointF stickOffset_;
    QPointF aimDir_{1.0, 0.0};
    // 安卓外接鼠标：朝向跟随指针。默认关，收到真实鼠标事件后打开，手指落回屏幕则交还摇杆
    bool mouseAim_ = false;
    QPoint mousePos_;
    // 「检测到鼠标」的提示只弹一次
    bool mouseAimToasted_ = false;
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
    // 天赋进度面板：默认折叠成一行标题，点标题行（桌面左键 / 触屏点按）才展开明细
    bool talentHudOpen_ = false;
    // 上一帧画出的面板矩形，作为展开 / 收起的热区
    QRect talentHudRect_;
    // 这次点击落在天赋面板上，抬起时也不当作一次攻击
    bool talentClick_ = false;
};
