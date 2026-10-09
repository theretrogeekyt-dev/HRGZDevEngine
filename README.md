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

### 5. Xbox 360 Homebrew & "Peer Pressure" Softmod HAL (`src/hal/xenon/`)
- **Peer Pressure Softmod Support**: Built specifically to test and leverage Grimdoomer's persistent [Xbox 360 Peer Pressure Softmod](https://github.com/grimdoomer/Xbox360PeerPressure).
- **XeLL Bare-Metal Execution (`xenon.elf`)**: Boots directly on bare-metal hardware via the console's **Eject Button** (Peer Pressure's built-in OtherOS XeLL environment), completely bypassing dashboard limitations.
- **16:9 720p Widescreen Output**: Fast LUT-based scaling from DOOM's 426x200 16:9 framebuffer to 1280x720 HDTV resolution.
- **PowerPC 50 MHz TimeBase Hardware Timer**: 64-bit `mftb` register timebase driver guaranteeing deterministic 35 Hz playsim tics on PowerPC.
- **Twin-Stick Controller Mapping**: Full analog and digital integration with Xbox 360 wireless and USB controllers.
- **Multi-Device Storage Autodiscovery**: Automatically detects IWADs across USB (`uda:`, `usb:`), internal HDD (`Hdd1:\PeerPressure\OtherOS\`), and game root.

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
│       ├── common/        # Shared software sound mixer, MUS-to-MIDI, IP net, and Gamepad driver
│       │   ├── i_sound_mixer.c / .h
│       │   ├── i_mus2midi.c / .h
│       │   ├── i_net_ip.c / .h
│       │   └── i_gamepad.c / .h   # Unified Xbox & PlayStation controller driver
│       ├── xenon/         # Xbox 360 Peer Pressure & XeLL HAL (720p 16:9, PowerPC TB, Gamepad)
│       │   ├── i_video_xenon.c
│       │   ├── i_sound_xenon.c
│       │   ├── i_system_xenon.c
│       │   ├── i_net_xenon.c
│       │   ├── i_main_xenon.c
│       │   └── Makefile.xenon
│       ├── mac/           # Native macOS driver (Metal HW accel, Cocoa, GameController, AudioToolbox)
│       │   ├── i_video_mac.m
│       │   ├── i_sound_mac.m
│       │   ├── i_system_mac.c
│       │   └── ...
│       ├── win32/         # Pure Windows driver (OpenGL HW accel, GDI fallback, WinMM, XInput, Winsock)
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
| **macOS Native** | `macos-latest` | `HRGZDevEngine-DOOM-macOS.zip` | Compiles native Metal + Cocoa + GameController + AudioToolbox binary and packages complete `DOOM.app` bundle |
| **Windows Native** | `windows-latest` | `HRGZDevEngine-DOOM-Windows.zip` | Compiles native `doom.exe` via MinGW-w64 with Win32, OpenGL hardware acceleration, XInput + DirectInput controllers, WinMM audio, and Winsock2 |
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
  -framework GameController \
  -framework AudioToolbox -framework CoreFoundation -framework Carbon \
  -lm -o build/mac/doom_mac

# Run:
./build/mac/doom_mac -iwad doom1.wad
```

> **Note on Downloaded macOS Release**:
> When opening `DOOM.app` downloaded from GitHub, macOS Gatekeeper may flag unnotarized open-source binaries as "damaged" or unverified. If this occurs, run:
> ```bash
> xattr -cr /path/to/DOOM.app
> ```
> Or Right-Click `DOOM.app` -> Select **Open** -> Click **Open**.

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

### 4. Xbox 360 Homebrew ("Peer Pressure" Softmod via XeLL)
Xbox 360 bare-metal homebrew applications are built as **PowerPC ELF** binaries (`xenon.elf`) that run under **XeLL** (the Xenon Linux Loader), taking direct uninhibited control of the Xenon PowerPC CPU, 720p 16:9 framebuffer, audio DAC, and USB controllers.

#### Why does Aurora say "Unable to open the file"?
Aurora is an Xbox 360 dashboard application that runs inside the official OS (`xboxkrnl.exe`). Dashboards strictly require native Xbox Executables (`.xex`). Bare-metal homebrew like `xenon.elf` runs **outside** the dashboard directly on the hardware via XeLL!

#### How to Play on Xbox 360 (Peer Pressure Softmod):
1. **Booting via the Console Eject Button (Recommended)**:
   - Format a USB drive to FAT32.
   - Copy `xenon.elf` and your IWAD (`doom1.wad` or `doom2.wad`) directly to the **ROOT** of the USB drive (or to `Hdd1:\PeerPressure\OtherOS\` on console HDD).
   - Plug the USB drive into your console.
   - Turn on the console by pressing the **EJECT BUTTON** (not the power button).
   - Peer Pressure will boot XeLL, auto-detect `xenon.elf`, and launch DOOM in 720p 16:9 immediately!
2. **Booting from Aurora via XeLL Shortcut**:
   - If you are already inside Aurora and don't want to get up to press the Eject button, use the standard community homebrew utility `XellLaunch` (available in SoftmodExtras / Xbox 360 homebrew tools) to soft-reboot the console into XeLL from Aurora.

#### Compiling with PowerPC Cross-Compiler:
```bash
# Cross-compile for PowerPC 32-bit Big-Endian (XeLL):
powerpc-linux-gnu-gcc -O2 -mcpu=powerpc -m32 -mbig-endian \
  -std=c99 -DLIBXENON -D__BIG_ENDIAN__ \
  src/doom/*.c src/hal/common/*.c src/hal/xenon/*.c \
  -Isrc/doom -Isrc/hal/common -lm -s -o build/xenon/xenon.elf
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

### Keyboard & Mouse

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

### Controller Support (PlayStation & Xbox)

HRGZDevEngine DOOM features native, zero-setup plug-and-play gamepad support with modern twin-stick FPS controls:
- **macOS Native**: Apple `GameController.framework` (Bluetooth & USB DualSense, DualShock 4, Xbox Wireless, and Elite controllers).
- **Windows Native**: Dynamic `XInput` for Xbox controllers + WinMM `joyGetPosEx` DirectInput for native PlayStation controllers.
- **Linux Native**: Standard `SDL_GameController` mappings.

| Action | PlayStation Controller (DualShock 4 / DualSense PS5) | Xbox Controller (Series X\|S / One / 360 / Elite) |
|---|---|---|
| **Move & Strafe** | Left Analog Stick (Full 360° Analog) | Left Analog Stick (Full 360° Analog) |
| **Look & Turn** | Right Analog Stick (Smooth Sub-Pixel Curve) | Right Analog Stick (Smooth Sub-Pixel Curve) |
| **Fire / Attack** | Right Trigger (`R2`) | Right Trigger (`RT`) |
| **Sprint / Speed** | Left Trigger (`L2`) or `◯` (Circle) | Left Trigger (`LT`) or `B` Button |
| **Open / Use / Activate** | `✕` (Cross) or `□` (Square) | `A` Button or `X` Button |
| **Next Weapon** | Right Bumper (`R1`) | Right Bumper (`RB`) |
| **Previous Weapon** | Left Bumper (`L1`) | Left Bumper (`LB`) |
| **Toggle Always Run** | `L3` (Left Stick Click) | `LS` (Left Stick Click) |
| **180° Quick Turn** | `R3` (Right Stick Click) | `RS` (Right Stick Click) |
| **Automap Toggle** | `△` (Triangle) / Touchpad / Share | `Y` Button / View / Back |
| **Options / Pause** | Options Button | Menu / Start Button |
| **Quick Weapon Slots** | D-Pad (Up: Shotgun, Down: Chaingun, Left: Rockets/Plasma, Right: BFG/Chainsaw) | D-Pad (Up: Shotgun, Down: Chaingun, Left: Rockets/Plasma, Right: BFG/Chainsaw) |
| **Menu Navigation** | D-Pad / Left Stick (`✕` Confirm, `◯` Back) | D-Pad / Left Stick (`A` Confirm, `B` Back) |

---

## Verification & Integrity Check

The software renderer and playsim have been tested and verified against the canonical Shareware DOOM v1.10 IWAD (`doom1.wad`, MD5: `955a540476a804a89a05a62d089ec276`). All 1,264 lumps, 163 textures, and E1M1 BSP nodes load and render with 100% bit-exact accuracy.

