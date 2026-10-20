#pragma once

#include <QString>

class QCheckBox;
class QLabel;
class QSlider;
class QWidget;

// 一条「标题 + 滑块 + 数值」的设置行。主菜单设置页与游戏内暂停菜单共用这一份构造，
// 保证两处的控件、父容器、尺寸约束、触摸路径完全一致（首页原先手搓了一套，
// 安卓上拖动明显不如暂停菜单里那条顺）。
struct SettingRow {
    QWidget* row = nullptr;
    QCheckBox* check = nullptr;  // withCheck=false 时为空，标题改用 QLabel
    QSlider* slider = nullptr;
    QLabel* value = nullptr;
};

SettingRow makeSettingRow(QWidget* parent, const QString& title, int from, int to, int topMargin,
    bool withCheck = true);
