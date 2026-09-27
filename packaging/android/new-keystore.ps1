# 生成《无尽之灾》安卓正式签名密钥（只需执行一次），并输出：
#   <OutDir>\endless-disaster.keystore           密钥库，务必备份，丢失后无法再覆盖升级
#   <OutDir>\endless-disaster-signing.ps1        本地编译时加载的签名环境变量（含密码）
#   <OutDir>\endless-disaster-github-secrets.txt 需要填到 GitHub 仓库 Secrets 的三项内容
# 这些文件都不要提交到仓库。详见 docs/android-signing.md。
param(
    [string]$OutDir = "E:\Keys",
    [string]$Alias = "endless",
    [string]$Keytool = ""
)
$ErrorActionPreference = "Stop"

$keystore = Join-Path $OutDir "endless-disaster.keystore"
$envFile = Join-Path $OutDir "endless-disaster-signing.ps1"
$secretsFile = Join-Path $OutDir "endless-disaster-github-secrets.txt"

if (Test-Path $keystore) {
    throw "密钥库已存在：$keystore 。已发布过的 APK 依赖它，不要重新生成；确需更换请先手动移走旧文件。"
}

if (-not $Keytool) {
    $candidates = @()
    if ($env:JAVA_HOME) { $candidates += Join-Path $env:JAVA_HOME "bin\keytool.exe" }
    $candidates += "E:\Android\openjdk\jdk-17.0.12\bin\keytool.exe"
    $cmd = Get-Command keytool -ErrorAction SilentlyContinue
    if ($cmd) { $candidates += $cmd.Source }
    $Keytool = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
    if (-not $Keytool) { throw "找不到 keytool，请用 -Keytool 指定 JDK 里的 keytool.exe" }
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$bytes = New-Object byte[] 18
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
$password = -join ($bytes | ForEach-Object { $_.ToString("x2") })

# PKCS12 密钥库的 store 密码与 key 密码必须相同
& $Keytool -genkeypair -v -storetype PKCS12 -keystore $keystore -alias $Alias `
    -keyalg RSA -keysize 2048 -validity 10000 `
    -storepass $password -keypass $password `
    -dname "CN=Endless Disaster, O=Shadow-ye, C=CN" | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $keystore)) { throw "keytool 生成密钥失败" }

$utf8Bom = New-Object System.Text.UTF8Encoding $true

$envScript = @"
# 由 packaging/android/new-keystore.ps1 生成；含密码，勿外传、勿提交
`$env:QT_ANDROID_KEYSTORE_PATH = "$keystore"
`$env:QT_ANDROID_KEYSTORE_ALIAS = "$Alias"
`$env:QT_ANDROID_KEYSTORE_STORE_PASS = "$password"
`$env:QT_ANDROID_KEYSTORE_KEY_PASS = "$password"
"@
[System.IO.File]::WriteAllText($envFile, $envScript, $utf8Bom)

$b64 = [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($keystore))
$secrets = @"
GitHub 仓库 -> Settings -> Secrets and variables -> Actions -> New repository secret
依次添加以下三项（Name 与 Secret 分别填入）：

Name:   ANDROID_KEYSTORE_BASE64
Secret: $b64

Name:   ANDROID_KEYSTORE_PASSWORD
Secret: $password

Name:   ANDROID_KEY_ALIAS
Secret: $Alias
"@
[System.IO.File]::WriteAllText($secretsFile, $secrets, $utf8Bom)

$fingerprint = (& $Keytool -list -v -keystore $keystore -storepass $password -alias $Alias |
    Select-String "SHA256:" | Select-Object -First 1).Line.Trim()

Write-Host "密钥库:        $keystore"
Write-Host "本地签名环境:  $envFile"
Write-Host "GitHub Secrets: $secretsFile"
Write-Host "证书指纹:      $fingerprint"
