param(
    [string]$Abi = "arm64-v8a",
    [string]$OutRoot = "",
    [switch]$SkipBuild,
    [switch]$OpenXR
)
# SlashCo Android APK builder (aapt2 + SHA-256 v2 signing, no gradle required).
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$out = if ($OutRoot) { $OutRoot } else { Join-Path $repoRoot ("build-android-" + $Abi) }
$work = Join-Path $PSScriptRoot "work"
New-Item -ItemType Directory -Force -Path $work | Out-Null

function Resolve-Android([string]$s) { if ([string]::IsNullOrWhiteSpace($s)) { return $null }; return (Resolve-Path $s).Path }
$sdk = Resolve-Android $env:ANDROID_HOME
if (-not $sdk) { $sdk = Resolve-Android $env:ANDROID_SDK_ROOT }
if (-not $sdk) { $sdk = Resolve-Android (Join-Path $env:LOCALAPPDATA "Android\Sdk") }
if (-not $sdk) { throw "Android SDK not found (set ANDROID_HOME or install to %LOCALAPPDATA%\Android\Sdk)" }

$ndk = Get-ChildItem (Join-Path $sdk "ndk") -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
if (-not $ndk) { throw "Android NDK not found under $sdk\ndk (install via Android Studio SDK manager or cmdline-tools)" }
$ndkPath = $ndk.FullName -replace '\\', '/'
Write-Host "sdk: $sdk"
Write-Host "ndk: $ndkPath"

$buildToolsVer = "34.0.0"
$bt = Join-Path $sdk "build-tools\$buildToolsVer"
if (-not (Test-Path $bt)) { $bt = (Get-ChildItem (Join-Path $sdk "build-tools") -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName }
$platformFw = Get-ChildItem (Join-Path $sdk "platforms") -Directory -Filter "android-3*" | Sort-Object Name -Descending | Select-Object -First 1
$androidJar = Join-Path $platformFw.FullName "android.jar"

$jdk = Resolve-Android $env:JAVA_HOME
if (-not $jdk) {
    foreach ($p in @("C:\Program Files\Microsoft\jdk-21.0.8.9-hotspot")) {
        if (Test-Path $p) { $jdk = $p; break }
    }
}
if (-not (Test-Path $jdk)) { throw "JDK not found (set JAVA_HOME)" }
$env:JAVA_HOME = $jdk
$env:PATH = (Join-Path $jdk "bin") + ";" + $env:PATH
$env:ANDROID_NDK = $ndkPath

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $cmake = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\cmake.exe"
    if (-not (Test-Path $cmake)) { throw "cmake not found on PATH" }
}
if (-not $ninja) {
    $linksNinja = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\ninja.exe"
    if (Test-Path $linksNinja) { $ninjaDir = Split-Path $linksNinja -Parent }
}
if (-not $ninjaDir) {
    try { $ninjaInst = (Get-Command ninja.exe -ErrorAction SilentlyContinue); if ($ninjaInst) { $ninjaDir = $ninjaInst.DirectoryName } } catch {}
}
if (-not $ninjaDir) {
    try {
        $found = & $cmake --system-information 2>$null | Select-String -Pattern "CMAKE_MAKE_PROGRAM" -First 1
        if ($found) {
            $guess = ($found.Line -split "=", 2)[1].Trim()
            if ($guess) { $ninjaDir = Split-Path $guess -Parent }
        }
    } catch {}
}
if ($ninjaDir) { $env:PATH = "$ninjaDir;$env:PATH" } else { throw "ninja not found" }
$gen = "Ninja"

if (-not $SkipBuild) {
    $cfg = @(
        "-S", $repoRoot, "-B", $out,
        "-G", $gen,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_SYSTEM_NAME=Android",
        "-DCMAKE_ANDROID_NDK=$ndkPath",
        "-DCMAKE_ANDROID_ARCH_ABI=$Abi",
        "-DCMAKE_ANDROID_API=24",
        "-DCMAKE_ANDROID_STL_TYPE=c++_static",
        "-DANDROID_NDK=$ndkPath",
        "-DSLASHCO_ENABLE_IMGUI=OFF",
        "-DSLASHCO_ENABLE_PACKAGING=OFF",
        "-DSLASHCO_ENABLE_LOCAL_HOST=ON"
    )
    if ($OpenXR) { $cfg += "-DSLASHCO_ENABLE_OPENXR=ON" }
    & $cmake @cfg | Select-Object -Last 6
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & $cmake --build $out --parallel | Select-Object -Last 10
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
}

$soPath = Join-Path $out "libSlashCoClient.so"
if (-not (Test-Path $soPath)) { throw "libSlashCoClient.so not produced (check cmake/build logs)" }
$soSize = (Get-Item $soPath).Length
Write-Host ("libSlashCoClient.so: {0:N0} bytes" -f $soSize)

$manifest = Join-Path $PSScriptRoot "AndroidManifest.xml"
$assetsDir = Join-Path $PSScriptRoot "assets"
$resDir = Join-Path $PSScriptRoot "res"
$mipmap = Join-Path $resDir "mipmap"
if (-not (Test-Path (Join-Path $mipmap "ic_launcher.png"))) {
    & (Join-Path $PSScriptRoot "make_icon.ps1") $mipmap | Out-Null
}
$unsigned = Join-Path $work "SlashCo-unsigned-$Abi.apk"
if (Test-Path $unsigned) { Remove-Item $unsigned -Force }

$resZip = Join-Path $work "res.zip"
& (Join-Path $bt "aapt2.exe") compile --dir $resDir -o $resZip | Out-Null
if ($LASTEXITCODE -ne 0) { throw "aapt2 compile (res) failed" }

& (Join-Path $bt "aapt2.exe") link --manifest $manifest -I $androidJar -A $assetsDir -o $unsigned $resZip | Out-Null
if ($LASTEXITCODE -ne 0) { throw "aapt2 link failed" }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($unsigned, "Update")
try {
    $entry = $zip.CreateEntry("lib/$Abi/libSlashCoClient.so")
    $stream = $entry.Open()
    $bytes = [System.IO.File]::ReadAllBytes($soPath)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Close()
} finally { $zip.Dispose() }
Write-Host "packed jniLibs -> $unsigned"

$aligned = Join-Path $work "SlashCo-aligned-$Abi.apk"
if (Test-Path $aligned) { Remove-Item $aligned -Force }
& (Join-Path $bt "zipalign.exe") -f -p 4 $unsigned $aligned | Out-Null
if ($LASTEXITCODE -ne 0) { throw "zipalign failed" }

$ks = Join-Path $PSScriptRoot "debug.keystore"
if (-not (Test-Path $ks)) {
    & (Join-Path $jdk "bin\keytool.exe") -genkeypair -keystore $ks -alias androiddebugkey -keypass android -storepass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" | Out-Null
}

$signed = Join-Path $work "SlashCo-signed-$Abi.apk"
if (Test-Path $signed) { Remove-Item $signed -Force }
& (Join-Path $bt "apksigner.bat") sign --ks $ks --ks-pass "pass:android" --key-pass "pass:android" --ks-key-alias androiddebugkey --out $signed $aligned | Out-Null
if ($LASTEXITCODE -ne 0) { throw "apksigner signing failed" }

$outApk = Join-Path $repoRoot ("SlashCo-0.3.0-android-" + $Abi + ".apk")
if (Test-Path $outApk) { Remove-Item $outApk -Force }
Copy-Item $signed $outApk -Force
Write-Host ""
Write-Host "APK: $outApk"
& (Join-Path $jdk "bin\jar.exe") -tf $outApk | Select-Object -First 12
