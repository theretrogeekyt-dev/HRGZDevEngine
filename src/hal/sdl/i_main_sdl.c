// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Main entry point for Modern SDL2 (Windows, macOS, Linux).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

int main(int argc, char** argv)
{
    myargc = argc;
    myargv = argv;

    printf("HRGZDevEngine DOOM (Modern SDL2)\n");

    D_DoomMain();

    return 0;
}

