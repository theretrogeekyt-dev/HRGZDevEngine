// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Main entry point for Modern SDL2 (Windows, macOS, Linux).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <SDL2/SDL.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

int main(int argc, char** argv)
{
    // Auto-resolve base directory where binary is located so game.wad / game.json are found
    char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        size_t len = strlen(basePath);
        if (len > 0 && (basePath[len - 1] == '/' || basePath[len - 1] == '\\'))
        {
            basePath[len - 1] = '\0';
        }
#if defined(_WIN32)
        SetCurrentDirectoryA(basePath);
        SetEnvironmentVariableA("DOOMWADDIR", basePath);
#else
        chdir(basePath);
        setenv("DOOMWADDIR", basePath, 1);
#endif
        SDL_free(basePath);
    }

    myargc = argc;
    myargv = argv;

    printf("HRGZDevEngine DOOM (Modern SDL2)\n");

    D_DoomMain();

    return 0;
}

