# HRGZDevEngine Studio
### All-in-One Game Creation, Compilation & Distribution System Powered by DOOM

**HRGZDevEngine Studio** is a visual desktop application and distribution suite that empowers developers, level designers, and modders to package custom DOOM-engine projects, WAD archives, DeHackEd patches, and audio assets into **standalone, commercially distributable retro games** with isolated saves, custom branding, and zero external dependencies.

---

### 📦 Official Studio Downloads (Latest Release)

Pre-built packages for **HRGZDevEngine Studio** are available for all major desktop operating systems:

| Platform | Download Package | Format | Details |
| :--- | :--- | :--- | :--- |
| 🍏 **macOS** | [`HRGZDevEngine-Studio-macOS.dmg`](https://github.com/theretrogeekyt-dev/HRGZDevEngine/releases/latest/download/HRGZDevEngine-Studio-macOS.dmg) | Apple Disk Image (`.dmg`) | Standalone native Cocoa/WebKit desktop application (Apple Silicon & Intel) |
| 🪟 **Windows** | [`HRGZDevEngine-Studio-Windows.zip`](https://github.com/theretrogeekyt-dev/HRGZDevEngine/releases/latest/download/HRGZDevEngine-Studio-Windows.zip) | Executable Bundle (`.exe`) | Dedicated Win32 standalone launcher (`HRGZDevEngine-Studio.exe`) without command prompt windows |
| 🐧 **Linux** | [`hrgzdevengine-studio_1.0.0_all.deb`](https://github.com/theretrogeekyt-dev/HRGZDevEngine/releases/latest/download/hrgzdevengine-studio_1.0.0_all.deb) | Debian Package (`.deb`) | Native package with `/usr/bin/hrgz-studio` launcher and desktop menu entry |

---

## 🚀 Key Studio Features

- **Visual Desktop Studio GUI**: Run `./hrgz-studio` or open the installed app to launch the interactive dark-mode dashboard.
- **Universal CLI (`bin/hrgz`)**: Command-line interface for terminal users, scripting, and CI/CD pipelines (`hrgz init`, `hrgz build`, `hrgz dist`, `hrgz run`).
- **1-Click Native Game Distribution**:
  - **macOS**: Standalone `.app` bundles packed inside an Apple Disk Image (`.dmg`) with `/Applications` drag-and-drop link.
  - **Windows**: Dedicated `.exe` packages with embedded assets and launcher batch scripts for Steam and itch.io.
  - **Linux**: Dedicated Debian packages (`.deb`) installing into `/usr/games/` with `.desktop` menu integration.
- **Integrated WAD Asset Pipeline**: Fast lump inspector validating palettes (`PLAYPAL`), textures, sprites, maps, and music.
- **Embedded Engine Runtime**: Powered by the modern 64-bit HRGZDevEngine core with Apple Metal hardware acceleration, OpenGL streaming, true 16:9 widescreen, and multi-resolution display scaling.


---

## Directory Structure

```
HRGZDevEngine/
├── .github/
│   └── workflows/
│       └── build.yml      # GitHub Actions CI/CD multi-platform build & release pipeline
├── Makefile.psp           # Official PSPDEV build system for generating EBOOT.PBP
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
│       ├── psp/           # Sony PlayStation Portable driver (eDRAM VRAM, libpspaudio, libpspctrl, 333MHz)
│       │   ├── i_main_psp.c
│       │   ├── i_video_psp.c
│       │   ├── i_sound_psp.c
│       │   ├── i_system_psp.c
│       │   ├── ICON0.PNG
│       │   └── README_PSP.md
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
| **PlayStation Portable** | `pspdev/pspdev:latest` | `HRGZDevEngine-DOOM-PSP.zip` | Compiles MIPS allegrex binary and packages official `EBOOT.PBP` with `ICON0.PNG` for PSP, PS Vita, and PPSSPP |
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

### 4. PlayStation Portable (PSP — Official PSPDEV SDK)
```bash
# Build EBOOT.PBP inside official PSPDEV container:
docker run --rm -v "$(pwd):/src" -w /src pspdev/pspdev:latest make -f Makefile.psp

# Deploy to PSP Memory Stick:
# Copy the resulting EBOOT.PBP and your doom1.wad to ms0:/PSP/GAME/HRGZDOOM/
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
| `-res <w>x<h>` | Set custom window resolution (e.g. `-res 1920x1080`, `-res 1280x720`, `-res 3840x2160`) |
| `-width <w> -height <h>` | Set custom window dimensions |
| `-480p` | Launch in 854x480 resolution (480p SD) |
| `-720p` | Launch in 1280x720 resolution (720p HD, default) |
| `-900p` | Launch in 1600x900 resolution (900p HD+) |
| `-1080p` | Launch in 1920x1080 resolution (1080p Full HD) |
| `-1440p` | Launch in 2560x1440 resolution (1440p QHD) |
| `-4k` / `-2160p` | Launch in 3840x2160 resolution (4K UHD) |
| `-scale <1..6>` | Legacy window scaling multiplier (1 to 6) |
| `-fullscreen` / `-fs` | Launch directly in fullscreen mode |
| `-windowed` / `-win` | Launch in windowed mode |
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
| **Toggle Fullscreen** | `Alt+Enter` | `F11` |

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

## HRGZDevEngine Studio & Game Distribution Guide

### 1. Launching the Visual Desktop Studio
To start the visual game studio dashboard:
```bash
./hrgz-studio
# Or using npm
npm run studio
```
The studio dashboard will open automatically in your browser at `http://127.0.0.1:4820` with:
- **Interactive Project Dashboard**: Overview of metadata, game slug, version, and quick publishing links.
- **Visual Asset Pipeline**: Automatic WAD lump parser, verifying palette, textures, sound effects, and level lists.
- **Real-Time Compiler Hub**: Live compiler logs streaming directly to the in-browser terminal console.
- **One-Click Distribution**: Builds `.zip`, `.dmg`, `.deb`, and `.exe` releases with `itch.toml` action files and Steam configs.

### Official Pre-Built Studio Downloads (GitHub Releases)
Pre-built packages for **HRGZDevEngine Studio** are published with every GitHub Release:
- 🍏 **macOS**: [`HRGZDevEngine-Studio-macOS.dmg`](https://github.com/theretrogeekyt-dev/HRGZDevEngine/releases/latest/download/HRGZDevEngine-Studio-macOS.dmg) (Apple Disk Image)
- 🪟 **Windows**: [`HRGZDevEngine-Studio-Windows.zip`](https://github.com/theretrogeekyt-dev/HRGZDevEngine/releases/latest/download/HRGZDevEngine-Studio-Windows.zip) (Standalone `HRGZDevEngine-Studio.exe` bundle)
- 🐧 **Linux**: [`hrgzdevengine-studio_1.0.0_all.deb`](https://github.com/theretrogeekyt-dev/HRGZDevEngine/releases/latest/download/hrgzdevengine-studio_1.0.0_all.deb) (Debian Package)


### 2. Dedicated Native Packaging (.dmg, .deb, .exe)
You can package the **Studio App** or your **Custom Games** into dedicated platform packages:

#### Packaging the Studio App:
```bash
# Package all platforms
npm run package:studio

# Or package individual platforms:
npm run package:mac    # Generates dist/HRGZDevEngine-Studio-macOS.dmg
npm run package:linux  # Generates dist/hrgzdevengine-studio_1.0.0_all.deb
npm run package:win    # Generates dist/HRGZDevEngine-Studio-Windows.exe bundle
```

#### Packaging Your Standalone Game:
```bash
# Scaffold a new standalone game project
./bin/hrgz init "My Great Game"

# Inspect any WAD file (lumps, maps, palettes, sprites)
./bin/hrgz inspect path/to/game.wad

# Package game into macOS Apple Disk Image (.dmg)
./bin/hrgz dist dmg

# Package game into Linux Debian Package (.deb)
./bin/hrgz dist deb

# Package game into Windows Standalone Executable (.exe)
./bin/hrgz dist exe

# Launch the game locally for instant gameplay testing
./bin/hrgz run
```

---

## Verification & Integrity Check

The software renderer and playsim have been tested and verified against the canonical Shareware DOOM v1.10 IWAD (`doom1.wad`, MD5: `955a540476a804a89a05a62d089ec276`). All 1,264 lumps, 163 textures, and E1M1 BSP nodes load and render with 100% bit-exact accuracy.

