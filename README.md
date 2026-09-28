# 无尽之灾（Endless Disaster）

一款 **肉鸽（Roguelike / Roguelite）** 动作生存游戏：怪物潮汐不断涌来，你在一次次开局中选择职业与技能、成长天赋、冲击更高生存时间与积分。死亡即结束本局，记录会保留最高用时与最高积分。

> 怪物入侵世界，而你要做的是活下去。

## 游戏介绍

- **类型**：2D 俯视角肉鸽动作生存  
- **核心循环**：出发 → 自选职业与技能 → 无尽刷怪求生 → 结算记录 → 再开一局  
- **职业**：战士、女剑客、女魔法师、机甲人（属性与技能体系不同）  
- **成长**：局内升级与装备；天赋（重手 / 远行 / 轻身 / 熟练 / 以小博大）按行为解锁  
- **技能自选**：出发前为键位装配技能（战士/剑客四选三；法师四键自选；机甲人八选三）  
- **机甲人**：8 方向像素角色，普攻为快速点射（弹匣 30 发，长按左键换弹）；可选散射、爆破弹、推进、过载、无人机蜂群、磁力场、战术医疗包、喷气背包  
- **怪物**：史莱姆、骷髅、蘑菇、飞行怪、施法怪，30 秒后出现会绕圈点射的机器人小兵  
- **引擎**：Qt6 Widgets + Multimedia  

本仓库默认 **不包含** `build/` 可执行文件。

### 直接游玩（推荐）

#### Windows

