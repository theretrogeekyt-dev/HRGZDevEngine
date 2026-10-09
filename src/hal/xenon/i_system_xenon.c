//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM - Xbox 360 HAL: System Driver
// High-resolution 50 MHz PowerPC TimeBase clock, Zone Memory, and Exception Handling
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#if !defined(__powerpc__) && !defined(__ppc__) && !defined(__PPC__)
#include <sys/time.h>
#endif

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "i_sound.h"
#include "../common/i_gamepad.h"
#include "m_argv.h"

#if defined(LIBXENON)
#include <xenon_soc/xenon_power.h>
#include <ppc/timebase.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif

// Xbox 360 Xenon TimeBase frequency is 50,000,000 Hz (50 MHz)
#define XENON_TIMEBASE_FREQ 50000000ULL

static uint64_t xenon_start_tb = 0;
static boolean  xenon_timer_inited = false;

static inline uint64_t Xenon_ReadTimeBase(void)
{
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#if defined(__ppc64__) || defined(__powerpc64__) || defined(_ARCH_PPC64)
    uint64_t tb;
    asm volatile("mftb %0" : "=r"(tb));
    return tb;
#else
    uint32_t tbu1, tbl, tbu2;
    do {
        asm volatile("mftbu %0" : "=r"(tbu1));
        asm volatile("mftb  %0" : "=r"(tbl));
        asm volatile("mftbu %0" : "=r"(tbu2));
    } while (tbu1 != tbu2);
    return (((uint64_t)tbu1) << 32) | (uint64_t)tbl;
#endif
#elif defined(_XBOX)
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (uint64_t)li.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec);
#endif
}

void I_Init(void)
{
    if (xenon_timer_inited)
        return;

    xenon_start_tb = Xenon_ReadTimeBase();
    xenon_timer_inited = true;
    printf("I_Init (Xbox 360): Xenon system timer initialized\n");
}

byte* I_ZoneBase(int* size)
{
    // The Xbox 360 has 512MB GDDR3 unified memory; allocate 32MB for DOOM by default
    int mb = 32;
    int p = M_CheckParm("-mb");
    if (p && p < myargc - 1)
        mb = atoi(myargv[p + 1]);

    if (mb < 8) mb = 8;
    if (mb > 128) mb = 128;

    *size = mb * 1024 * 1024;
    byte* zone = (byte*)malloc(*size);
    if (!zone)
    {
        *size = 16 * 1024 * 1024;
        zone = (byte*)malloc(*size);
        if (!zone)
            I_Error("I_ZoneBase (Xbox 360): Failed to allocate %dMB zone memory", mb);
    }
    printf("I_ZoneBase (Xbox 360): Allocated %dMB heap zone\n", *size / (1024 * 1024));
    return zone;
}

int I_GetTime(void)
{
    if (!xenon_timer_inited)
        I_Init();

    uint64_t current_tb = Xenon_ReadTimeBase();
    uint64_t elapsed = current_tb - xenon_start_tb;

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
    return (int)((elapsed * 35ULL) / XENON_TIMEBASE_FREQ);
#elif defined(_XBOX)
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (int)((elapsed * 35ULL) / (uint64_t)freq.QuadPart);
#else
    // Host fallback (microseconds)
    return (int)((elapsed * 35ULL) / 1000000ULL);
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

#if defined(LIBXENON)
    printf("HRGZDevEngine DOOM: Returning to XeLL...\n");
    xenon_return_to_xell();
#elif defined(_XBOX)
    XLaunchNewImage(NULL, NULL);
#else
    exit(0);
#endif
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

    fprintf(stderr, "\n========================================\n");
    fprintf(stderr, "XBOX 360 DOOM FATAL ERROR:\n%s\n", msg);
    fprintf(stderr, "========================================\n\n");

#if defined(LIBXENON)
    xenon_return_to_xell();
#elif defined(_XBOX)
    XLaunchNewImage(NULL, NULL);
#else
    exit(1);
#endif
}

