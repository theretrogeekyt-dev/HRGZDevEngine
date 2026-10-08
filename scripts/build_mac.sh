#!/bin/bash
#==============================================================================
# HRGZDevEngine DOOM - Native macOS Build Script
#
# Builds native Cocoa/CoreGraphics/AudioToolbox DOOM executable and DOOM.app.
# Zero external library dependencies (no SDL2 or Homebrew required)!
#==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

BUILD_DIR="build"
TARGET="${BUILD_DIR}/doom_mac"
APP_DIR="${BUILD_DIR}/DOOM.app"

echo "======================================================="
echo "Building HRGZDevEngine DOOM for macOS (Apple Silicon / Intel)"
echo "======================================================="

mkdir -p "${BUILD_DIR}"

clang -O2 -Wall -Wno-parentheses -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-unused-variable -std=c99 \
    src/doom/*.c \
    src/hal/common/*.c \
    src/hal/mac/*.c \
    src/hal/mac/*.m \
    -Isrc/doom \
    -Isrc/hal/common \
    -framework Cocoa \
    -framework AudioToolbox \
    -framework CoreFoundation \
    -framework Carbon \
    -lm \
    -o "${TARGET}"

echo "Binary compiled: ${TARGET}"

# Create standalone macOS Application Bundle (DOOM.app)
echo "Packaging ${APP_DIR}..."
mkdir -p "${APP_DIR}/Contents/MacOS"
mkdir -p "${APP_DIR}/Contents/Resources"

cp "${TARGET}" "${APP_DIR}/Contents/MacOS/DOOM"
chmod +x "${APP_DIR}/Contents/MacOS/DOOM"

cat > "${APP_DIR}/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>DOOM</string>
    <key>CFBundleIdentifier</key>
    <string>com.hrgzdevengine.doom</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>DOOM</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.10</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

if [ -f "doom1.wad" ]; then
    cp "doom1.wad" "${APP_DIR}/Contents/Resources/doom1.wad"
    echo "Bundled doom1.wad into DOOM.app/Contents/Resources/"
fi

echo "======================================================="
echo "Build Succeeded!"
echo "Run CLI:       ./build/doom_mac -iwad doom1.wad"
echo "Launch App:    open build/DOOM.app"
echo "======================================================="

