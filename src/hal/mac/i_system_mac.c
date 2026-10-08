// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Native macOS system interface (mach_absolute_time, zone memory, exit).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <mach/mach_time.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "m_argv.h"

static uint64_t start_mach_time = 0;
static mach_timebase_info_data_t timebase_info;
static boolean timer_inited = false;

void I_Init(void)
{
    if (timer_inited)
        return;

    mach_timebase_info(&timebase_info);
    start_mach_time = mach_absolute_time();
    timer_inited = true;
}

byte* I_ZoneBase(int* size)
{
    int mb = 16;
    int p = M_CheckParm("-mb");
    if (p && p < myargc - 1)
        mb = atoi(myargv[p + 1]);

    if (mb < 8) mb = 8;
    if (mb > 128) mb = 128;

    *size = mb * 1024 * 1024;
    byte* zone = (byte*)malloc(*size);
    if (!zone)
    {
        *size = 8 * 1024 * 1024;
        zone = (byte*)malloc(*size);
        if (!zone)
            I_Error("I_ZoneBase: Failed to allocate %dMB zone memory", mb);
    }
    return zone;
}

int I_GetTime(void)
{
    if (!timer_inited)
        I_Init();

    uint64_t elapsed = mach_absolute_time() - start_mach_time;
    uint64_t elapsed_ns = elapsed * timebase_info.numer / timebase_info.denom;
    return (int)((elapsed_ns * 35) / 1000000000ULL);
}

ticcmd_t emptycmd_mac;
ticcmd_t* I_BaseTiccmd(void)
{
    return &emptycmd_mac;
}

void I_Quit(void)
{
    I_ShutdownGraphics();
    I_ShutdownSound();
    printf("\nExiting DOOM...\n");
    exit(0);
}

byte* I_AllocLow(int length)
{
    return (byte*)malloc(length);
}

void I_Tactile(int on, int off, int total)
{
    (void)on; (void)off; (void)total;
}

void I_Error(char* error, ...)
{
    char msg[1024];
    va_list argptr;

    I_ShutdownGraphics();
    I_ShutdownSound();

    va_start(argptr, error);
    vsprintf(msg, error, argptr);
    va_end(argptr);

    fprintf(stderr, "\nFATAL ERROR: %s\n", msg);
    exit(1);
}

