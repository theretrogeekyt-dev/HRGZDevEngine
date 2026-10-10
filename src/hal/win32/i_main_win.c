// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Main entry point for Modern Windows (Win32 / Win64).
//	Supports both console and GUI subsystems (MSVC and MinGW).
//
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

int main(int argc, char** argv)
{
    // Ensure working directory is set to the binary's directory so relative paths
    // (game.wad, game.json, etc.) resolve when launched from Explorer or shortcuts.
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0)
    {
        char* lastSlash = strrchr(exePath, '\\');
        if (!lastSlash)
            lastSlash = strrchr(exePath, '/');
        if (lastSlash)
        {
            *lastSlash = '\0';
            SetCurrentDirectoryA(exePath);
            SetEnvironmentVariableA("DOOMWADDIR", exePath);
        }
    }

    myargc = argc;
    myargv = argv;

    D_DoomMain();

    return 0;
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    return main(__argc, __argv);
}
#endif

