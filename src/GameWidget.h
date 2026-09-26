#pragma once

#include "Session.h"
#include "Sprites.h"

#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QWidget>

class QPainter;

class QCheckBox;
class QLabel;
class QSlider;
class QTimer;

class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    void startNew(HeroClass hero, int skillD, int skillF, int skillC, int skillV = -1);
    void startContinue(const QJsonObject& game);

signals:
    void returnedToMenu();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void tick();
    void setPaused(bool paused);
    void refreshGuide();
    void leaveToMenu();
    void saveGame();
    void askSettle();
    void commitEnd();
    void layoutOverlays();
    void syncKey(int key, bool down);
    int viewScale(int& originX, int& originY) const;
    QPointF mouseWorld() const;
    void drawWorld(QPainter& painter);
    void drawRadar(QPainter& painter, int originX, int originY, int scale);
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

    QWidget* pausePanel_ = nullptr;
    QCheckBox* pauseSfxCheck_ = nullptr;
    QSlider* pauseSfxSlider_ = nullptr;
    QLabel* pauseSfxValue_ = nullptr;
    QCheckBox* pauseBgmCheck_ = nullptr;
    QSlider* pauseBgmSlider_ = nullptr;
    QLabel* pauseBgmValue_ = nullptr;
    QLabel* guideText_ = nullptr;
    QWidget* resultPanel_ = nullptr;
    QLabel* resultText_ = nullptr;
};
