// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// System driver: Precision RTC/microsecond timing, memory allocation,
// clean kernel exit, and error handling.
//
// Uses the official PSPDEV SDK (https://pspdev.github.io)
//
//-----------------------------------------------------------------------------

#include <pspkernel.h>
#include <psprtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_gamepad.h"
#include "m_argv.h"

static u64 psp_start_ticks = 0;
static u32 psp_tick_res = 0;
static boolean psp_timer_inited = false;

void I_Init(void)
{
    if (psp_timer_inited)
        return;

    psp_tick_res = sceRtcGetTickResolution();
    if (psp_tick_res == 0)
        psp_tick_res = 1000000; // Fallback to 1 MHz

    sceRtcGetCurrentTick((u64*)&psp_start_ticks);
    psp_timer_inited = true;
}

byte* I_ZoneBase(int* size)
{
    // On PSP (32MB RAM on Fat, 64MB on Slim), reserve 12-16MB for DOOM heap zone
    int mb = 14;
    int p = M_CheckParm("-mb");
    if (p && p < myargc - 1)
        mb = atoi(myargv[p + 1]);

    if (mb < 6) mb = 6;
    if (mb > 24) mb = 24;

    *size = mb * 1024 * 1024;
    byte* zone = (byte*)malloc(*size);
    if (!zone)
    {
        // Try falling back to 8MB if 14MB is not contiguous
        *size = 8 * 1024 * 1024;
        zone = (byte*)malloc(*size);
        if (!zone)
        {
            I_Error("I_ZoneBase: Failed to allocate %dMB zone memory on PSP", mb);
        }
    }
    return zone;
}

int I_GetTime(void)
{
    if (!psp_timer_inited)
        I_Init();

    u64 now;
    sceRtcGetCurrentTick(&now);

    u64 elapsed = now - psp_start_ticks;
    // Convert elapsed ticks to 35 Hz DOOM tics
    return (int)((elapsed * 35) / psp_tick_res);
}

ticcmd_t* I_BaseTiccmd(void)
{
    return I_Gamepad_BaseTiccmd();
}

void I_Quit(void)
{
    I_ShutdownGraphics();
    I_ShutdownSound();
    sceKernelExitGame();
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

    fprintf(stderr, "\n=======================================================\n");
    fprintf(stderr, "HRGZDevEngine DOOM PSP FATAL ERROR:\n%s\n", msg);
    fprintf(stderr, "=======================================================\n\n");

    sceKernelDelayThread(3000000); // 3 seconds delay so developer/player can read error on screen
    sceKernelExitGame();
    exit(1);
}
