#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# GWBHJ — Build script
#
# Builds the Zygisk companion (.so) for THIS device's ABI, assembles a
# flashable Magisk/KSU/APatch module ZIP.
#
# Termux:  ./build.sh [version] [versionCode]
# CI:      Multi-ABI release builds use GitHub Actions
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

VERSION="${1:-2.0.0}"
VERSION_CODE="${2:-200}"
OUT_DIR="$REPO/output"
BUILD_DIR="$REPO/.build"
STAGE="$BUILD_DIR/module"

c_info=$'\033[1;36m'; c_ok=$'\033[1;32m'; c_err=$'\033[1;31m'; c_off=$'\033[0m'
log(){ printf '%s> %s%s\n' "$c_info" "$*" "$c_off"; }
ok(){  printf '%s> %s%s\n' "$c_ok" "$*" "$c_off"; }
die(){ printf '%s> %s%s\n' "$c_err" "$*" "$c_off" >&2; exit 1; }

# ── detect device ABI ────────────────────────────────────────────────────────
case "$(uname -m)" in
  aarch64)       ABI=arm64-v8a ;;
  armv7l|armv8l) ABI=armeabi-v7a ;;
  x86_64)        ABI=x86_64 ;;
  i686|i386)     ABI=x86 ;;
  *) die "unsupported host arch: $(uname -m)" ;;
esac
log "target ABI: $ABI   version: $VERSION ($VERSION_CODE)"

# ── check prerequisites ──────────────────────────────────────────────────────
if command -v clang++ >/dev/null 2>&1; then
    CXX=clang++
elif command -v g++ >/dev/null 2>&1; then
    CXX=g++
else
    die "no C++ compiler found (install clang++ or g++)"
fi

need_cmd(){ command -v "$1" >/dev/null 2>&1 || die "need $1 (install: $2)"; }
need_cmd zip "zip"
need_cmd cmake "cmake"

# ── fresh staging ────────────────────────────────────────────────────────────
rm -rf "$BUILD_DIR"
mkdir -p "$STAGE/zygisk"

# ── build Zygisk companion (.so) ─────────────────────────────────────────────
BUILD_SRC="$BUILD_DIR/cmake"
mkdir -p "$BUILD_SRC"

log "building zygisk companion ($ABI.so)"
cmake -S "$REPO/src" -B "$BUILD_SRC" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DANDROID_ABI="$ABI" \
    2>&1 || die "cmake configure failed"

cmake --build "$BUILD_SRC" -j$(nproc 2>/dev/null || echo 2) 2>&1 || die "cmake build failed"

if [ -f "$BUILD_SRC/libspoof.so" ]; then
    cp "$BUILD_SRC/libspoof.so" "$STAGE/zygisk/$ABI.so"
else
    die "build output not found: libspoof.so"
fi

if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$STAGE/zygisk/$ABI.so" 2>/dev/null || true
fi
ok "zygisk/$ABI.so  ($(du -h "$STAGE/zygisk/$ABI.so" | cut -f1))"

# ── also build for all supported ABIs if cross-compilation is available ──────
for EXTRA_ABI in arm64-v8a armeabi-v7a x86_64 x86; do
    if [ "$EXTRA_ABI" = "$ABI" ]; then continue; fi
    EXTRA_BUILD="$BUILD_SRC_$EXTRA_ABI"
    mkdir -p "$EXTRA_BUILD"
done

# ── assemble static module files ─────────────────────────────────────────────
log "assembling module tree"
cp -r module/. "$STAGE/"

cat > "$STAGE/module.prop" <<EOF
id=gwbhj_jailbreak
name=过强标黑机
version=$VERSION
versionCode=$VERSION_CODE
author=蜡笔不小心
description=过腾讯黑名单，采用白名单控制默认添加三角洲，其它软件去"whitelist.txt"添加，自带越狱免解过环境，支持冻结精简多开系统软件。v2.0+ 授权验证系统，防共享防传播。
minMagisk=20.4
EOF

# ── permissions ──────────────────────────────────────────────────────────────
chmod 0755 "$STAGE/customize.sh" "$STAGE/service.sh" "$STAGE/uninstall.sh" \
           "$STAGE/action.sh" \
           "$STAGE/META-INF/com/google/android/update-binary" 2>/dev/null || true
chmod 0644 "$STAGE/whitelist.txt" 2>/dev/null || true

# ── package ──────────────────────────────────────────────────────────────────
mkdir -p "$OUT_DIR"
ZIP="$OUT_DIR/GWBHJ-KSU-Jailbreak-v$VERSION-$ABI.zip"
rm -f "$ZIP"
( cd "$STAGE" && zip -r -q -X "$ZIP" . -x '.*' )
ok "packaged -> $ZIP  ($(du -h "$ZIP" | cut -f1))"
log "done"
