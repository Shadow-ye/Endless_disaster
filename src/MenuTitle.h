#pragma once

#include <QElapsedTimer>
#include <QFont>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

class QPainter;
class QTimer;

// 主菜单标题：还原展示页（site/）的标题效果——白字压在深红立体投影上，
// 外面裹一层红色辉光，整体偶尔闪一下，并每隔 4.2 秒来一次青 / 红错位的信号故障。
// 字号与字体由外面按窗口宽度算好（见 MainWindow::layoutMenuTitle），这里只管画。
//
// 两套绘制路径：
//   - 桌面（Windows 等）：辉光 + 投影 + 白字一次性烘进 QPixmap 缓存，每帧只 blit 一次，
//     缩略图放大带来的柔光是标题观感的关键，不要换掉。
//   - 安卓：同一个 painter 里先画半透明字形 / drawImage 之后，白字会整批丢字形
//     （只剩红色投影），所以改成白字先画、辉光最后用 DestinationOver 垫在下面。
// 两条路径都不使用 QPainterPath::addText：它只认单一字体、不做回退，中文会整片豆腐块。
class MenuTitle : public QWidget {
    Q_OBJECT

public:
    explicit MenuTitle(QWidget* parent = nullptr);

    void setText(const QString& text);
    void setTitleFont(const QFont& font);

    // 主菜单的布局对标题是居中对齐，宽度完全按 sizeHint 给：
    // 不写的话标题会被挤成一条窄带，几个字叠在一起
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // 安卓：由不透明背景层（MenuPage）在自己的 paintEvent 里调用。
    // 透明子控件上安卓会把覆盖式绘制的白字整层丢掉，只剩红色投影。
    void paintOn(QPainter& painter);
    // 请求重绘的宿主（MenuPage）：动画帧要连带把底图一起重画
    void setHostWidget(QWidget* host);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // 故障帧：4.2 秒周期里的 88%~95% 各来一帧错位 + 横向裁切（clip-path inset）
    struct GlitchFrame {
        bool visible = false;
        qreal dx = 0.0;
        qreal dy = 0.0;
        qreal alpha = 0.0;
        qreal clipTop = 0.0;
        qreal clipBottom = 0.0;
    };
    static GlitchFrame glitchAt(qreal phase);
    // 整体闪烁：6 秒周期里偶尔压一下透明度
    qreal flicker() const;
    // 重排并重建静态层缓存（辉光 + 投影 + 白字），尺寸或字体变了才做（桌面路径）
    void rebuild();
    // 单独重建辉光层（桌面路径用）
    void rebuildGlowLayer();
    // 只按字体度量算一次墨迹居中补偿（安卓路径用，绝不能碰离屏画布）
    void updateInkShift();
    QRectF textBox() const;
    // 安卓：直接画在控件上，白字在前、辉光垫底
    void paintDirect(QPainter& painter);
    // 桌面：缓存 + blit
    void paintCached(QPainter& painter);

    QString text_ = QStringLiteral("无尽之灾");
    QFont font_;
    QWidget* host_ = nullptr;
    // 按墨迹（而非排版宽度）居中所需的水平补偿：让标题左右边界能和下面的字幕对齐
    qreal inkShiftX_ = 0.0;
    QPixmap cache_;
    QSize cacheSize_;
    QFont cacheFont_;
    // 安卓用的独立辉光缩略图
    QImage glowLayer_;
    QSize glowLayerSize_;
    QFont glowLayerFont_;
    qreal glowLayerDpr_ = 0.0;
    QTimer* timer_ = nullptr;
    QElapsedTimer clock_;
    qreal elapsed_ = 0.0;
};
