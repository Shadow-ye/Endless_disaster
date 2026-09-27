# 在 Windows 本地编译安卓 APK（arm64）。默认加载正式签名，得到的包可以与 GitHub Release 的包互相覆盖安装。
#   .\packaging\android\build-apk.ps1                 # 正式签名（需先运行 new-keystore.ps1 或拿到交接的密钥）
#   .\packaging\android\build-apk.ps1 -DebugSignature # 不用正式密钥，只能全新安装，不能覆盖正式包
# 产物：dist\EndlessDisaster-android-arm64.apk。详见 docs/android-signing.md。
param(
    [string]$QtRoot = "E:\Qt\6.8.3",
    [string]$SdkRoot = "E:\Android\android-sdk",
    [string]$NdkVersion = "26.1.10909125",
    [string]$JavaHome = "E:\Android\openjdk\jdk-17.0.12",
    [string]$Signing = "E:\Keys\endless-disaster-signing.ps1",
    [string]$BuildDir = "build-android",
    [switch]$DebugSignature
)
$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repo

function Need([string]$path, [string]$what) {
    if (-not (Test-Path $path)) { throw "找不到${what}：$path" }
}

$qtAndroid = Join-Path $QtRoot "android_arm64_v8a"
$qtHost = Join-Path $QtRoot "mingw_64"
$ndk = Join-Path $SdkRoot "ndk\$NdkVersion"
Need $qtAndroid "Qt for Android (arm64)"
Need $qtHost "Qt 主机工具 (mingw_64)"
Need $ndk "Android NDK"
Need (Join-Path $JavaHome "bin\java.exe") "JDK 17"

$cmake = @("E:\CMake\bin\cmake.exe", (Get-Command cmake -ErrorAction SilentlyContinue).Source) |
    Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
$ninja = @((Get-Command ninja -ErrorAction SilentlyContinue).Source, "C:\msys64\ucrt64\bin\ninja.exe") |
    Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $cmake) { throw "找不到 cmake" }
if (-not $ninja) { throw "找不到 ninja" }

$env:JAVA_HOME = $JavaHome
$env:ANDROID_SDK_ROOT = $SdkRoot
$env:ANDROID_NDK_ROOT = $ndk
# Gradle 缓存和调试密钥默认落在 C 盘用户目录，统一改到 E 盘
$env:GRADLE_USER_HOME = "E:\Android\.gradle"
$env:ANDROID_USER_HOME = "E:\Android\.android"
# 常驻的 Gradle 守护进程会占住输出管道，脚本编完也不退出
$env:GRADLE_OPTS = "-Dorg.gradle.daemon=false"

foreach ($name in "QT_ANDROID_KEYSTORE_PATH", "QT_ANDROID_KEYSTORE_ALIAS",
                  "QT_ANDROID_KEYSTORE_STORE_PASS", "QT_ANDROID_KEYSTORE_KEY_PASS") {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
$signArg = "-DQT_ANDROID_SIGN_APK=OFF"
if (-not $DebugSignature) {
    Need $Signing "签名环境脚本（先运行 packaging\android\new-keystore.ps1，或用 -Signing 指定交接的脚本）"
    . $Signing
    Need $env:QT_ANDROID_KEYSTORE_PATH "密钥库"
    $signArg = "-DQT_ANDROID_SIGN_APK=ON"
}

& $cmake -S . -B $BuildDir -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_TOOLCHAIN_FILE=$qtAndroid\lib\cmake\Qt6\qt.toolchain.cmake" `
    "-DQT_HOST_PATH=$qtHost" `
    "-DANDROID_SDK_ROOT=$SdkRoot" `
    "-DANDROID_NDK_ROOT=$ndk" `
    -DCMAKE_BUILD_TYPE=Release `
    $signArg
if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

& $cmake --build $BuildDir --target apk
if ($LASTEXITCODE -ne 0) { throw "APK 编译失败" }

$pattern = if ($DebugSignature) { "*.apk" } else { "*-signed.apk" }
$apk = Get-ChildItem (Join-Path $BuildDir "android-build") -Recurse -Filter $pattern |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $apk) { throw "没有找到生成的 APK ($pattern)" }

New-Item -ItemType Directory -Force -Path dist | Out-Null
$out = Join-Path $repo "dist\EndlessDisaster-android-arm64.apk"
Copy-Item $apk.FullName $out -Force

$buildTools = Get-ChildItem (Join-Path $SdkRoot "build-tools") -Directory | Sort-Object Name -Descending | Select-Object -First 1
$cert = & (Join-Path $buildTools.FullName "apksigner.bat") verify --print-certs $out |
    Select-String "SHA-256 digest" | Select-Object -First 1
Write-Host "APK:      $out"
Write-Host "证书指纹: $($cert.Line.Split(':')[-1].Trim())"
