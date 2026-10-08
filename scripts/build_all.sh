#!/bin/bash
#==============================================================================
# HRGZDevEngine DOOM - Unified Multi-Platform Build Pipeline
#
# Builds ALL DOOM engine ports in a single command on macOS:
#  1. Native macOS (Cocoa + AudioToolbox + DOOM.app Bundle)
#  2. Modern Windows (Native Win32 GDI + WinMM audio, zero DLLs)
#  3. MS-DOS (32-bit Protected Mode DJGPP Mode 13h + CWSDPMI)
#  4. Automated Headless Test Runner (Playsim & renderer regression)
#==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

BUILD_DIR="${ROOT_DIR}/build"
MAC_DIR="${BUILD_DIR}/mac"
WIN_DIR="${BUILD_DIR}/win"
DOS_DIR="${BUILD_DIR}/dos"
TEST_DIR="${BUILD_DIR}/test"

mkdir -p "${MAC_DIR}" "${WIN_DIR}" "${DOS_DIR}" "${TEST_DIR}"

echo "================================================================================"
echo "    HRGZDevEngine DOOM - Unified Multi-Port Cross-Compilation (macOS Host)"
echo "================================================================================"
echo "Host OS:      $(uname -s) $(uname -m) ($(sw_vers -productVersion 2>/dev/null || uname -r))"
echo "Root Path:    ${ROOT_DIR}"
echo "Output Path:  ${BUILD_DIR}"
echo "================================================================================"

CORE_SRCS=(
    src/doom/am_map.c
    src/doom/d_items.c
    src/doom/d_main.c
    src/doom/d_net.c
    src/doom/doomdef.c
    src/doom/doomstat.c
    src/doom/dstrings.c
    src/doom/f_finale.c
    src/doom/f_wipe.c
    src/doom/g_game.c
    src/doom/hu_lib.c
    src/doom/hu_stuff.c
    src/doom/info.c
    src/doom/m_argv.c
    src/doom/m_bbox.c
    src/doom/m_cheat.c
    src/doom/m_fixed.c
    src/doom/m_menu.c
    src/doom/m_misc.c
    src/doom/m_random.c
    src/doom/m_swap.c
    src/doom/p_ceilng.c
    src/doom/p_doors.c
    src/doom/p_enemy.c
    src/doom/p_floor.c
    src/doom/p_inter.c
    src/doom/p_lights.c
    src/doom/p_map.c
    src/doom/p_maputl.c
    src/doom/p_mobj.c
    src/doom/p_plats.c
    src/doom/p_pspr.c
    src/doom/p_saveg.c
    src/doom/p_setup.c
    src/doom/p_sight.c
    src/doom/p_spec.c
    src/doom/p_switch.c
    src/doom/p_telept.c
    src/doom/p_tick.c
    src/doom/p_user.c
    src/doom/r_bsp.c
    src/doom/r_data.c
    src/doom/r_draw.c
    src/doom/r_main.c
    src/doom/r_plane.c
    src/doom/r_segs.c
    src/doom/r_sky.c
    src/doom/r_things.c
    src/doom/s_sound.c
    src/doom/sounds.c
    src/doom/st_lib.c
    src/doom/st_stuff.c
    src/doom/tables.c
    src/doom/v_video.c
    src/doom/w_wad.c
    src/doom/wi_stuff.c
    src/doom/z_zone.c
)

COMMON_SRCS=(
    src/hal/common/i_sound_mixer.c
    src/hal/common/i_mus2midi.c
    src/hal/common/i_net_ip.c
)

MAC_SRCS=(
    src/hal/mac/i_main_mac.m
    src/hal/mac/i_system_mac.c
    src/hal/mac/i_video_mac.m
    src/hal/mac/i_sound_mac.m
    src/hal/mac/i_net_mac.c
)

WIN_SRCS=(
    src/hal/win32/i_main_win.c
    src/hal/win32/i_system_win.c
    src/hal/win32/i_video_win.c
    src/hal/win32/i_sound_win.c
    src/hal/win32/i_net_win.c
)

DOS_SRCS=(
    src/hal/dos/i_main_dos.c
    src/hal/dos/i_system_dos.c
    src/hal/dos/i_video_dos.c
    src/hal/dos/i_sound_dos.c
    src/hal/dos/i_sb_dos.c
    src/hal/dos/i_opl_dos.c
    src/hal/dos/i_net_dos.c
)

