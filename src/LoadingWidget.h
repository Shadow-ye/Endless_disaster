#pragma once

#include "Sprites.h"

#include <QElapsedTimer>
#include <QString>
#include <QWidget>

class QPainter;
class QTimer;

// 进入本局前的加载页：纯黑舞台上一条紫色方块扫描带自左向右循环掠过，
// 中央偏下是进度条，条首站着一只会走路的蓝色史莱姆。
// 页面本身不干活，只负责显示：进度与阶段文字由 MainWindow 逐帧喂进来，
// 视觉进度平滑逼近目标值，避免进度条跳变。
class LoadingWidget : public QWidget {
    Q_OBJECT

public:
    explicit LoadingWidget(QWidget* parent = nullptr);

    // 开始一轮加载：重置进度 / 最短停留计时 / 淡入，并启动动画定时器
    void start();
    // 停止动画定时器（离开页面时调用，不占用游戏帧预算）
    void stop();

    // 目标进度（0~1）
    void setProgress(float value);
    // 进度条上方的阶段提示文字
    void setStageText(const QString& text);

signals:
    // 进度走满、已停留够最短时间且脉冲动画播完：可以切到游戏画面了
    void finished();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void tick();
    void drawGrid(QPainter& painter);
    void drawProgress(QPainter& painter, qreal scale);
    void drawSlime(QPainter& painter, qreal scale, qreal x, qreal y, bool walking);

    QTimer* timer_ = nullptr;
    SpriteAnim slimeWalk_;
    SpriteAnim slimeIdle_;
    QElapsedTimer clock_;
    qint64 lastNs_ = 0;
    float animT_ = 0.f;
    // 扫描带左边缘的位置（像素）
    float scanX_ = 0.f;
    float target_ = 0.f;
    float visual_ = 0.f;
    float fadeT_ = 0.f;
    // 进度走满后的紫色脉冲：1 → 0
    float pulseT_ = 0.f;
    QString stageText_;
    bool pulsePlayed_ = false;
    bool done_ = false;
};
