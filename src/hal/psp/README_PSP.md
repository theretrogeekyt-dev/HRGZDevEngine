# HRGZDevEngine DOOM for PlayStation Portable (PSP)

A high-performance native port of HRGZDevEngine DOOM for the Sony PlayStation Portable (PSP), built using the official [PSPDEV SDK](https://pspdev.github.io).

---

## Features

- **Native Widescreen Display (480x272)**: Direct double-buffered hardware rendering to uncached PSP eDRAM VRAM at 60 FPS without tearing.
- **Aspect Scaling Modes**:
  - `480x272 (Full Widescreen)`: Edge-to-edge 16:9 widescreen scaling utilizing the full 4.3" display.
  - `426x200 (Pixel-Perfect)`: Centered 1:1 pixel rendering with black border framing.
- **Authentic PSP Controls**:
  - **Analog Nub**: Smooth analog motion and turning with calibrated deadzones.
  - **D-Pad**: Menu navigation & weapon cycling.
  - **$\times$ (Cross)**: Primary attack / Confirm menu.
  - **$\square$ (Square)**: Use / Open doors / Activate switches.
  - **$\bigcirc$ (Circle)**: Sprint / Always Run toggle.
  - **$\triangle$ (Triangle)**: Automap toggle / Back in menus.
  - **L-Trigger**: Strafe Left / Previous weapon.
  - **R-Trigger**: Primary Fire (shoulder trigger).
  - **START**: Pause / In-game options menu.
  - **SELECT**: Automap display.
- **Hardware Multichannel Audio**:
  - Dedicated asynchronous kernel audio thread streaming 16-bit 44.1 kHz stereo PCM via `libpspaudio`.
  - Crisp DOOM sound effects and MUS2MIDI synthesis with zero stuttering.
- **Performance Maximized**:
  - Automatically clocks Allegrex CPU & BUS to official maximum performance mode (333 MHz / 166 MHz) via `scePowerSetClockFrequency`.
- **Universal Compatibility**:
  - Compatible with all PSP revisions (PSP-1000 Fat, PSP-2000 Slim, PSP-3000 Brite, PSP Go, PSP Street E1000).
  - Fully compatible with PlayStation Vita (via Adrenaline CFW).
  - Fully compatible with the PPSSPP emulator across all desktop and mobile platforms.

---

## Installation & Setup

1. Copy the `HRGZDOOM` folder containing `EBOOT.PBP` to your PSP Memory Stick under:
   ```
   ms0:/PSP/GAME/HRGZDOOM/
   ```
   *(On PSP Go Internal Storage: `ef0:/PSP/GAME/HRGZDOOM/`)*
2. Place your DOOM WAD file in the same directory:
   - `doom1.wad` (Shareware DOOM)
   - `doom.wad` (Registered DOOM / The Ultimate DOOM)
   - `doom2.wad` (DOOM II: Hell on Earth)
   - `tnt.wad` / `plutonia.wad` (Final DOOM)
3. On your PSP, navigate to the **Game** menu $\rightarrow$ **Memory Stick** $\rightarrow$ select **HRGZDevEngine DOOM** to play!

---

## Building from Source

The port is compiled using the official PSPDEV toolchain via Docker:

```bash
# Clone the repository
git clone https://github.com/theretrogeekyt-dev/HRGZDevEngine.git
cd HRGZDevEngine

# Compile using official PSPDEV Docker image
docker run --rm -v "$(pwd):/src" -w /src pspdev/pspdev:latest make -f Makefile.psp
```

This generates `EBOOT.PBP` packaged with the authentic DOOM icon and metadata, ready to deploy.
