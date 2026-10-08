// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	System interface for Modern SDL2 (timing, memory, error handling).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <SDL2/SDL.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_gamepad.h"
#include "m_argv.h"

static uint32_t sdl_start_ticks = 0;
static boolean  sdl_timer_inited = false;

void I_Init(void)
{
    if (sdl_timer_inited)
        return;

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    }

    sdl_start_ticks = SDL_GetTicks();
    sdl_timer_inited = true;
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
    if (!sdl_timer_inited)
        I_Init();

    uint32_t elapsed_ms = SDL_GetTicks() - sdl_start_ticks;
    return (int)((elapsed_ms * 35) / 1000);
}

ticcmd_t* I_BaseTiccmd(void)
{
    return I_Gamepad_BaseTiccmd();
}

void I_Quit(void)
{
    I_ShutdownGraphics();
    I_ShutdownSound();
    SDL_Quit();
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
    vsnprintf(msg, sizeof(msg), error, argptr);
    va_end(argptr);

    fprintf(stderr, "\nDOOM Error: %s\n\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "HRGZDevEngine DOOM Error", msg, NULL);
    SDL_Quit();

    exit(1);
}

