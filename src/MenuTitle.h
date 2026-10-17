#pragma once

#include <QElapsedTimer>
#include <QFont>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

class QTimer;

// 主菜单标题：还原展示页（site/）的标题效果——白字压在深红立体投影上，
// 外面裹一层红色辉光，整体偶尔闪一下，并每隔 4.2 秒来一次青 / 红错位的信号故障。
// 字号与字体由外面按窗口宽度算好（见 MainWindow::layoutMenuTitle），这里只管画。
//
// 文字一律走 QPainter::drawText：QPainterPath::addText 只认单一字体、不做字体回退，
// 中文会整片渲染成方框（豆腐块）。
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
    // 重排并重建静态层缓存（辉光 + 投影 + 白字），尺寸或字体变了才做
    void rebuild();
    QRectF textBox() const;

    QString text_ = QStringLiteral("无尽之灾");
    QFont font_;
    // 按墨迹（而非排版宽度）居中所需的水平补偿：让标题左右边界能和下面的字幕对齐
    qreal inkShiftX_ = 0.0;
    QPixmap cache_;
    QSize cacheSize_;
    QFont cacheFont_;
    QTimer* timer_ = nullptr;
    QElapsedTimer clock_;
    qreal elapsed_ = 0.0;
};