TEST_SRCS=(
    src/hal/test/i_main_test.c
    src/hal/test/i_system_test.c
    src/hal/test/i_video_test.c
    src/hal/test/i_sound_test.c
    src/hal/test/i_net_test.c
)

WARN_FLAGS="-Wall -Wno-parentheses -Wno-unused-const-variable -Wno-unused-but-set-variable -Wno-unused-variable -Wno-misleading-indentation -Wno-format-overflow -Wno-maybe-uninitialized -Wno-dangling-pointer -Wno-unknown-warning-option"

STATUS_MAC="SKIPPED"
STATUS_TEST="SKIPPED"
STATUS_WIN="SKIPPED"
STATUS_DOS="SKIPPED"

#------------------------------------------------------------------------------
# 1. Build macOS Native Port (Cocoa + AudioToolbox + DOOM.app)
#------------------------------------------------------------------------------
echo ""
echo "[1/4] Building Native macOS Port..."
MAC_BIN="${MAC_DIR}/doom_mac"
MAC_APP="${MAC_DIR}/DOOM.app"

clang -O2 ${WARN_FLAGS} -std=c99 \
    "${CORE_SRCS[@]}" \
    "${COMMON_SRCS[@]}" \
    "${MAC_SRCS[@]}" \
    -Isrc/doom \
    -Isrc/hal/common \
    -framework Cocoa \
    -framework AudioToolbox \
    -framework CoreFoundation \
    -framework Carbon \
    -lm \
    -o "${MAC_BIN}"

# Package DOOM.app bundle
mkdir -p "${MAC_APP}/Contents/MacOS"
mkdir -p "${MAC_APP}/Contents/Resources"
cp "${MAC_BIN}" "${MAC_APP}/Contents/MacOS/DOOM"
chmod +x "${MAC_APP}/Contents/MacOS/DOOM"

cat > "${MAC_APP}/Contents/Info.plist" << 'EOF'
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
    cp "doom1.wad" "${MAC_APP}/Contents/Resources/doom1.wad"
fi

# Mirror to top build directory
cp "${MAC_BIN}" "${BUILD_DIR}/doom_mac"
rm -rf "${BUILD_DIR}/DOOM.app"
cp -R "${MAC_APP}" "${BUILD_DIR}/DOOM.app"
STATUS_MAC="SUCCESS"
echo "      -> macOS Binary: ${MAC_BIN} ($(du -h "${MAC_BIN}" | cut -f1))"
echo "      -> macOS Bundle: ${MAC_APP}"

#------------------------------------------------------------------------------
# 2. Build Headless Automated Test Port
#------------------------------------------------------------------------------
echo ""
echo "[2/4] Building Headless Test Runner..."
TEST_BIN="${TEST_DIR}/doom_test"

clang -O2 ${WARN_FLAGS} -std=c99 \
    "${CORE_SRCS[@]}" \
    "${COMMON_SRCS[@]}" \
    "${TEST_SRCS[@]}" \
    -Isrc/doom \
    -Isrc/hal/common \
    -lm \
    -o "${TEST_BIN}"

cp "${TEST_BIN}" "${BUILD_DIR}/doom_test"
STATUS_TEST="SUCCESS"
echo "      -> Test Runner:  ${TEST_BIN} ($(du -h "${TEST_BIN}" | cut -f1))"

#------------------------------------------------------------------------------
# 3. Build Modern Windows Port (Cross-compiled via MinGW-w64)
#------------------------------------------------------------------------------
echo ""
echo "[3/4] Cross-Compiling Modern Windows Port..."
WIN_CC=""
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    WIN_CC="x86_64-w64-mingw32-gcc"
elif [ -f "/opt/homebrew/bin/x86_64-w64-mingw32-gcc" ]; then
    WIN_CC="/opt/homebrew/bin/x86_64-w64-mingw32-gcc"
elif command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    WIN_CC="i686-w64-mingw32-gcc"
fi

