// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Main program entry point for MS-DOS.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_video.h"

int main(int argc, char** argv)
{
    myargc = argc;
    myargv = argv;

    printf("HRGZDevEngine DOOM (MS-DOS 32-bit Protected Mode)\n");

    D_DoomMain();

    return 0;
}

