// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	System interface for Modern Windows (high-precision QPC timer, memory, errors).
//
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_gamepad.h"
#include "m_argv.h"

static LARGE_INTEGER qpc_freq;
static LARGE_INTEGER qpc_start;
static boolean timer_initialized = false;

void I_Init(void)
{
    if (timer_initialized)
        return;

#if defined(_WIN32)
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&qpc_freq);
    QueryPerformanceCounter(&qpc_start);
#endif
    timer_initialized = true;
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

#if defined(_WIN32)
    byte* zone = (byte*)VirtualAlloc(NULL, *size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    byte* zone = (byte*)malloc(*size);
#endif

    if (!zone)
    {
        *size = 8 * 1024 * 1024;
#if defined(_WIN32)
        zone = (byte*)VirtualAlloc(NULL, *size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        zone = (byte*)malloc(*size);
#endif
        if (!zone)
            I_Error("I_ZoneBase: Failed to allocate %dMB zone memory", mb);
    }

    return zone;
}

int I_GetTime(void)
{
    if (!timer_initialized)
        I_Init();

#if defined(_WIN32)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    int64_t elapsed = now.QuadPart - qpc_start.QuadPart;
    return (int)((elapsed * 35) / qpc_freq.QuadPart);
#else
    return 0;
#endif
}

ticcmd_t* I_BaseTiccmd(void)
{
    return I_Gamepad_BaseTiccmd();
}

void I_Quit(void)
{
    I_ShutdownGraphics();
    I_ShutdownSound();

#if defined(_WIN32)
    timeEndPeriod(1);
#endif

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

#if defined(_WIN32)
    timeEndPeriod(1);
#endif

    va_start(argptr, error);
    vsnprintf(msg, sizeof(msg), error, argptr);
    va_end(argptr);

    fprintf(stderr, "\nDOOM Error: %s\n\n", msg);

#if defined(_WIN32)
    MessageBoxA(NULL, msg, "HRGZDevEngine DOOM Error", MB_OK | MB_ICONERROR);
#endif

    exit(1);
}