if [ -n "${WIN_CC}" ]; then
    WIN_BIN="${WIN_DIR}/doom.exe"
    ${WIN_CC} -O2 ${WARN_FLAGS} -std=c99 \
        "${CORE_SRCS[@]}" \
        "${COMMON_SRCS[@]}" \
        "${WIN_SRCS[@]}" \
        -Isrc/doom \
        -Isrc/hal/common \
        -lgdi32 \
        -lwinmm \
        -lws2_32 \
        -lm \
        -s \
        -o "${WIN_BIN}"

    # Copy to build root as doom_win.exe (prevents case collisions with DOOM.EXE on macOS APFS)
    cp "${WIN_BIN}" "${BUILD_DIR}/doom_win.exe"
    if [ -f "doom1.wad" ]; then
        cp "doom1.wad" "${WIN_DIR}/doom1.wad"
    fi
    STATUS_WIN="SUCCESS (${WIN_CC})"
    echo "      -> Windows Port: ${WIN_BIN} ($(du -h "${WIN_BIN}" | cut -f1))"
    echo "      -> Mirrored as:  ${BUILD_DIR}/doom_win.exe"
else
    echo "      [!] MinGW-w64 compiler not found in PATH."
    echo "          Install via Homebrew: brew install mingw-w64"
    STATUS_WIN="FAILED (Missing mingw-w64)"
fi

#------------------------------------------------------------------------------
# 4. Build MS-DOS Port (Cross-compiled via DJGPP)
#------------------------------------------------------------------------------
echo ""
echo "[4/4] Cross-Compiling MS-DOS Port..."
DOS_CC=""
if [ -f "${ROOT_DIR}/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc" ]; then
    DOS_CC="${ROOT_DIR}/toolchain/djgpp/bin/i586-pc-msdosdjgpp-gcc"
elif command -v i586-pc-msdosdjgpp-gcc >/dev/null 2>&1; then
    DOS_CC="i586-pc-msdosdjgpp-gcc"
fi

if [ -n "${DOS_CC}" ]; then
    DOS_BIN="${DOS_DIR}/DOOM.EXE"
    ${DOS_CC} -O2 ${WARN_FLAGS} -std=gnu99 \
        "${CORE_SRCS[@]}" \
        "${COMMON_SRCS[@]}" \
        "${DOS_SRCS[@]}" \
        -Isrc/doom \
        -Isrc/hal/common \
        -lm \
        -s \
        -o "${DOS_BIN}"

    # Bundle CWSDPMI.EXE if available
    if [ -f "${ROOT_DIR}/toolchain/CWSDPMI.EXE" ]; then
        cp "${ROOT_DIR}/toolchain/CWSDPMI.EXE" "${DOS_DIR}/CWSDPMI.EXE"
    fi
    if [ -f "doom1.wad" ]; then
        cp "doom1.wad" "${DOS_DIR}/DOOM1.WAD"
    fi

    # Copy to build root as doom_dos.exe (prevents case collisions with doom.exe on macOS APFS)
    cp "${DOS_BIN}" "${BUILD_DIR}/doom_dos.exe"
    STATUS_DOS="SUCCESS (${DOS_CC})"
    echo "      -> MS-DOS Port:  ${DOS_BIN} ($(du -h "${DOS_BIN}" | cut -f1))"
    echo "      -> Mirrored as:  ${BUILD_DIR}/doom_dos.exe"
else
    echo "      [!] DJGPP cross-compiler not found in toolchain/djgpp or PATH."
    STATUS_DOS="FAILED (Missing DJGPP)"
fi

#------------------------------------------------------------------------------
# Summary Table
#------------------------------------------------------------------------------
echo ""
echo "================================================================================"
echo "                           PORT BUILD SUMMARY"
echo "================================================================================"
printf " %-20s | %-12s | %-42s\n" "Platform Target" "Status" "Binary Location"
echo "--------------------------------------------------------------------------------"
printf " %-20s | %-12s | %-42s\n" "macOS Native" "${STATUS_MAC%% *}" "build/mac/doom_mac, build/DOOM.app"
printf " %-20s | %-12s | %-42s\n" "Headless Test" "${STATUS_TEST%% *}" "build/test/doom_test"
printf " %-20s | %-12s | %-42s\n" "Modern Windows" "${STATUS_WIN%% *}" "build/win/doom.exe, build/doom_win.exe"
printf " %-20s | %-12s | %-42s\n" "MS-DOS Mode 13h" "${STATUS_DOS%% *}" "build/dos/DOOM.EXE, build/doom_dos.exe"
echo "================================================================================"
echo "All available targets built successfully in one go!"
echo "================================================================================"

