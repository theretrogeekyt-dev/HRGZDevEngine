@echo off
rem ===========================================================================
rem HRGZDevEngine DOOM - Modern Windows Build Script (MSVC or MinGW)
rem
rem Builds native Windows 32/64-bit DOOM executable with ZERO external DLLs!
rem Uses native Win32 GDI (StretchDIBits) and WinMM (waveOut / midiOut).
rem ===========================================================================

echo =======================================================
echo Building HRGZDevEngine DOOM for Modern Windows
echo =======================================================

rem Check for MSVC (cl.exe in PATH)
where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Detected MSVC (Visual Studio) compiler.
    echo Compiling native Win32 DOOM...

    if not exist build mkdir build

    cl /nologo /O2 /W3 /wd4996 /wd4244 /wd4267 /D_CRT_SECURE_NO_WARNINGS ^
       /Isrc\doom /Isrc\hal\common ^
       src\doom\*.c src\hal\common\*.c src\hal\win32\*.c ^
       /link gdi32.lib winmm.lib /out:doom.exe

    if %errorlevel% equ 0 (
        echo.
        echo =======================================================
        echo MSVC Build Succeeded! Executable: doom.exe
        echo Run: doom.exe -iwad doom1.wad
        echo =======================================================
        goto end
    ) else (
        echo MSVC Build Failed.
        goto end
    )
)

rem Check for MinGW GCC
where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Detected MinGW / GCC compiler.
    echo Building using Makefile.win...
    make -f Makefile.win
    if %errorlevel% equ 0 (
        echo.
        echo =======================================================
        echo MinGW Build Succeeded! Executable: doom.exe
        echo Run: doom.exe -iwad doom1.wad
        echo =======================================================
        goto end
    ) else (
        echo MinGW Build Failed.
        goto end
    )
)

echo.
echo Error: Neither MSVC (cl.exe) nor MinGW (gcc.exe) was found in PATH.
echo Please run this script from:
echo   - "x64 Native Tools Command Prompt for VS" (for MSVC), OR
echo   - A command prompt with MinGW / MinGW-w64 in PATH.

:end

