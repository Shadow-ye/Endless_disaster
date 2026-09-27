# 用官方 Qt（MinGW 13.1 + 自带的精简 LGPL FFmpeg）编译 Windows 版，并打成可直接分发的目录和 zip。
#   .\packaging\windows\build-windows.ps1
# 产物：dist\EndlessDisaster\（解压即玩）与 dist\EndlessDisaster-windows-x64.zip。
# 工具链安装（均在 E 盘）：
#   aqt install-qt windows desktop 6.8.3 win64_mingw -m qtmultimedia -O E:\Qt
#   aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O E:\Qt
param(
    [string]$QtDir = "E:\Qt\6.8.3\mingw_64",
    [string]$MinGWDir = "E:\Qt\Tools\mingw1310_64",
    [string]$BuildDir = "build-windows"
)
$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repo

function Need([string]$path, [string]$what) {
    if (-not (Test-Path $path)) { throw "找不到${what}：$path" }
}

Need (Join-Path $QtDir "bin\windeployqt.exe") "官方 Qt (mingw_64)"
Need (Join-Path $QtDir "bin\avcodec-61.dll") "官方 Qt 自带的 FFmpeg（安装时需带 -m qtmultimedia）"
Need (Join-Path $MinGWDir "bin\g++.exe") "Qt 官方 MinGW 13.1"

$cmake = @("E:\CMake\bin\cmake.exe", (Get-Command cmake -ErrorAction SilentlyContinue).Source) |
    Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
$ninja = @((Get-Command ninja -ErrorAction SilentlyContinue).Source, "C:\msys64\ucrt64\bin\ninja.exe") |
    Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $cmake) { throw "找不到 cmake" }
if (-not $ninja) { throw "找不到 ninja" }

# 不能让 MSYS2 的 bin 留在 PATH 里：它的 libstdc++ / Qt DLL 与官方 Qt 不兼容，windeployqt 可能拷错
$env:PATH = "$MinGWDir\bin;$QtDir\bin;$env:SystemRoot\System32;$env:SystemRoot"

& $cmake -S . -B $BuildDir -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    "-DCMAKE_CXX_COMPILER=$MinGWDir\bin\g++.exe" `
    -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

& $cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

$out = Join-Path $repo "dist\EndlessDisaster"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $out | Out-Null
# 只复制运行需要的文件，排除 CMake / Ninja 的中间产物
robocopy $BuildDir $out /E /NFL /NDL /NJH /NJS /NP `
    /XD CMakeFiles EndlessDisaster_autogen .qt Testing `
    /XF CMakeCache.txt cmake_install.cmake build.ninja .ninja_deps .ninja_log compile_commands.json *.a | Out-Null
if ($LASTEXITCODE -ge 8) { throw "复制到 dist 失败" }
# robocopy 成功时也返回 1~7，不清零的话 CI 会把它当成失败
$global:LASTEXITCODE = 0

$zip = Join-Path $repo "dist\EndlessDisaster-windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $out -DestinationPath $zip

$size = (Get-ChildItem $out -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("目录: {0}  ({1:N1} MB)" -f $out, $size)
Write-Host ("压缩包: {0}  ({1:N1} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))
