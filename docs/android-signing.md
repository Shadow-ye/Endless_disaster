# 安卓包签名与覆盖安装说明

本文说明《无尽之灾》安卓 APK 怎样才能在手机上**直接覆盖安装（升级）**、签名密钥放在哪、如何本地编译，以及交接时需要移交的东西。

## 1. 覆盖安装的三个条件

安卓只有在同时满足以下三点时，才允许新 APK 覆盖手机上已装的旧版本，并保留存档：

| 条件 | 本项目的做法 |
| --- | --- |
| 包名相同 | 固定为 `io.github.shadowye.endlessdisaster`（`CMakeLists.txt` 中 `QT_ANDROID_PACKAGE_NAME`），**永远不要改** |
| 签名证书相同 | 本地与 GitHub Actions 共用同一把正式密钥（见第 2 节） |
| versionCode 不减小 | 由版本号自动计算：`主版本×10000 + 次版本×100 + 修订号`，例如 0.3.1 → 301；本地与 CI 同版本得到相同的值（特殊情况可用 `-DED_ANDROID_VERSION_CODE=数字` 手动指定） |

不满足时手机会提示“应用未安装 / 与现有软件包存在冲突”，adb 会报：

- `INSTALL_FAILED_UPDATE_INCOMPATIBLE`：签名不一致；
- `INSTALL_FAILED_VERSION_DOWNGRADE`：versionCode 比已装版本小。

游戏存档保存在应用私有目录（`QStandardPaths::AppDataLocation`），**卸载会一并删除**，所以要尽量走覆盖安装。

## 2. 正式签名密钥

密钥由 `packaging/android/new-keystore.ps1` 生成（已生成过，**不要重复生成**），默认位于 `E:\Keys`：

| 文件 | 用途 |
| --- | --- |
| `endless-disaster.keystore` | 密钥库（PKCS12，别名 `endless`，有效期约 27 年） |
| `endless-disaster-signing.ps1` | 本地编译时加载的环境变量，内含密码 |
| `endless-disaster-github-secrets.txt` | 需填入 GitHub 仓库 Secrets 的三项内容 |

当前证书 SHA-256 指纹：

```
E2:FA:50:5B:D0:1C:95:83:E4:E8:30:6C:7C:50:7E:0F:E0:57:CB:0B:02:8A:CC:60:9E:77:19:F8:89:36:A5:50
```

注意事项：

- **一定要把 `E:\Keys` 整个目录备份到安全的地方**（如加密网盘、U 盘）。密钥丢失后，已安装的用户只能卸载重装，没有任何补救办法。
- 这些文件都已被 `.gitignore` 排除，**不要提交到仓库，也不要发到聊天群**。
- 若确实需要换密钥（例如泄露），所有用户都要卸载重装一次；换完后更新本文中的指纹。

## 3. GitHub Actions 配置（一次性）

工作流 `.github/workflows/packages.yml` 从仓库 Secrets 读取密钥：

1. 打开仓库 → **Settings** → **Secrets and variables** → **Actions** → **New repository secret**；
2. 按 `E:\Keys\endless-disaster-github-secrets.txt` 中的内容添加三项：
   - `ANDROID_KEYSTORE_BASE64`：密钥库的 base64；
   - `ANDROID_KEYSTORE_PASSWORD`：密码；
   - `ANDROID_KEY_ALIAS`：`endless`。

以上三项已于 2026-09-27 通过 GitHub CLI 配置完成（`E:\Tools\gh\bin\gh.exe`，配置目录 `E:\Tools\gh\config`，登录凭据在 Windows 凭据管理器）。需要重新写入时：

```powershell
$env:GH_CONFIG_DIR = "E:\Tools\gh\config"
E:\Tools\gh\bin\gh.exe secret list --repo Shadow-ye/Endless_disaster
```

工作流的行为：

- 推送 `v*` 标签发布时，**未配置密钥会直接失败**，保证发出去的 APK 一定能被后续版本覆盖；
- 普通推送到 main 时，未配置密钥会用一次性临时密钥编译（带警告），仅供测试；
- 每次构建的 “APK info” 注释里会打印证书指纹（`cert SHA-256 …`），应与第 2 节的指纹一致（apksigner 输出为不带冒号的小写形式）。

## 4. 本地编译

环境（均在 E 盘）：Qt 6.8.3（`android_arm64_v8a` + `mingw_64`）、Android SDK `E:\Android\android-sdk`、NDK 26.1.10909125、JDK 17 `E:\Android\openjdk\jdk-17.0.12`。

```powershell
# 正式签名，产物可与 GitHub Release 的 APK 互相覆盖
powershell -ExecutionPolicy Bypass -File packaging\android\build-apk.ps1

# 没有密钥时的调试签名包：只能全新安装，不能覆盖正式包
powershell -ExecutionPolicy Bypass -File packaging\android\build-apk.ps1 -DebugSignature
```

- 产物：`dist\EndlessDisaster-android-arm64.apk`，脚本最后会打印证书指纹；
- 路径不同可用参数覆盖：`-QtRoot`、`-SdkRoot`、`-NdkVersion`、`-JavaHome`、`-Signing`；
- 脚本把 Gradle 缓存和调试密钥放到 `E:\Android\.gradle`、`E:\Android\.android`，不占用 C 盘。

用 Qt Creator 或自己调 CMake 也可以：先执行 `. E:\Keys\endless-disaster-signing.ps1` 设置 `QT_ANDROID_KEYSTORE_*` 环境变量，`CMakeLists.txt` 检测到后会自动开启签名（配置输出里会显示 `Android APK will be signed with ...`）。

## 5. 发布新版本流程

1. 修改 `CMakeLists.txt` 里 `project(... VERSION x.y.z)`，**每次发布都要递增**（versionCode 随之增大）；
2. 提交并推送到 main，确认 Actions 的 Packages 工作流通过；
3. 打标签发布：`git tag -a vx.y.z -m "..."`，`git push origin vx.y.z`；
4. Release 页面生成后，下载 APK 在手机上点击安装，应提示“更新”而不是“冲突”。

同一版本号重新编译（versionCode 相同）也可以覆盖安装，但正式发布请务必改版本号。

## 6. 历史版本迁移

- **v0.3.0 及更早的安卓包**使用一次性临时签名（versionCode 为 3），无法被新版覆盖：用户需要**卸载一次**再安装 v0.3.1 及以后的版本，之后就能一直覆盖升级；
- 从 v0.3.1 起所有安卓包都使用第 2 节的正式密钥。

## 7. 常见问题

**如何查看某个 APK 的签名？**

```powershell
E:\Android\android-sdk\build-tools\35.0.0\apksigner.bat verify --print-certs xxx.apk
```

**如何查看手机上已装版本的 versionCode？**

```powershell
adb shell dumpsys package io.github.shadowye.endlessdisaster | findstr versionCode
```

**手机提示“与现有软件包冲突”？** 说明签名不一致：确认两个 APK 的指纹，旧包若是 v0.3.0 或调试签名包，只能卸载后重装。

## 8. 交接清单

- [ ] `E:\Keys` 目录（密钥库 + 密码）已通过安全渠道移交，并有备份；
- [ ] GitHub 仓库 Secrets 中三项已配置，最近一次 Actions 构建打印的指纹与第 2 节一致；
- [ ] 接手人知道包名不能改、每次发布需递增版本号。
