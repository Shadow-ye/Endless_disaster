#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace Platform {
// 可能存放游戏资源的根目录：可执行文件所在目录（Windows / 开发构建），
// 以及 ../share/endless-disaster（Linux 安装包 / AppImage）；Android 资源打包在 qrc（":"）。
QStringList dataRoots();

// 本机可用的界面中文字体（Windows 优先微软雅黑，Linux 回退到 Noto CJK / 文泉驿等）。
QString uiFontFamily();

// 标题用的楷体类字体，找不到时回退到界面字体。
QString titleFontFamily();

// 是否默认使用触屏操作（Android；桌面可用环境变量 ENDLESS_TOUCH_UI=1 预览）。
bool touchUi();

// 必须在创建 QApplication 之前调用：Android 手机上整体缩小界面，保证横屏逻辑高度够用。
void configureDisplay();

// 多媒体后端可直接播放的地址；qrc 内的文件会先解到缓存目录。
QUrl mediaUrl(const QString& path);
}  // namespace Platform
