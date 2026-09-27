#pragma once

#include <QString>
#include <QStringList>

namespace Platform {
// 可能存放游戏资源的根目录：可执行文件所在目录（Windows / 开发构建），
// 以及 ../share/endless-disaster（Linux 安装包 / AppImage）。
QStringList dataRoots();

// 本机可用的界面中文字体（Windows 优先微软雅黑，Linux 回退到 Noto CJK / 文泉驿等）。
QString uiFontFamily();

// 标题用的楷体类字体，找不到时回退到界面字体。
QString titleFontFamily();
}  // namespace Platform
