#pragma once

#include "Session.h"
#include "Sprites.h"

#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QPainter;

class QCheckBox;
class QLabel;
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
    enum class TouchControl { None, Stick, Attack, Dodge, Jump, Guard, Heal, SkillR, SkillF, SkillC, SkillV, Guide, Pause };
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
    void commitEnd();
    void layoutOverlays();
    void syncKey(int key, bool down);
    QRect viewRect() const;
    QPointF mouseWorld() const;
    void drawWorld(QPainter& painter);
    void drawRadar(QPainter& painter, const QRect& view);

    qreal touchUnit() const;
    QPointF stickHome() const;
    QVector<TouchButton> touchButtons() const;
    void handleTouch(QTouchEvent* event);
    void updateStick(const QPointF& pos);
    void releaseStick();
    void pressTouch(TouchControl control, bool down);
    void releaseAllTouches();
    void drawTouchControls(QPainter& painter);
    void drawShadow(QPainter& painter, float x, float y);
    void drawAnim(QPainter& painter, const SpriteAnim& anim, ActorState state, float animT, float x, float y, bool flip, bool moving);
    static int frameIndex(const SpriteAnim& anim, float time, bool loop, float fps);

    Session session_;
    SpriteSet sprites_;
    bool spritesOk_ = false;
    QTimer* timer_ = nullptr;
    QElapsedTimer clock_;
    float accumulator_ = 0.f;
    float toastTime_ = 0.f;
    QString toast_;
    InputState input_;
    bool endCommitted_ = false;
    bool running_ = false;

    bool touchUi_ = false;
    QHash<int, TouchControl> touchBindings_;
    int stickTouchId_ = -1;
    QPointF stickCenter_;
    QPointF stickOffset_;
    QPointF aimDir_{1.0, 0.0};

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
};
