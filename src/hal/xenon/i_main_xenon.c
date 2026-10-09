//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM - Xbox 360 HAL: Main Entry Point
// Autodetection of storage devices, IWAD discovery, and Peer Pressure integration
//
//-----------------------------------------------------------------------------

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"

#if defined(LIBXENON)
#include <xenos/xenos.h>
#include <console/console.h>
#include <usb/usb.h>
#include <diskio/ata.h>
#endif

// Common IWAD candidate paths across Xbox 360 storage devices
static const char* const candidate_wads[] = {
    // Current working directory
    "doom2.wad",
    "DOOM2.WAD",
    "doom.wad",
    "DOOM.WAD",
    "doom1.wad",
    "DOOM1.WAD",
    "tnt.wad",
    "plutonia.wad",

    // Dashboard game directory
    "game:\\doom2.wad",
    "game:\\DOOM2.WAD",
    "game:\\doom.wad",
    "game:\\DOOM.WAD",
    "game:\\doom1.wad",
    "game:\\DOOM1.WAD",

    // USB drive mount points (LibXenon & Dashboard)
    "usb:\\doom2.wad",
    "usb:\\doom1.wad",
    "usb:\\doom.wad",
    "uda:\\doom2.wad",
    "uda:\\doom1.wad",
    "uda:\\doom.wad",
    "uda0:\\doom2.wad",
    "uda0:\\doom1.wad",
    "uda0:\\doom.wad",
    "/usb/doom2.wad",
    "/usb/doom1.wad",
    "/usb/doom.wad",
    "/uda/doom2.wad",
    "/uda/doom1.wad",
    "/uda/doom.wad",

    // Peer Pressure Softmod directories on Hard Drive
    "Hdd1:\\PeerPressure\\OtherOS\\doom2.wad",
    "Hdd1:\\PeerPressure\\OtherOS\\doom1.wad",
    "Hdd1:\\PeerPressure\\OtherOS\\doom.wad",
    "Hdd1:\\Content\\0000000000000000\\doom2.wad",
    "Hdd1:\\Content\\0000000000000000\\doom1.wad",
    "hdd:\\doom2.wad",
    "hdd:\\doom1.wad",
    "hdd:\\doom.wad",
    "/sda/doom2.wad",
    "/sda/doom1.wad",
    "/sda/doom.wad",

    NULL
};

static int FileExists(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (fp)
    {
        fclose(fp);
        return 1;
    }
    return 0;
}

static const char* AutoDetectIWAD(void)
{
    for (int i = 0; candidate_wads[i] != NULL; i++)
    {
        if (FileExists(candidate_wads[i]))
        {
            return candidate_wads[i];
        }
    }
    return NULL;
}

int main(int argc, char** argv)
{
    printf("=======================================================\n");
    printf("HRGZDevEngine DOOM — Xbox 360 Edition\n");
    printf("Optimized for Xbox 360 'Peer Pressure' Softmod & XeLL\n");
    printf("=======================================================\n");

#if defined(LIBXENON)
    // Initialize LibXenon console hardware, USB, and SATA HDD
    usb_init();
    xenon_ata_init();
#endif

    myargc = argc;
    myargv = argv;

    // Check if user already provided -iwad
    int has_iwad_arg = 0;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-iwad") == 0)
        {
            has_iwad_arg = 1;
            break;
        }
    }

    // If no -iwad argument provided, search Xbox 360 storage devices
    if (!has_iwad_arg)
    {
        const char* detected_iwad = AutoDetectIWAD();
        if (detected_iwad)
        {
            printf("Xbox 360 Autodiscovery: Found IWAD at '%s'\n", detected_iwad);

            // Construct augmented argument vector
            char** new_argv = (char**)malloc((argc + 3) * sizeof(char*));
            for (int i = 0; i < argc; i++)
            {
                new_argv[i] = argv[i];
            }
            new_argv[argc]     = strdup("-iwad");
            new_argv[argc + 1] = strdup(detected_iwad);
            new_argv[argc + 2] = NULL;

            myargc = argc + 2;
            myargv = new_argv;
        }
        else
        {
            printf("Xbox 360 Autodiscovery: No IWAD found on USB/HDD. Falling back to default search.\n");
        }
    }

    D_DoomMain();

    return 0;
}

