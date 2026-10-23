#pragma once

#include "Session.h"
#include "Sprites.h"

#include <QVBoxLayout>
#include <array>
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
        MimicSkill,
        Bag
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
    // 解锁成就：首次解锁时在画面上弹一行提示（id 见 Codex.h）
    void unlockAchievement(const QString& id);
    void askSettle();
    void showVoidPrompt();
    void showRevivePrompt();
    void commitEnd();
    // 史莱姆之躯的拟态图鉴：T 键 / 触屏按钮开关，打开时时停
    void toggleMimicPanel();
    void closeMimicPanel();
    void refreshMimicPanel();
    // 背包：B 键 / 触屏右上角「背包」按钮开关（打开时时停，BGM 同设置界面压低不停播），
    // 鼠标悬停或触屏点按物品格看详情，触屏另有面板右上角的 ✕ 关闭键

    void toggleBagPanel();
    void closeBagPanel();
    // 改天（法师技能）：Session 置起请求后弹出天气选择面板，用 Qt 按钮选一种天气；
    // 打开时时停，选完 / ✕ / Esc 关闭；取消不消耗 MP 与冷却
    void openWeatherPanel();
    void closeWeatherPanel();
    void refreshWeatherPanel();
    void pickWeather(WeatherKind kind);
    // 背包面板的几何：绘制与命中判定共用一份计算
    struct BagLayout {
        QRect panel;
        int cell = 0;
        int gap = 6;
        int cols = 5;
        int rows = 1;
        int titleH = 0;
        int pad = 0;
    };
    BagLayout bagLayout(const QRect& view) const;
    // 指针下的背包格子序号；不在任何格子上时返回 -1
    int bagCellAt(const QPoint& viewPos) const;
    // 物品图标（things/ 下的文件）；缺图时返回空图，面板退化成文字首字
    const QImage& itemIcon(int kind);
    // 指针在控件内的位置；触屏且没接鼠标时返回 (-1,-1)（没有悬停这回事）
    QPoint pointerLocal() const;
    void drawBagPanel(QPainter& painter, const QRect& view);
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
    // 抹杀的法阵：五层从地面铺开的圆盘，逐层更大，带俯视 3D 的柱面光墙
    void drawEraseCircles(QPainter& painter, const Player& player);
    // 蜂群锁定的准星：四角括号收拢、扫描线、咬住后的呼吸光框与数据链
    void drawDroneLock(QPainter& painter);
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
    // 画面外的传送平台：在视口边缘标出它在哪一侧、通往哪里、还有多远
    void drawTeleportMarkers(QPainter& painter, const QRect& view);

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
    // 天气：按当前天气与强度铺粒子（雨丝 / 雪片 / 冰雹 / 沙线 / 飞叶 / 溅射），
    // 全部走画布坐标——天气是笼罩整屏的现象，不跟着相机滚动
    void spawnWeather(float dt);
    void updateWeather(float dt);
    // 天气的画面层：大气色调、沙尘横带、暴风雪白幕、粒子、落雷与落雷白闪
    void drawWeather(QPainter& painter);
    void drawLightningBolt(QPainter& painter);
    // 左上角 HUD 里的天气状态条（一行，随天赋面板往下排）
    void drawWeatherHud(QPainter& painter, int x, int y, int w);
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

    // 天气粒子：单独一套数组，不与战斗特效挤占同一个上限。
    // style 见 GameWidget.cpp 里的 WeatherLook 常量（雨丝 / 雪片 / 冰雹 / 沙线 / 飞叶 / 溅射）
    struct WeatherParticle {
        float x = 0.f;
        float y = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float life = 0.f;
        float maxLife = 1.f;
        float size = 2.f;
        float len = 6.f;
        float spin = 0.f;
        int style = 0;
        QRgb color = 0;
    };

    Session session_;
    std::vector<Particle> particles_;
    // 天气粒子与它的生成节流：生成量按「每秒多少颗 × 当前强度」累积，
    // 每一种绘制样式各占一个格子（雨丝 / 雪片 / 冰雹 / 沙线 / 飞叶 / 溅射 / 浮尘）
    std::vector<WeatherParticle> weather_;
    std::array<float, 7> weatherSpawnAcc_{};
    // 天气自己的时间线：台风的阵风、雪花的摇摆都按它走
    float weatherTime_ = 0.f;
    // 独立于 Session 的随机数，避免粒子扰动玩法随机序列
    uint32_t vfxRng_ = 0x9E3779B9u;
    SpriteSet sprites_;
    bool spritesOk_ = false;
    QImage canvas_;
    QImage vignette_;
    // 灾难滤镜：预先压暗过的地图表贴（地面 / 岩石 / 树木），角色与特效贴图保持原亮度
    QImage tilesDark_;
    QTimer* timer_ = nullptr;
    QElapsedTimer clock_;
    // 帧率显示：开关来自设置页（settings.json），开局时读一次；统计走固定窗口，避免数字逐帧抖动
    bool showFps_ = false;
    // 画面亮度百分比（settings.json）：100 为默认，全局叠在游戏画面上，HUD 不受影响
    int brightness_ = 100;
    int fpsFrames_ = 0;        // 当前统计窗口内累计帧数
    double fpsAccumMs_ = 0.0;  // 当前统计窗口内累计耗时(ms)
    float fpsValue_ = 0.f;     // 结算后的显示用帧率
    float frameMs_ = 0.f;      // 结算后的显示用单帧耗时(ms)
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
    // 游戏内设置：画面亮度（同主菜单设置页的那一项）
    QSlider* pauseBrightSlider_ = nullptr;
    QLabel* pauseBrightValue_ = nullptr;
    // 游戏内设置：天气系统开关（同主菜单设置页的那一项），关掉立刻转晴
    QCheckBox* pauseWeatherCheck_ = nullptr;
    QLabel* guideText_ = nullptr;
    QScrollArea* guideScroll_ = nullptr;
    QWidget* resultPanel_ = nullptr;
    QLabel* resultText_ = nullptr;
    QWidget* confirmPanel_ = nullptr;
    QWidget* voidPanel_ = nullptr;
    QWidget* revivePanel_ = nullptr;
    QLabel* reviveText_ = nullptr;
    QLabel* reviveWarn_ = nullptr;
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
    // 背包面板（B 键 / 触屏右上角按钮）：打开时时停，空背包时提示「空空如也」
    bool bagOpen_ = false;
    // 触屏点选的格子（-1 为无），用来在没有鼠标的设备上固定显示物品信息
    int bagSelected_ = -1;
    // 上一次画出的面板矩形，作为点击穿透（不触发攻击）的判据
    QRect bagPanelRect_;
    // 面板右上角的 ✕ 关闭键：手机没有 B 键，必须有个能点的关闭按钮
    QRect bagCloseRect_;
    // 已加载的物品图标缓存，键是 Item::kind
    QHash<int, QImage> itemIcons_;
    // 这次点击落在背包面板上，抬起时也不当作一次攻击
    bool bagClick_ = false;
    // 改天的天气选择面板：七个按钮（晴 + 六种天灾）
    QWidget* weatherPanel_ = nullptr;
    QLabel* weatherLabel_ = nullptr;
    QVector<QPushButton*> weatherButtons_;
    bool weatherPickOpen_ = false;
};
