// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// Main entry point, PSP kernel callbacks, clock speed, and lifecycle.
//
// Uses the official PSPDEV SDK (https://pspdev.github.io)
//
//-----------------------------------------------------------------------------

#include <pspkernel.h>
#include <pspdebug.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

// PSP Homebrew Module Info
PSP_MODULE_INFO("HRGZ_DOOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-2048); // Reserve 2MB for OS/threads, rest for heap

static int exit_request = 0;

// Exit callback triggered when HOME/PS button is pressed on the PSP
static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    exit_request = 1;
    sceKernelExitGame();
    return 0;
}

// Background thread waiting for PSP kernel callbacks
static int CallbackThread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

// Sets up the callback thread
static int SetupCallbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
    {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

int main(int argc, char **argv)
{
    // Initialize callbacks so the user can exit to the PSP XMB
    SetupCallbacks();

    // Overclock the PSP to its official maximum 333 MHz performance mode (CPU 333, BUS 166)
    // Ensures locked 60 FPS software rendering and audio mixing
    scePowerSetClockFrequency(333, 333, 166);

    // Switch working directory to the directory of the EBOOT.PBP so doom1.wad is found
    if (argc > 0 && argv[0])
    {
        char path[256];
        strncpy(path, argv[0], sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        char *slash = strrchr(path, '/');
        if (!slash)
            slash = strrchr(path, '\\');
        if (slash)
        {
            *slash = '\0';
            chdir(path);
            strncpy(psp_game_dir, path, sizeof(psp_game_dir) - 1);
            psp_game_dir[sizeof(psp_game_dir) - 1] = '\0';
        }
    }

    myargc = argc;
    myargv = argv;

    // Launch DOOM engine
    D_DoomMain();

    return 0;
}