**[Releases · 最新 Windows 包](https://github.com/Shadow-ye/Endless_disaster/releases/latest)**

1. 下载 `EndlessDisaster-windows-x64.zip`（Windows 10 / 11 x64，自带 Qt 与音频解码）  
2. 解压到任意目录  
3. 双击 `EndlessDisaster.exe`

旧的 v0.1.0 包在未安装 MSYS2 的电脑上背景音乐没有声音，请改用最新版本。

#### Linux

**[Releases · 最新 Linux 包](https://github.com/Shadow-ye/Endless_disaster/releases/latest)**

- **AppImage（万能包，推荐）**：下载 `EndlessDisaster-x86_64.AppImage`，然后  
  `chmod +x EndlessDisaster-x86_64.AppImage && ./EndlessDisaster-x86_64.AppImage`  
  自带 Qt 与音频解码，适用于 glibc ≥ 2.35 的 x86_64 发行版（Ubuntu 22.04+、Debian 12+、Fedora 36+ 等）。缺少 FUSE 时安装 `libfuse2`，或设置 `APPIMAGE_EXTRACT_AND_RUN=1` 再运行。
- **Debian / Ubuntu**：下载 `endless-disaster_*_amd64.deb`，执行 `sudo apt install ./endless-disaster_*_amd64.deb`，之后从应用菜单打开「无尽之灾」或运行 `endless-disaster`。

中文出现方块字时，请安装系统中文字体（如 `fonts-noto-cjk`）。

#### Android

**[Releases · 最新安卓包](https://github.com/Shadow-ye/Endless_disaster/releases/latest)**：下载 `EndlessDisaster-android-arm64.apk` 安装（Android 9+，arm64 手机；需允许「安装未知来源应用」）。游戏固定横屏。

| 位置 | 触屏操作 |
|------|----------|
| 左下 | 摇杆：移动，同时决定朝向 / 瞄准（左半屏任意处按下即出现） |
| 右下 | 攻击（点按轻击、长按重击；机甲人长按换弹，按钮下方显示剩余弹药）；内圈闪避 / 防御 / 跳跃；外圈技能与恢复（冷却中显示剩余秒数） |
| 右上 | 索敌（开启后攻击与技能自动朝向最近的敌人，脚下红圈标出锁定目标；闪避仍按摇杆方向）、说明（技能与天赋，同 Tab）、暂停（同 Esc）；系统返回键也可暂停 |

桌面版可用环境变量 `ENDLESS_TOUCH_UI=1` 预览触屏布局。

从 v0.3.1 起新版本可直接覆盖安装并保留存档；手机上若装的是 v0.3.0，需要先卸载一次（该版本使用临时签名）。详见 [安卓签名与覆盖安装说明](docs/android-signing.md)。

### 历史版本

所有版本的安装包都保留在 **[Releases](https://github.com/Shadow-ye/Endless_disaster/releases)** 页面；游戏主菜单右下角的「历史版本」「游戏详情」按钮也会打开这里和本仓库。

| 版本 | 主要内容 |
|------|----------|
| [v0.5.1](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.5.1) | 画面外的怪物跨过岩石和灌木追击，离画面越远越快；天赋「以小博大」：击杀 10 个等级超过自己的怪物后，对更高等级敌人伤害变为 1.3 倍 |
| [v0.5.0](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.5.0) | Windows：迷宫遗迹与克苏鲁之眼；击杀 20 个入侵怪物获得天赋「世界指引」，按 G 寻路；万葬播放配音；出发界面改用角色像素图 |
| [v0.4.0](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.4.0) | 新职业机甲人（8 方向，弹匣与换弹，八选三技能：蜂群、磁力场、医疗包、喷气背包等）；新怪物机器人小兵；修复飞行 / 跳跃落在岩石上卡住 |
| [v0.3.4](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.3.4) | 安卓：自动索敌（可开关），攻击与技能朝向最近的敌人，闪避仍按摇杆方向 |
| [v0.3.3](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.3.3) | 安卓：摇杆不再被另一只手的触屏干扰，左半屏专管摇杆，底座跟随手指 |
| [v0.3.2](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.3.2) | 安卓：修复掉帧、摇杆卡方向、多指时面板点不动；Windows 包改用官方 Qt，BGM 正常播放 |
| [v0.3.1](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.3.1) | 安卓改用固定签名，此后的新版本可直接覆盖安装并保留存档 |
| [v0.3.0](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.3.0) | 首个安卓版本，加入触屏摇杆与按键（临时签名，升级需先卸载） |
| [v0.2.0](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.2.0) | Linux 版：`.deb` 与 AppImage |
| [v0.1.0](https://github.com/Shadow-ye/Endless_disaster/releases/tag/v0.1.0) | Windows 可玩包（无 MSYS2 的电脑上 BGM 无声，建议用新版） |

也可按下方自行从源码编译。

## 环境配置

### 系统

- Windows 10 / 11（x64）推荐  
- 需能运行 Qt6 Widgets 程序  

### 依赖

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| CMake | ≥ 3.21 | 生成工程 |
| C++ 编译器 | 支持 C++17 | Windows 用 Qt 官方 MinGW 13.1 |
| Qt6 | 6.8.3，Widgets、Multimedia | Windows 用官方 Qt（自带精简 LGPL FFmpeg） |
| Ninja | 任意 | 生成器 |

#### Windows：官方 Qt + MinGW（推荐）

用 [aqtinstall](https://github.com/miurahr/aqtinstall) 安装到 E 盘（免登录 Qt 账号）：

```bash
aqt install-qt windows desktop 6.8.3 win64_mingw -m qtmultimedia -O E:\Qt
aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O E:\Qt
```

不建议用 MSYS2 的 Qt 打 Windows 包：它的 FFmpeg 是 GPL 全功能版，依赖约 70 个额外 DLL（约 200 MB），漏拷任何一个 BGM 都会无声。

## 编译运行

在仓库根目录执行：

```powershell
.\packaging\windows\build-windows.ps1
```

脚本会用 `E:\Qt\6.8.3\mingw_64` 与 `E:\Qt\Tools\mingw1310_64` 编译到 `build-windows/`（路径可用 `-QtDir`、`-MinGWDir` 参数修改），构建后步骤复制资源（`assets/`、`BGM/`、`character-img/`、故事背景与图标）并用同一套 Qt 的 `windeployqt` 部署运行库（含 FFmpeg），最后生成：

```text
dist/EndlessDisaster/EndlessDisaster.exe      # 解压即玩的目录（约 78 MB）
dist/EndlessDisaster-windows-x64.zip           # 用于发布（约 45 MB）
```

脚本运行时会把 `PATH` 限定为官方 Qt 与 MinGW，避免混入 MSYS2 等其他 Qt 的 DLL。

### Linux 编译与打包

以 Ubuntu 22.04+ / Debian 12+ 为例：

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-multimedia-dev libgl1-mesa-dev \
  gstreamer1.0-libav gstreamer1.0-plugins-good fonts-noto-cjk
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
./build/endless-disaster            # 直接运行
(cd build && cpack -G DEB)          # 生成 build/endless-disaster_<版本>_amd64.deb
```

安装后程序位于 `/usr/bin/endless-disaster`，资源位于 `/usr/share/endless-disaster/`。

### Android 编译

需要 Qt 6.8（`android_arm64_v8a` 与同版本桌面 Qt）、Android SDK、NDK r26b、JDK 17：

```bash
<Qt>/6.8.3/android_arm64_v8a/bin/qt-cmake -S . -B build-android -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH=<Qt>/6.8.3/gcc_64 -DANDROID_SDK_ROOT=<sdk> -DANDROID_NDK_ROOT=<sdk>/ndk/26.1.10909125
cmake --build build-android --target apk
```

Windows 上可直接运行 `packaging\android\build-apk.ps1`，它会加载正式签名密钥，产物可与 Release 中的 APK 互相覆盖安装。
设置了 `QT_ANDROID_KEYSTORE_*` 环境变量时 CMake 会自动签名，否则 APK 使用调试签名，不能覆盖正式包。

安卓版资源通过 Qt 资源系统（qrc）打包进 APK。

### 自动打包

推送 `v*` 标签时，GitHub Actions（`.github/workflows/packages.yml`）会自动编译 `.deb`、AppImage、安卓 APK 与 Windows zip 并发布到 Release。
发布安卓包必须在仓库 Secrets 中配置 `ANDROID_KEYSTORE_BASE64`、`ANDROID_KEYSTORE_PASSWORD`、`ANDROID_KEY_ALIAS`，否则发布会失败。
密钥的位置、配置方法与交接清单见 [docs/android-signing.md](docs/android-signing.md)。

## 游玩方法

### 主流程

1. 打开游戏，主菜单选择 **出发**  
2. 在 **出发之前** 选择职业，并为技能键装配技能（可点立绘切换职业）  
3. 点击 **进入** 开始本局  
4. 存活越久、击杀越多，积分越高；死亡或主动结算后返回菜单  
5. **继续** 可读取上次存档；**设置** 可开关音效 / BGM 与音量  

### 操作一览

| 操作 | 按键 / 鼠标 |
|------|-------------|
| 移动 | `W` `A` `S` `D` |
| 朝向 / 瞄准 | 鼠标位置 |
| 轻击 | 鼠标左键点按（机甲人每发耗 1 发弹药） |
| 重击 | 鼠标左键按住约 0.42 秒后放出（耗体力）；机甲人没有重击，按住即换弹补满 30 发 |
| 闪避 / 法师闪现 | `Shift` 或 鼠标右键（耗体力；法师为闪现） |
| 跳跃 | `Space`（耗体力） |
| 防御 | `Q` |
| 恢复 | `E` |
| 技能栏 | `R` `F` `C`（法师另有 `V`） |
| 技能与天赋说明 | `Tab` |
| 暂停 | `Esc` |

### 界面提示

- 左上：HP / 护盾 / 体力 / 魔力、等级与经验、护甲与暴击  
- 下方：技能冷却条（机甲人另有弹匣格，显示剩余弹药）  
- 右上：小雷达显示附近怪物  
- 头顶昵称：**玩家**  

### 资产说明

角色像素资源基于「小苏早睡」素材改色复刻，详见 [`assets/ART_CREDIT.txt`](assets/ART_CREDIT.txt)。请遵守原素材许可（可改色；勿单独转售素材包）。
机甲人与机器人小兵使用 CC0 素材（Hormelz「8 - Directional Drone Robot」、patvanmackelberg「Killbot (8 Directional)」），由 `tools/robot_sprites.py` 转换；蜂群无人机由 `tools/drone_sprite.py` 原创绘制。

## 仓库结构（简要）

```text
src/                 游戏逻辑与界面
assets/              角色、怪物、音效、UI、地图块
BGM/                 背景音乐
CMakeLists.txt       构建与资源拷贝
```

## 许可与声明

代码与工程文件以仓库为准；第三方美术 / 音频请遵循各自授权与署名要求。
