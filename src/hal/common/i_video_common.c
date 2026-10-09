// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Shared display resolution presets, CLI argument parsing, and state
//	management for native presentation drivers (Metal, OpenGL, SDL2, Test).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "m_argv.h"
#include "i_video.h"
#include "i_video_common.h"

// Standard display output resolution presets
const display_resolution_t display_resolutions[NUM_DISPLAY_RESOLUTIONS] = {
    {  854,  480, "854x480 (480p SD)"    },
    { 1280,  720, "1280x720 (720p HD)"   },
    { 1600,  900, "1600x900 (900p HD+)"  },
    { 1920, 1080, "1920x1080 (1080p FHD)"},
    { 2560, 1440, "2560x1440 (1440p QHD)"},
    { 3840, 2160, "3840x2160 (4K UHD)"   },
    {  640,  480, "640x480 (Classic)"    }
};

int current_resolution_index = 1; // Default to 1280x720 (720p HD)
int display_width = 1280;
int display_height = 720;
boolean display_fullscreen = false;

int I_FindResolutionIndex(int width, int height)
{
    // Exact match first
    for (int i = 0; i < NUM_DISPLAY_RESOLUTIONS; i++)
    {
        if (display_resolutions[i].width == width &&
            display_resolutions[i].height == height)
        {
            return i;
        }
    }

    // Closest match by dimension delta
    int best_idx = 1; // 720p fallback
    int best_diff = 1000000;
    for (int i = 0; i < NUM_DISPLAY_RESOLUTIONS; i++)
    {
        int diff = abs(display_resolutions[i].width - width) +
                   abs(display_resolutions[i].height - height);
        if (diff < best_diff)
        {
            best_diff = diff;
            best_idx = i;
        }
    }
    return best_idx;
}

void I_ParseDisplayParams(int* out_width, int* out_height, boolean* out_fullscreen)
{
    int w = 1280;
    int h = 720;
    boolean fs = false;

    // Direct resolution preset shortcuts
    if (M_CheckParm("-480p"))   { w = 854;  h = 480;  }
    if (M_CheckParm("-720p"))   { w = 1280; h = 720;  }
    if (M_CheckParm("-900p"))   { w = 1600; h = 900;  }
    if (M_CheckParm("-1080p"))  { w = 1920; h = 1080; }
    if (M_CheckParm("-1440p"))  { w = 2560; h = 1440; }
    if (M_CheckParm("-4k") || M_CheckParm("-2160p")) { w = 3840; h = 2160; }

    // Custom resolution: -res <w>x<h>
    int p = M_CheckParm("-res");
    if (p && p < myargc - 1)
    {
        int rw = 0, rh = 0;
        if (sscanf(myargv[p + 1], "%dx%d", &rw, &rh) == 2 && rw > 0 && rh > 0)
        {
            w = rw;
            h = rh;
        }
    }

    // Custom width and height: -width <w> -height <h>
    int pw = M_CheckParm("-width");
    int ph = M_CheckParm("-height");
    if (pw && pw < myargc - 1)
    {
        int custom_w = atoi(myargv[pw + 1]);
        if (custom_w > 0) w = custom_w;
    }
    if (ph && ph < myargc - 1)
    {
        int custom_h = atoi(myargv[ph + 1]);
        if (custom_h > 0) h = custom_h;
    }

    // Legacy multiplier flag: -scale <1..6>
    int ps = M_CheckParm("-scale");
    if (ps && ps < myargc - 1)
    {
        int scale = atoi(myargv[ps + 1]);
        if (scale < 1) scale = 1;
        if (scale > 6) scale = 6;
        w = SCREENWIDTH * scale;
        h = (int)(SCREENHEIGHT * scale * 1.2);
    }

    // Display mode: fullscreen vs windowed
    if (M_CheckParm("-fullscreen") || M_CheckParm("-fs"))
        fs = true;
    if (M_CheckParm("-windowed") || M_CheckParm("-window") || M_CheckParm("-win"))
        fs = false;

    *out_width = w;
    *out_height = h;
    *out_fullscreen = fs;

    display_width = w;
    display_height = h;
    display_fullscreen = fs;
    current_resolution_index = I_FindResolutionIndex(w, h);
}

void I_SetResolutionIndex(int index, boolean fullscreen)
{
    if (index < 0) index = 0;
    if (index >= NUM_DISPLAY_RESOLUTIONS) index = NUM_DISPLAY_RESOLUTIONS - 1;

    current_resolution_index = index;
    I_SetResolution(display_resolutions[index].width,
                    display_resolutions[index].height,
                    fullscreen);
}

void I_GetResolution(int* width, int* height, boolean* fullscreen)
{
    if (width) *width = display_width;
    if (height) *height = display_height;
    if (fullscreen) *fullscreen = display_fullscreen;
}
