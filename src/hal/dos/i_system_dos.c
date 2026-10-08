// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DOOM system interface for MS-DOS (DJGPP / Watcom).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined(__DJGPP__)
#include <sys/time.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#endif

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "m_argv.h"

#if defined(__DJGPP__)
static long base_sec = 0;
static int last_tics = 0;
#else
static clock_t start_clock;
#endif
static boolean timer_initialized = false;

void I_Init(void)
{
#if defined(__DJGPP__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    base_sec = tv.tv_sec;
    last_tics = 0;
#else
    start_clock = clock();
#endif
    timer_initialized = true;
}

byte* I_ZoneBase(int* size)
{
    // Default to 8MB or 16MB for DOS DOOM
    int mb = 8;
    int p = M_CheckParm("-mb");
    if (p && p < myargc - 1)
        mb = atoi(myargv[p + 1]);

    if (mb < 4) mb = 4;
    if (mb > 64) mb = 64;

    *size = mb * 1024 * 1024;
    byte* zone = (byte*)malloc(*size);
    while (!zone && mb > 4)
    {
        mb -= 2;
        *size = mb * 1024 * 1024;
        zone = (byte*)malloc(*size);
    }

    if (!zone)
    {
        *size = 4 * 1024 * 1024;
        zone = (byte*)malloc(*size);
        if (!zone)
            I_Error("I_ZoneBase: Failed to allocate %dMB zone memory", mb);
    }

    return zone;
}

int I_GetTime(void)
{
    if (!timer_initialized)
        I_Init();

#if defined(__DJGPP__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (!base_sec)
        base_sec = tv.tv_sec;

    int tics = (int)((tv.tv_sec - base_sec) * 35 + ((long long)tv.tv_usec * 35) / 1000000);
    if (tics < last_tics)
        tics = last_tics;
    else
        last_tics = tics;

    return tics;
#else
    clock_t now = clock();
    return (int)((now - start_clock) * 35 / CLOCKS_PER_SEC);
#endif
}

ticcmd_t emptycmd;
ticcmd_t* I_BaseTiccmd(void)
{
    return &emptycmd;
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
    va_list argptr;

    I_ShutdownGraphics();
    I_ShutdownSound();

    va_start(argptr, error);
    fprintf(stderr, "\nDOOM Error: ");
    vfprintf(stderr, error, argptr);
    fprintf(stderr, "\n\n");
    va_end(argptr);

    exit(1);
}

