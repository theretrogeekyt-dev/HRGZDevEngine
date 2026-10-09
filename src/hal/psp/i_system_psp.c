// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// System driver: Precision RTC/microsecond timing, memory allocation,
// clean kernel exit, on-screen debug console error reporting.
//
// Uses the official PSPDEV SDK (https://pspdev.github.io)
//
//-----------------------------------------------------------------------------

#include <pspkernel.h>
#include <psprtc.h>
#include <pspdebug.h>
#include <pspctrl.h>
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
    // On PSP (32MB RAM on Fat, 64MB on Slim), attempt descending zone allocations
    int try_mb[] = { 14, 12, 10, 8, 6 };
    int num_tries = sizeof(try_mb) / sizeof(try_mb[0]);

    int p = M_CheckParm("-mb");
    if (p && p < myargc - 1)
    {
        try_mb[0] = atoi(myargv[p + 1]);
        if (try_mb[0] < 4) try_mb[0] = 4;
        if (try_mb[0] > 24) try_mb[0] = 24;
    }

    for (int i = 0; i < num_tries; i++)
    {
        *size = try_mb[i] * 1024 * 1024;
        byte* zone = (byte*)malloc(*size);
        if (zone)
        {
            return zone;
        }
    }

    I_Error("I_ZoneBase: Failed to allocate contiguous DOOM zone memory (tried 14MB down to 6MB)");
    return NULL;
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

    // Save error to log file on memory stick
    FILE* logf = fopen("HRGZ_ERROR.TXT", "w");
    if (logf)
    {
        fprintf(logf, "HRGZDevEngine DOOM PSP FATAL ERROR:\n%s\n", msg);
        fclose(logf);
    }

    // Display error directly on PSP display using hardware debug screen
    pspDebugScreenInit();
    pspDebugScreenSetTextColor(0x000000FF); // Red
    pspDebugScreenPrintf("\n  =======================================================\n");
    pspDebugScreenPrintf("  HRGZDevEngine DOOM PSP - FATAL ERROR\n");
    pspDebugScreenPrintf("  =======================================================\n\n");
    pspDebugScreenSetTextColor(0x00FFFFFF); // White
    pspDebugScreenPrintf("  Error: %s\n\n", msg);
    pspDebugScreenSetTextColor(0x0000FFFF); // Yellow
    pspDebugScreenPrintf("  If files are missing, ensure DOOM1.WAD or DOOM.WAD is in:\n");
    pspDebugScreenPrintf("  ms0:/PSP/GAME/HRGZDOOM/\n\n");
    pspDebugScreenSetTextColor(0x00AAAAAA); // Grey
    pspDebugScreenPrintf("  Saved error details to HRGZ_ERROR.TXT\n");
    pspDebugScreenPrintf("  Press (X) or (O) to exit to PSP Home Menu.\n");

    // Sample controller and wait for user acknowledgment or 15-second timeout
    SceCtrlData pad;
    for (int i = 0; i < 900; i++)
    {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START))
        {
            break;
        }
        sceKernelDelayThread(16666);
    }

    sceKernelExitGame();
    exit(1);
}
