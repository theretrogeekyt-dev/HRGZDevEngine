# HRGZDevEngine DOOM Source Port

A high-performance, cross-platform DOOM source port engineered for **HRGZDevEngine**, based on the canonical id Software 1993/1997 source code ([id-software/DOOM](https://github.com/id-software/DOOM)).

Designed from the ground up for **Modern Windows** (Win32 / Win64 with **zero external dependencies**), **macOS** (Apple Metal hardware acceleration & Cocoa), and modern POSIX systems via SDL2 and an automated headless verification test harness.

---

## Highlights & Engineering Overview

### 1. Modern 64-Bit & Cross-Platform Engine Core (`src/doom/`)
The original 1997 Linux release was riddled with 32-bit pointer assumptions, unaligned memory accesses, compiler-specific behaviors, and deprecated Unix headers. All have been systematically rectified:
- **Zero Pointer Truncation**: Standardized on `<stdint.h>` (`intptr_t`, `uintptr_t`, `int32_t`, `int16_t`, `uint8_t`). Pointers are never cast to 32-bit integers.
- **Fixed 64-Bit Pointer Array Allocations**: Fixed legacy bugs in `r_data.c` and `p_setup.c` where pointer tables (`textures`, `texturecolumnlump`, `texturecolumnofs`, `texturecomposite`, `linebuffer`) were allocated assuming 4-byte pointers (`* 4`), which caused memory corruption on 64-bit architectures.
- **Binary Struct Packing**: Explicit `#pragma pack(push, 1)` and `pop` applied across all binary WAD structures (`doomdata.h`, `w_wad.h`, `r_data.c`), guaranteeing byte-for-byte binary compatibility with vanilla WAD lumps regardless of compiler struct alignment defaults.
- **Win32 Enum Conflict Safeguards**: Resolved conflicts between DOOM's boolean type (`boolean`) and Windows SDK `rpcndr.h`.
- **Clean Configuration Subsystem**: Overhauled `m_misc.c` with explicit typed entries (`isstring`), eliminating unsafe pointer-to-integer casts when parsing `default.cfg`.
- **Accurate Fixed-Point Math**: 64-bit integer accelerated `FixedDiv` and safely parenthesized endian-swapping macros in `m_swap.h`.
- **16:9 Widescreen Renderer**: 426x200 true widescreen software rendering with sub-pixel accurate horizontal FOV expansion and double-buffered frame refreshing.

### 2. Native macOS Driver (`src/hal/mac/`)
- **Apple Metal Hardware Acceleration**: Zero-latency streaming texture presentation with triple-buffering via Metal and QuartzCore.
- **Native Dual Audio**: 16-bit 11025 Hz software multichannel sound mixer and General MIDI playback streamed via macOS `AudioToolbox`.
- **Packaged App Bundle**: Self-contained `DOOM.app` application bundle with embedded IWAD and high-resolution Retina display scaling.

### 3. Native Modern Windows Driver (`src/hal/win32/`)
- **Zero External DLL Dependencies**: Runs out-of-the-box on Windows 95 through Windows 11 without requiring SDL, DirectX, OpenAL, or any runtime redistributable.
- **OpenGL Hardware Acceleration**: High-performance WGL streaming texture upload with V-Sync and graceful GDI `StretchDIBits` fallback.
- **Smooth Mouse Capture**: Windowed and fullscreen cursor confinement with relative motion turning.
- **Native Dual Audio Subsystem**:
  - **Digital Sound Effects**: 16-bit 11025 Hz software multichannel mixer streamed via WinMM `waveOut`.
  - **General MIDI Music**: Real-time DOOM MUS-to-MIDI parser and sequencer driving the built-in Microsoft GS Wavetable Synth (`midiOut`).

### 4. Automated Headless Test Harness (`src/hal/test/`)
- Deterministic headless playsim runner for automated CI/CD and regression testing.
- Renders genuine DOOM BSP scenes into uncompressed RGB PPM frames (`test_frame35.ppm`, `test_frame100.ppm`).

---

## Directory Structure

```
HRGZDevEngine/
├── .github/
│   └── workflows/
│       └── build.yml      # GitHub Actions CI/CD multi-platform build & release pipeline
├── src/
│   ├── doom/              # Core DOOM playsim, software renderer, and game logic
│   │   ├── doomdef.h      # Engine constants, types, and global structures
│   │   ├── r_*.c / r_*.h  # 3D BSP software renderer (widescreen, sub-pixel accurate)
│   │   ├── p_*.c / p_*.h  # Playsim, enemy AI, physics, line specials, sectors
│   │   ├── w_wad.c        # WAD filesystem and lump cache management
│   │   └── ...
│   └── hal/               # Hardware Abstraction Layer (HAL)
│       ├── common/        # Shared software sound mixer, MUS-to-MIDI, and IP networking
│       │   ├── i_sound_mixer.c / .h
│       │   ├── i_mus2midi.c / .h
│       │   └── i_net_ip.c / .h
│       ├── mac/           # Native macOS driver (Metal HW accel, Cocoa, AudioToolbox MIDI)
│       │   ├── i_video_mac.m
│       │   ├── i_sound_mac.m
│       │   ├── i_system_mac.c
│       │   └── ...
│       ├── win32/         # Pure Windows driver (OpenGL HW accel, GDI fallback, WinMM, Winsock)
│       │   ├── i_video_win.c
│       │   ├── i_system_win.c
│       │   ├── i_sound_win.c
│       │   └── ...
│       ├── sdl/           # Cross-platform SDL2 driver (macOS, Linux, Windows)
│       └── test/          # Headless automated verification test harness
└── doom1.wad              # DOOM Shareware IWAD (v1.10) for testing
```

---

## Automated Multi-Platform CI/CD (GitHub Actions)

HRGZDevEngine DOOM utilizes a unified **GitHub Actions CI/CD Pipeline** ([`.github/workflows/build.yml`](.github/workflows/build.yml)) that automates building, testing, packaging, and releasing across all supported operating systems simultaneously.

### Release Versioning & Changelog
- **Unique Versioning**: Every continuous release and tag automatically receives a unique version identifier prefixed with **`0.0.1`** (e.g. `0.0.1.4`, `0.0.1.5`).
- **Automated Changelogs**: Every published release includes a comprehensive markdown changelog tracking recent commits, platform packages, and engine features.

### Automated Pipeline Jobs

| Job | Environment | Output Artifact | Notes |
|---|---|---|---|
| **macOS Native** | `macos-latest` | `HRGZDevEngine-DOOM-macOS.zip` | Compiles native Metal + Cocoa + AudioToolbox binary and packages complete `DOOM.app` bundle |
| **Windows Native** | `windows-latest` | `HRGZDevEngine-DOOM-Windows.zip` | Compiles native `doom.exe` via MinGW-w64 with Win32, OpenGL hardware acceleration, WinMM audio, and Winsock2 |
| **Linux & Test Suite** | `ubuntu-latest` | `HRGZDevEngine-DOOM-Linux-SDL2.zip`<br>`Verification-Screenshots.zip` | Runs headless E1M1 playsim (150 frames), player movement (120 frames), and menu tests; compiles Linux `doom_sdl` binary |
| **Release Publisher** | `ubuntu-latest` | GitHub Release Assets | Automatically attaches all platform zip archives and formatted changelog |

---

## Local Direct Compilation (No Makefiles Required)

To compile locally without relying on legacy build systems or Makefiles, invoke your compiler directly:

### 1. macOS (Native Metal & Cocoa — Zero Dependencies)
```bash
mkdir -p build/mac
clang -O2 -std=c99 \
  src/doom/*.c src/hal/common/*.c src/hal/mac/*.c src/hal/mac/*.m \
  -Isrc/doom -Isrc/hal/common \
  -framework Cocoa -framework Metal -framework QuartzCore \
  -framework AudioToolbox -framework CoreFoundation -framework Carbon \
  -lm -o build/mac/doom_mac

# Run:
./build/mac/doom_mac -iwad doom1.wad
```

### 2. Modern Windows (Native Win32 & OpenGL)
#### MinGW-w64:
```bash
mkdir -p build/win
gcc -O2 -std=c99 \
  src/doom/*.c src/hal/common/*.c src/hal/win32/*.c \
  -Isrc/doom -Isrc/hal/common \
  -lgdi32 -lwinmm -lws2_32 -lopengl32 -lm -s \
  -o build/win/doom.exe

# Run:
build\win\doom.exe -iwad doom1.wad
```

#### Microsoft Visual C++ (MSVC):
```cmd
mkdir build\win
cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Isrc\doom /Isrc\hal\common src\doom\*.c src\hal\common\*.c src\hal\win32\*.c /link gdi32.lib winmm.lib ws2_32.lib opengl32.lib /out:build\win\doom.exe
```

### 3. Linux (SDL2)
```bash
mkdir -p build/linux
gcc -O2 -std=c99 \
  src/doom/*.c src/hal/common/*.c src/hal/sdl/*.c \
  -Isrc/doom -Isrc/hal/common \
  -lSDL2 -lm -s -o build/linux/doom_sdl

# Run:
./build/linux/doom_sdl -iwad doom1.wad
```

### 5. Automated Headless Test Suite (Any OS)
```bash
mkdir -p build/test
gcc -O2 -std=c99 \
  src/doom/*.c src/hal/common/*.c src/hal/test/*.c \
  -Isrc/doom -Isrc/hal/common \
  -lm -o build/test/doom_test

# Execute 150-frame headless playsim verification:
./build/test/doom_test -iwad doom1.wad -warp 1 1 -testframes 150
```

---

## Command-Line Parameters

| Parameter | Description |
|---|---|
| `-iwad <path>` | Specify custom IWAD file (`doom1.wad`, `doom.wad`, `doom2.wad`, etc.) |
| `-scale <1..6>` | Set window scaling multiplier on Windows (default: 3 = 960x600) |
| `-fullscreen` | Launch directly in fullscreen mode |
| `-warp <e> <m>` | Warp straight into episode `<e>` map `<m>` (e.g. `-warp 1 1`) |
| `-skill <1..5>` | Select gameplay difficulty (1: I'm Too Young to Die .. 5: Nightmare) |
| `-nomouse` | Disable mouse control and cursor locking |
| `-nosound` | Disable digital sound effects |
| `-nomusic` | Disable background music playback |
| `-testframes <N>` | Run `<N>` frames headlessly in test mode and quit |

---

## Controls

| Action | Primary Key | Secondary / Mouse |
|---|---|---|
| **Move Forward** | `W` / `Up Arrow` | Mouse Up |
| **Move Backward** | `S` / `Down Arrow` | Mouse Down |
| **Strafe Left** | `A` | `,` |
| **Strafe Right** | `D` | `.` |
| **Turn Left / Right** | `Left Arrow` / `Right Arrow` | Mouse X Axis |
| **Fire Weapon** | `Left Ctrl` | Left Mouse Button |
| **Open / Use / Activate** | `Space` | `E` |
| **Speed / Sprint** | `Left Shift` | `Right Shift` |
| **Weapons 1 - 7** | `1` .. `7` | Number row |
| **Automap** | `Tab` | - |
| **Pause** | `Pause` | - |
| **Main Menu** | `Escape` | - |

---

## Verification & Integrity Check

The software renderer and playsim have been tested and verified against the canonical Shareware DOOM v1.10 IWAD (`doom1.wad`, MD5: `955a540476a804a89a05a62d089ec276`). All 1,264 lumps, 163 textures, and E1M1 BSP nodes load and render with 100% bit-exact accuracy.

