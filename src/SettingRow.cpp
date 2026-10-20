#include "SettingRow.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QWidget>

// 设置行里的滑块就是普通 QSlider：桌面是它，安卓也让它走 Qt 的「触摸合成鼠标」，
// 两边同一份代码、同一套重绘路径。
//
// 这里曾经加过一层安卓专属实现：自研触摸映射（WA_AcceptTouchEvents + TouchBegin/Update/End）、
// 16ms 标脏节拍、整行刷新。结果是值在变、画面却冻住不刷，而且"冻住的那一项"还会随控件形状 /
// 顺序换来换去——真正的病根是这张静止页面的局部重绘不一定被提交上屏（见 MainWindow 里
// 每次改动把整页标脏的兜底），不是控件本身。原本那点"卡"是另一回事：每次 valueChanged
// 都同步写盘 + 逐池重建音频，那一部分已经在 MainWindow / GameWidget 里单独修掉了。

// 一条「标题 + 滑块 + 数值」的设置行。主菜单设置页与游戏内暂停菜单共用这一份构造，
// 保证两处的控件、父容器、尺寸约束、触摸路径完全一致。
SettingRow makeSettingRow(QWidget* parent, const QString& title, int from, int to, int topMargin,
    bool withCheck) {
    SettingRow out;
    out.row = new QWidget(parent);
    auto* layout = new QHBoxLayout(out.row);
    layout->setContentsMargins(0, topMargin, 0, 0);
    if (withCheck) {
        out.check = new QCheckBox(title, out.row);
        layout->addWidget(out.check);
    } else {
        auto* label = new QLabel(title, out.row);
        layout->addWidget(label);
    }
    out.slider = new QSlider(Qt::Horizontal, out.row);
    out.slider->setRange(from, to);
    layout->addWidget(out.slider, 1);
    out.value = new QLabel(out.row);
    // 定宽：拖动时只重绘数字，别让文本宽度变化去惊动父布局
    out.value->setFixedWidth(36);
    layout->addWidget(out.value);
    return out;
}
