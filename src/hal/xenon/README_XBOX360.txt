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
WHY DID AURORA SAY "UNABLE TO OPEN THE FILE"?
-------------------------------------------------------------------------------
Aurora is a Dashboard application running inside the Xbox 360 OS (xboxkrnl.exe).
Dashboards strictly require native Xbox Executables (.xex).

LibXenon homebrew applications like DOOM are BARE-METAL executables (`xenon.elf`)
compiled for the Xenon PowerPC processor to take direct, uninhibited control of
the hardware (720p 16:9 framebuffer, audio DAC, USB controllers).

Bare-metal homebrew runs via XeLL (Xenon Linux Loader), NOT directly inside Aurora!


-------------------------------------------------------------------------------
HOW TO RUN ON PEER PRESSURE SOFTMOD
-------------------------------------------------------------------------------

METHOD 1: XeLL Boot via Console Eject Button (Recommended & Direct)
1. Format a USB flash drive to FAT32.
2. Copy `xenon.elf` and your IWAD (`doom1.wad` or `doom2.wad`) directly to the
   ROOT of the USB drive (or to `Hdd1:\PeerPressure\OtherOS\` on console HDD).
3. Plug the USB flash drive into any Xbox 360 USB port.
4. Turn on the console by pressing the EJECT BUTTON (instead of the power button).
5. Peer Pressure will boot into XeLL, detect `xenon.elf` on your USB drive, and
   immediately start DOOM in 720p 16:9 true widescreen!

METHOD 2: Launching XeLL from Aurora / Dashboard (Software Shortcut)
If you are already inside Aurora and don't want to get up to press the Eject button:
1. Place `xenon.elf` and `doom1.wad` on the root of your USB drive.
2. Use the popular homebrew shortcut utility `XellLaunch` (available in the Xbox
   homebrew community / SoftmodExtras).
3. Launch `XellLaunch` from Aurora—it soft-reboots the console straight into XeLL,
   which then loads `xenon.elf`!


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
