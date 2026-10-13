#pragma once

#include <QCursor>

// 游戏自带的像素光标（assets/cursor/*.cur）。
// 只在桌面端生效：安卓触屏没有鼠标指针，加载与替换都会直接跳过。
namespace Cursors {
// 加载光标并给整个应用装上替换规则：应在 QApplication 构造之后、窗口显示之前调用。
// 资源缺失时四种光标各自回退成系统默认形状，不影响启动。
void init();

QCursor arrow();      // 默认箭头，菜单与非交互区域
QCursor hand();       // 可点击处（按钮 / 可点区块）
QCursor text();       // 文本输入框
QCursor crosshair();  // 游戏画面里的瞄准指针
}  // namespace Cursors
