===============================================================================
HRGZDevEngine DOOM — Xbox 360 Edition
Optimized for the Xbox 360 "Peer Pressure" Softmod Exploit & XeLL
===============================================================================

This port of DOOM is tailored specifically for the Xbox 360 console running
the persistent "Peer Pressure" softmod (by Grimdoomer) or XeLL bare-metal loader.

Project Repository:
https://github.com/theretrogeekyt-dev/HRGZDevEngine

Peer Pressure Softmod:
https://github.com/grimdoomer/Xbox360PeerPressure


-------------------------------------------------------------------------------
FEATURES
-------------------------------------------------------------------------------
- 16:9 True Widescreen: Native 720p (1280x720) output matching modern HDTVs.
- Dual-Boot Architecture:
  * XeLL Bare-Metal ELF (`xenon.elf`): Boots from Eject button via Peer Pressure
    OtherOS integration.
  * Dashboard Unsigned XEX (`default.xex`): Launches directly from Aurora,
    FreestyleDash, XeXMenu, or standard dashboards patched by Peer Pressure.
- Twin-Stick Gamepad Controls: 1:1 mapping for wireless and USB Xbox 360 controllers.
- PowerPC 50 MHz Hardware TimeBase Timer: Rock-solid 35 Hz playsim tic rate.
- 32 MB Zone Memory: Leverages Xbox 360's 512 MB GDDR3 unified RAM.
- Multi-drive IWAD Autodiscovery: Automatically searches USB (`uda:`, `usb:`),
  internal HDD (`hdd:`, `sda:`, `Hdd1:\PeerPressure\OtherOS\`), and game root.
- Frame Refresh System: Double-buffered tear-free rendering.


-------------------------------------------------------------------------------
HOW TO INSTALL & PLAY ON PEER PRESSURE SOFTMOD
-------------------------------------------------------------------------------

METHOD 1: XeLL Boot (Eject Button)
1. Format a USB drive to FAT32.
2. Copy `xenon.elf` and your IWAD (`doom1.wad` or `doom2.wad`) to the ROOT of the USB drive,
   OR copy them to `Hdd1:\PeerPressure\OtherOS\` on your console's hard drive.
3. Plug the USB drive into your Xbox 360.
4. Turn on the console by pressing the EJECT BUTTON.
5. XeLL will boot and automatically launch `xenon.elf` into 16:9 DOOM!

METHOD 2: Dashboard Launch (Aurora / XeXMenu / Softmod Launcher)
1. Copy the `HRGZDevEngine-DOOM-Xbox360` folder containing `default.xex` and `doom1.wad`
   to your USB drive (`Usb0:\DOOM\`) or console HDD (`Hdd1:\Games\DOOM\`).
2. Launch your preferred dashboard (Aurora, XeXMenu, or FreestyleDash).
3. Browse to the folder and launch `default.xex`.
4. Thanks to Peer Pressure's signature and section hash patches, unsigned homebrew
   executes instantly with no hardware chip required!


-------------------------------------------------------------------------------
CONTROLLER LAYOUT (Xbox 360 Wireless / Wired Controller)
-------------------------------------------------------------------------------
- Left Thumbstick: Move & Strafe (Smooth analog)
- Right Thumbstick: Turn & Look (Analog look)
- Right Trigger (RT): Attack / Fire Weapon
- Left Trigger (LT): Sprint / Speed
- Left Bumper (LB): Previous Weapon
- Right Bumper (RB): Next Weapon
- A Button: Use / Open Doors / Activate Switches / Menu Select
- B Button: Cancel / Sprint / Menu Back
- X Button: Use / Open
- Y Button: Automap
- D-Pad: Move / Menu Navigation
- Start Button: Options Menu / Pause
- Back / View: Toggle Automap
- Left Stick Click (L3): Toggle Always-Run
- Right Stick Click (R3): 180° Quick-Turn


-------------------------------------------------------------------------------
SUPPORTED CONSOLE REVISIONS
-------------------------------------------------------------------------------
All consoles supported by the Peer Pressure exploit:
- Xenon
- Zephyr
- Falcon
- Jasper
- Trinity
(Note: Corona and Winchester revisions have hardware Southbridge fixes preventing
the exploit from running).
===============================================================================
