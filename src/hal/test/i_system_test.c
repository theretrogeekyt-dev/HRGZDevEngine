// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	System interface for automated test runner.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "m_argv.h"

static struct timeval start_time;
static boolean timer_inited = false;
static int simulated_tic = 0;
static boolean realtime_mode = false;

void I_Init(void)
{
    gettimeofday(&start_time, NULL);
    timer_inited = true;
    if (M_CheckParm("-realtime"))
        realtime_mode = true;
}

byte* I_ZoneBase(int* size)
{
    int mb = 16;
    *size = mb * 1024 * 1024;
    byte* zone = (byte*)malloc(*size);
    if (!zone)
    {
        *size = 8 * 1024 * 1024;
        zone = (byte*)malloc(*size);
        if (!zone)
            I_Error("I_ZoneBase: Failed to allocate test zone memory");
    }
    return zone;
}

int I_GetTime(void)
{
    if (!timer_inited)
        I_Init();

    if (!realtime_mode)
    {
        // Deterministic simulated tics for testing
        return simulated_tic++;
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    long sec = now.tv_sec - start_time.tv_sec;
    long usec = now.tv_usec - start_time.tv_usec;
    return (int)((sec * 1000000 + usec) * 35 / 1000000);
}

ticcmd_t emptycmd_test;
ticcmd_t* I_BaseTiccmd(void)
{
    return &emptycmd_test;
}

void I_Quit(void)
{
    I_ShutdownGraphics();
    I_ShutdownSound();
    printf("I_Quit: Clean exit.\n");
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
    fprintf(stderr, "\nTEST FAILURE - DOOM Error: ");
    vfprintf(stderr, error, argptr);
    fprintf(stderr, "\n\n");
    va_end(argptr);

    exit(1);
}

