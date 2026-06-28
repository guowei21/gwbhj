# GWBHJ Windows build script — 4 ABI + package
$ErrorActionPreference = "Stop"

$NDK = "D:\android-ndk-r27d"
$TC  = "$NDK\toolchains\llvm\prebuilt\windows-x86_64\bin"
$SRC = "src"
$INC = "$SRC\include"
$VER = "1.2.1"
$VCD = "121"
$STAGE = ".build\module"
$OUT = "output"

# ABIs: name -> compiler prefix
$abis = [ordered]@{
    "arm64-v8a"    = "aarch64-linux-android21-clang++.cmd"
    "armeabi-v7a"  = "armv7a-linux-androideabi21-clang++.cmd"
    "x86_64"       = "x86_64-linux-android21-clang++.cmd"
    "x86"          = "i686-linux-android21-clang++.cmd"
}

$flags = @(
    "-std=c++17","-Os","-flto","-ffunction-sections","-fdata-sections",
    "-fvisibility=hidden","-fvisibility-inlines-hidden","-fno-rtti",
    "-g0","-DNDEBUG","-fPIC","-shared",
    "-I$INC","$SRC\spoof_module.cpp","$SRC\atexit.cpp",
    "-Wl,--gc-sections","-Wl,--exclude-libs,ALL","-Wl,-s",
    "-static-libstdc++","-llog"
)

# ── fresh staging ──────────────────────────────────────────────
Remove-Item -Recurse -Force ".build" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path "$STAGE\zygisk" -Force | Out-Null
New-Item -ItemType Directory -Path "$STAGE\META-INF\com\google\android" -Force | Out-Null
New-Item -ItemType Directory -Path $OUT -Force | Out-Null

# ── build each ABI ─────────────────────────────────────────────
foreach ($abi in $abis.Keys) {
    $cxx = "$TC\$($abis[$abi])"
    $outSo = "$STAGE\zygisk\$abi.so"
    Write-Host "[BUILD] $abi ..." -ForegroundColor Cyan
    & $cxx @flags -o $outSo
    if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] $abi" -ForegroundColor Red; exit 1 }
    # strip
    & "$TC\llvm-strip.exe" --strip-unneeded $outSo
    Get-ChildItem "$STAGE\zygisk\*.tmp*" -ErrorAction SilentlyContinue | Remove-Item -Force
    $sz = [math]::Round((Get-Item $outSo).Length / 1KB, 0)
    Write-Host "[OK]   $abi.so  (${sz} KB)" -ForegroundColor Green
}

# ── copy module static files ───────────────────────────────────
Copy-Item "module\action.sh"      "$STAGE\" -Force
Copy-Item "module\customize.sh"   "$STAGE\" -Force
Copy-Item "module\service.sh"     "$STAGE\" -Force
Copy-Item "module\uninstall.sh"   "$STAGE\" -Force
Copy-Item "module\whitelist.txt"  "$STAGE\" -Force
Copy-Item "module\freeze_list.txt" "$STAGE\" -Force
Copy-Item "module\META-INF\com\google\android\update-binary" "$STAGE\META-INF\com\google\android\" -Force
Copy-Item "module\META-INF\com\google\android\updater-script" "$STAGE\META-INF\com\google\android\" -Force

# ── module.prop (UTF-8 with BOM → PowerShell here-string reads correctly) ──
$prop = @"
id=gwbhj_jailbreak
name=过强标黑机
version=$VER
versionCode=$VCD
author=蜡笔不小心
description=支持ANDROID_ID伪造与序列号控制,白名单,自带越狱免解过环境
minMagisk=20.4
"@
# Write module.prop as UTF-8 WITHOUT BOM (Magisk requirement)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText("$STAGE\module.prop", $prop, $utf8NoBom)

# ── package ZIP (forward slashes) ──────────────────────────────
$zipPath = "$OUT\GWBHJ-KSU-Jailbreak-v$VER.zip"
Remove-Item $zipPath -ErrorAction SilentlyContinue

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
$files = [System.IO.Directory]::GetFiles($STAGE, "*", [System.IO.SearchOption]::AllDirectories)
foreach ($f in $files) {
    $rel = $f.Substring($STAGE.Length).TrimStart('\','/') -replace '\\','/'
    $entry = $zip.CreateEntry($rel, [System.IO.Compression.CompressionLevel]::Optimal)
    $es = $entry.Open()
    $fs = [System.IO.File]::OpenRead($f)
    $fs.CopyTo($es)
    $fs.Dispose()
    $es.Dispose()
}
$zip.Dispose()

$zipKB = [math]::Round((Get-Item $zipPath).Length / 1KB, 0)
Write-Host ""
Write-Host "[DONE] $zipPath  (${zipKB} KB)" -ForegroundColor Green
