// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Main entry point for automated test runner.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

int main(int argc, char** argv)
{
    myargc = argc;
    myargv = argv;

    printf("=======================================================\n");
    printf("HRGZDevEngine DOOM: Automated Test & Validation Runner\n");
    printf("=======================================================\n\n");

    D_DoomMain();

    return 0;
}

