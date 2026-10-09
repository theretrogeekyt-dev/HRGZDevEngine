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
HOW TO RUN ON AURORA & PEER PRESSURE SOFTMOD
-------------------------------------------------------------------------------

METHOD 1: Direct 1-Click Launch from Aurora / XeXMenu (default.xex)
This package includes `default.xex` pre-configured to launch directly from the
Aurora dashboard or XeXMenu:
1. Copy this entire folder (containing `default.xex`, `xenon.elf`, and `doom1.wad`)
   to your console's hard drive or USB drive:
   - `Hdd1:\Games\DOOM\` or `Usb0:\DOOM\`
2. Open Aurora Dashboard or XeXMenu.
3. Open the DOOM folder and click `default.xex` (or select DOOM from your Aurora
   games library).
4. `default.xex` launches smoothly without errors, jumps directly to the Xenon
   loader, and boots DOOM into 720p 16:9 true widescreen!

METHOD 2: XeLL Direct Boot via Console Eject Button
If you prefer booting without going through Aurora:
1. Copy `xenon.elf` and `doom1.wad` directly to the ROOT of a FAT32 USB drive
   (or to `Hdd1:\PeerPressure\OtherOS\` on console HDD).
2. Plug the USB flash drive into any Xbox 360 USB port.
3. Turn on the console by pressing the console EJECT BUTTON.
4. Peer Pressure boots into XeLL, detects `xenon.elf`, and starts DOOM immediately!


-------------------------------------------------------------------------------
FEATURES
-------------------------------------------------------------------------------
- 16:9 True Widescreen: Native 720p (1280x720) output matching modern HDTVs.
- Bare-Metal Xenon Execution: Direct hardware control, zero OS overhead.
- Twin-Stick Gamepad Controls: 1:1 mapping for wireless and USB Xbox 360 controllers.
- PowerPC 50 MHz Hardware TimeBase Timer: Rock-solid 35 Hz playsim tic rate.
- 32 MB Zone Memory: Leverages Xbox 360's 512 MB GDDR3 unified RAM.
- Multi-drive IWAD Autodiscovery: Automatically searches USB (`uda:`, `usb:`),
  internal HDD (`hdd:`, `sda:`, `Hdd1:\PeerPressure\OtherOS\`), and game root.
- Frame Refresh System: Double-buffered tear-free rendering.


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
