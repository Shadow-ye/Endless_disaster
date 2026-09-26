# 无尽之灾（Endless Disaster）

一款 **肉鸽（Roguelike / Roguelite）** 动作生存游戏：怪物潮汐不断涌来，你在一次次开局中选择职业与技能、成长天赋、冲击更高生存时间与积分。死亡即结束本局，记录会保留最高用时与最高积分。

> 怪物入侵世界，而你要做的是活下去。

## 游戏介绍

- **类型**：2D 俯视角肉鸽动作生存  
- **核心循环**：出发 → 自选职业与技能 → 无尽刷怪求生 → 结算记录 → 再开一局  
- **职业**：战士、女剑客、女魔法师（属性与技能体系不同）  
- **成长**：局内升级与装备；天赋（重手 / 远行 / 轻身 / 熟练）按行为解锁  
- **技能自选**：出发前为键位装配技能（战士/剑客三选三键；法师四键自选）  
- **引擎**：Qt6 Widgets + Multimedia  

本仓库默认 **不包含** `build/` 可执行文件。要直接游玩，请按下方自行编译；或等待 Releases 发布打包版。

## 环境配置

### 系统

- Windows 10 / 11（x64）推荐  
- 需能运行 Qt6 Widgets 程序  

### 依赖

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| CMake | ≥ 3.21 | 生成工程 |
| C++ 编译器 | 支持 C++17 | 如 MSVC、MinGW（MSYS2 UCRT64） |
| Qt6 | Widgets、Multimedia | 运行与部署依赖 |

#### 使用 MSYS2（示例）

```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-multimedia
```

将 `C:/msys64/ucrt64/bin` 加入 `PATH`（以便找到 `cmake`、`ninja`、`windeployqt6`）。

#### 使用官方 Qt 安装器

安装 Qt 6.x，勾选 **Qt Widgets** 与 **Qt Multimedia**，并配置 CMake 能找到 `Qt6Config.cmake`（`CMAKE_PREFIX_PATH` 指向 Qt 安装目录）。

## 编译运行

在仓库根目录执行：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

成功后生成：

```text
build/EndlessDisaster.exe
```

构建后脚本会复制资源（`assets/`、`BGM/`、`character-img/`、故事背景与图标），并尽量用 `windeployqt` 部署 Qt 运行库。

直接启动：

```bash
./build/EndlessDisaster.exe
```

若缺少 DLL，请确认 `PATH` 含 Qt 的 `bin`，或重新完整编译一次以触发部署步骤。

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
| 轻击 | 鼠标左键点按 |
| 重击 | 鼠标左键按住约 0.42 秒后放出（耗体力） |
| 闪避 / 法师闪现 | `Shift` 或 鼠标右键（耗体力；法师为闪现） |
| 跳跃 | `Space`（耗体力） |
| 防御 | `Q` |
| 恢复 | `E` |
| 技能栏 | `R` `F` `C`（法师另有 `V`） |
| 技能与天赋说明 | `Tab` |
| 暂停 | `Esc` |

### 界面提示

- 左上：HP / 护盾 / 体力 / 魔力、等级与经验、护甲与暴击  
- 下方：技能冷却条  
- 右上：小雷达显示附近怪物  
- 头顶昵称：**玩家**  

### 资产说明

角色像素资源基于「小苏早睡」素材改色复刻，详见 [`assets/ART_CREDIT.txt`](assets/ART_CREDIT.txt)。请遵守原素材许可（可改色；勿单独转售素材包）。

## 仓库结构（简要）

```text
src/                 游戏逻辑与界面
assets/              角色、怪物、音效、UI、地图块
BGM/                 背景音乐
CMakeLists.txt       构建与资源拷贝
```

## 许可与声明

代码与工程文件以仓库为准；第三方美术 / 音频请遵循各自授权与署名要求。
