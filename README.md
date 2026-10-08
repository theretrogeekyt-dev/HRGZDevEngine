# HRGZDevEngine DOOM Source Port

A high-performance, cross-platform DOOM source port engineered for **HRGZDevEngine**, based on the canonical id Software 1993/1997 source code ([id-software/DOOM](https://github.com/id-software/DOOM)).

Designed from the ground up to compile and run authentically on both **MS-DOS** (16/32-bit protected mode via DJGPP / Watcom) and **Modern Windows** (Win32 / Win64 with **zero external dependencies**), as well as modern POSIX systems via SDL2 and an automated headless verification test harness.

---

## Highlights & Engineering Overview

### 1. Modern 64-Bit & Cross-Platform Engine Core (`src/doom/`)
The original 1997 Linux release was riddled with 32-bit pointer assumptions, unaligned memory accesses, compiler-specific behaviors, and deprecated Unix headers. All have been systematically rectified:
- **Zero Pointer Truncation**: Standardized on `<stdint.h>` (`intptr_t`, `uintptr_t`, `int32_t`, `int16_t`, `uint8_t`). Pointers are never cast to 32-bit integers.
- **Fixed 64-Bit Pointer Array Allocations**: Fixed legacy DOS bugs in `r_data.c` and `p_setup.c` where pointer tables (`textures`, `texturecolumnlump`, `texturecolumnofs`, `texturecomposite`, `linebuffer`) were allocated assuming 4-byte pointers (`* 4`), which caused memory corruption on 64-bit architectures.
- **Binary Struct Packing**: Explicit `#pragma pack(push, 1)` and `pop` applied across all binary WAD structures (`doomdata.h`, `w_wad.h`, `r_data.c`), guaranteeing byte-for-byte binary compatibility with vanilla WAD lumps regardless of compiler struct alignment defaults.
- **Win32 Enum Conflict Safeguards**: Resolved conflicts between DOOM's boolean type (`boolean`) and Windows SDK `rpcndr.h`.
- **Clean Configuration Subsystem**: Overhauled `m_misc.c` with explicit typed entries (`isstring`), eliminating unsafe pointer-to-integer casts when parsing `default.cfg`.
- **Accurate Fixed-Point Math**: 64-bit integer accelerated `FixedDiv` and safely parenthesized endian-swapping macros in `m_swap.h`.

### 2. Native MS-DOS Driver (`src/hal/dos/`)
- **VGA Mode 13h (320x200 256-color)**: Authentic Mode 13h via BIOS `INT 10h`.
- **Blitting**: High-performance near-pointer linear framebuffer blitting direct to `0xA0000` via `__djgpp_nearptr_enable()`, with automatic DPMI `dosmemput` fallback.
- **VGA DAC Palette Programming**: Direct I/O port programming (`0x3C8` / `0x3C9`) synchronized with vertical retrace (`0x3DA`) for tear-free rendering.
- **Low-Level Keyboard ISR**: Custom IRQ 1 / `INT 9h` interrupt handler providing true multi-key simultaneous rollover (strafe + run + fire + turn) without BIOS keyboard buffer beeps.
- **Hardware Timer & Memory**: Microsecond-resolution timer via DJGPP `uclock()`, and DPMI heap management.

### 3. Native Modern Windows Driver (`src/hal/win32/`)
- **Zero External DLL Dependencies**: Runs out-of-the-box on Windows 95 through Windows 11 without requiring SDL, DirectX, OpenAL, or any runtime redistributable.
- **Aspect-Correct GDI Blitter**: Hardware-accelerated desktop composition via `StretchDIBits` with 4:3 aspect ratio pillarboxing/letterboxing and integer scaling (`-scale 1` to `-scale 6`).
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
├── src/
│   ├── doom/              # Core DOOM playsim, software renderer, and game logic
│   │   ├── doomdef.h      # Engine constants, types, and global structures
│   │   ├── r_*.c / r_*.h  # 3D BSP software renderer (walls, floors, sprites, colormaps)
│   │   ├── p_*.c / p_*.h  # Playsim, enemy AI, physics, line specials, sectors
│   │   ├── w_wad.c        # WAD filesystem and lump cache management
│   │   └── ...
│   └── hal/               # Hardware Abstraction Layer (HAL)
│       ├── common/        # Shared software sound mixer and MUS-to-MIDI converter
│       │   ├── i_sound_mixer.c / .h
│       │   └── i_mus2midi.c / .h
│       ├── dos/           # MS-DOS platform driver (Mode 13h, INT 9h ISR, DJGPP DPMI)
│       │   ├── i_video_dos.c
│       │   ├── i_system_dos.c
│       │   ├── i_sound_dos.c
│       │   └── ...
│       ├── mac/           # Native macOS driver (Cocoa, CoreGraphics, AudioToolbox MIDI)
│       │   ├── i_video_mac.m
│       │   ├── i_sound_mac.m
│       │   ├── i_system_mac.c
│       │   └── ...
│       ├── win32/         # Pure Windows platform driver (GDI StretchDIBits, WinMM wave/midi)
│       │   ├── i_video_win.c
│       │   ├── i_system_win.c
│       │   ├── i_sound_win.c
│       │   └── ...
│       ├── sdl/           # Cross-platform SDL2 driver (macOS, Linux, Windows)
│       └── test/          # Headless automated verification test harness
├── scripts/
│   ├── build_all.sh       # Unified master build script (builds ALL ports on macOS)
│   ├── build_mac.sh       # Native macOS build & app packager
│   ├── build_dos.bat      # Automated DOS / DOSBox DJGPP build script
│   └── build_win32.bat    # Automated Windows build script (MSVC or MinGW)
├── Makefile               # Top-level unified Makefile (all-ports, mac, win, dos, test)
├── Makefile.dos           # DJGPP Makefile for MS-DOS (produces DOOM.EXE)
├── Makefile.win           # MinGW Makefile for Windows (produces doom.exe)
├── CMakeLists.txt         # Modern CMake build configuration
└── doom1.wad              # DOOM Shareware IWAD (v1.10) for testing
```

---

## Unified Multi-Platform Build (All Ports in One Go)

On macOS, you can build **all target platforms simultaneously** in a single command:

```bash
make
# or:
make all-ports
# or directly:
./scripts/build_all.sh
```

This single command builds and packages:
1. 🍏 **Native macOS Port**: `build/mac/doom_mac` and `build/DOOM.app`
2. 🪟 **Modern Windows Port**: `build/win/doom.exe` and `build/doom_win.exe` (PE32+ 64-bit, zero DLLs)
3. 💾 **MS-DOS Port**: `build/dos/DOOM.EXE` and `build/doom_dos.exe` (32-bit Mode 13h DPMI + `CWSDPMI.EXE`)
4. 🧪 **Headless Test Runner**: `build/test/doom_test` (automated playsim & renderer verification)

---

### 1. macOS (Native Cocoa & AudioToolbox - Zero Dependencies!)

Works out of the box on Apple Silicon (M1/M2/M3/M4) and Intel Macs running macOS 10.13 through macOS 15+. **No Homebrew or external libraries (SDL2) needed!**

#### Quick Build:
```bash
make mac
```
*or using the script directly:*
```bash
./scripts/build_mac.sh
```

#### Run on macOS:
```bash
# Launch CLI executable:
./build/doom_mac -iwad doom1.wad -scale 3

# Or launch as a native macOS Application:
open build/DOOM.app
```

---

### 2. Modern Windows (Native Win32 - Zero Dependencies)

#### Option A: Visual Studio / MSVC Developer Command Prompt
Open the **x64 Native Tools Command Prompt for VS** and run:
```cmd
scripts\build_win32.bat
```
Or compile directly:
```cmd
cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Isrc\doom /Isrc\hal\common src\doom\*.c src\hal\common\*.c src\hal\win32\*.c /link gdi32.lib winmm.lib /out:doom.exe
```

#### Option B: MinGW-w64
In PowerShell or CMD with MinGW in PATH:
```cmd
make -f Makefile.win
```

#### Run DOOM on Windows:
```cmd
doom.exe -iwad doom1.wad -scale 3
```

---

### 3. MS-DOS (DJGPP / FreeDOS / DOSBox)

#### Prerequisites:
- DJGPP (GCC 3.x, 4.x, or 5.x)
- CWSDPMI.EXE in PATH or working directory

#### Building under DOS / DOSBox:
```bat
scripts\build_dos.bat
```
Or directly:
```bat
make -f Makefile.dos
```

#### Run DOOM under DOS:
```bat
DOOM.EXE -iwad DOOM1.WAD
```

---

### 4. Headless Verification Test (Any System)

To compile and verify the playsim, renderer, and texture generation without opening a window:
```bash
# Build test runner
make test

# Run 70 tics of gameplay on E1M1 and output verification screenshot
make run-test
```
This produces `test_frame35.ppm` and confirms full playsim and software renderer execution.

---

### 4. Cross-Platform CMake Build (SDL2 or Native)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```
- On Windows: Builds `doom_win32.exe` (native Win32).
- On systems with SDL2: Builds `doom_sdl`.
- On all platforms: Builds `doom_test`.

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

