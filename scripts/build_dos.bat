@echo off
rem ===========================================================================
rem HRGZDevEngine DOOM - MS-DOS Build Script (DJGPP / FreeDOS / DOSBox)
rem
rem Prerequisites:
rem   - DJGPP installed with GCC and MAKE
rem   - DJGPP environment variable set (e.g., set DJGPP=C:\DJGPP\DJGPP.ENV)
rem   - CWSDPMI.EXE in PATH or working directory
rem ===========================================================================

echo =======================================================
echo Building HRGZDevEngine DOOM for MS-DOS (DJGPP Mode 13h)
echo =======================================================

if exist Makefile.dos (
    make -f Makefile.dos
) else (
    echo Error: Makefile.dos not found.
    goto end
)

if exist DOOM.EXE (
    echo.
    echo =======================================================
    echo MS-DOS Build Succeeded! Executable: DOOM.EXE
    echo Run: DOOM.EXE -iwad DOOM1.WAD
    echo =======================================================
) else (
    echo.
    echo Build failed. Please check compiler output.
)

:end

