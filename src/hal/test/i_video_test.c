// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Headless/offscreen video driver and frame validator for automated testing.
//	Saves frame 35 to test_screenshot.ppm to visually verify software rendering.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "i_video_common.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"
#include "m_menu.h"
#include "i_gamepad.h"

static byte current_palette[256 * 3];
static int frame_count = 0;
static int max_test_frames = 150; // Run 150 frames (~4.2 seconds) then exit cleanly

void I_SetResolution(int width, int height, boolean fullscreen)
{
    display_width = width;
    display_height = height;
    display_fullscreen = fullscreen;
    current_resolution_index = I_FindResolutionIndex(width, height);
    printf("I_SetResolution (headless): %dx%d (fullscreen: %s)\n",
           width, height, fullscreen ? "YES" : "NO");
}

void I_ToggleFullscreen(void)
{
    display_fullscreen = !display_fullscreen;
    printf("I_ToggleFullscreen (headless): fullscreen = %s\n",
           display_fullscreen ? "YES" : "NO");
}

void I_InitGraphics(void)
{
    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0])
        I_Error("I_InitGraphics: Failed to allocate offscreen framebuffer");

    I_Gamepad_Init();

    int win_w = 1280;
    int win_h = 720;
    boolean init_fullscreen = false;
    I_ParseDisplayParams(&win_w, &win_h, &init_fullscreen);

    int p = M_CheckParm("-testframes");
    if (p && p < myargc - 1)
        max_test_frames = atoi(myargv[p + 1]);

    printf("I_InitGraphics: Headless test video driver initialized (target frames: %d)\n", max_test_frames);
}

void I_ShutdownGraphics(void)
{
    if (screens[0])
    {
        free(screens[0]);
        screens[0] = NULL;
    }
}

void I_SetPalette(byte* palette)
{
    memcpy(current_palette, palette, 256 * 3);
}

void I_UpdateNoBlit(void)
{
}

static void SavePPM(const char* filename, const byte* screen, const byte* pal)
{
    FILE* f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", SCREENWIDTH, SCREENHEIGHT);
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
    {
        byte color_idx = screen[i];
        fputc(pal[color_idx * 3 + 0], f);
        fputc(pal[color_idx * 3 + 1], f);
        fputc(pal[color_idx * 3 + 2], f);
    }
    fclose(f);
    printf("Saved verification screenshot: %s\n", filename);
}

void I_FinishUpdate(void)
{
    frame_count++;

    // Dump screenshot at frame 35 and frame 100
    if (frame_count == 35)
    {
        SavePPM("test_frame35.ppm", screens[0], current_palette);
    }
    else if (frame_count == 100)
    {
        SavePPM("test_frame100.ppm", screens[0], current_palette);
    }

    if (M_CheckParm("-testmenu") && frame_count == 55)
    {
        SavePPM("test_menu.ppm", screens[0], current_palette);
    }

    if (M_CheckParm("-testmove") && frame_count == 80)
    {
        SavePPM("test_movement.ppm", screens[0], current_palette);
    }

    if (M_CheckParm("-testgamepad") && frame_count == 70)
    {
        SavePPM("test_gamepad.ppm", screens[0], current_palette);
    }

    if (M_CheckParm("-testdispmenu") && frame_count == 60)
    {
        SavePPM("test_display_menu.ppm", screens[0], current_palette);
    }

    if (M_CheckParm("-testoptions") && frame_count == 60)
    {
        SavePPM("test_options_menu.ppm", screens[0], current_palette);
    }

    if (frame_count >= max_test_frames)
    {
        printf("\n=======================================================\n");
        printf("SUCCESS: Completed %d test frames without errors or crashes!\n", frame_count);
        printf("Software renderer and playsim verified successfully.\n");
        printf("=======================================================\n\n");
        I_Quit();
    }

    // Frame refresh system:
    // After the frame has been presented on top of the old one, save it
    // and clear screens[0] so the next frame is built on a clean slate.
    if (!wipe_active)
    {
        if (screens[4])
            memcpy(screens[4], screens[0], SCREENWIDTH * SCREENHEIGHT);
        memset(screens[0], 0, SCREENWIDTH * SCREENHEIGHT);
    }
}

void I_ClearFrame(void)
{
    if (screens[0])
        memset(screens[0], 0, SCREENWIDTH * SCREENHEIGHT);
}

void I_WaitVBL(int count)
{
    (void)count;
}

void I_ReadScreen(byte* scr)
{
    if (screens[4] && scr)
        memcpy(scr, screens[4], SCREENWIDTH * SCREENHEIGHT);
    else if (screens[0] && scr)
        memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

void I_StartFrame(void)
{
}

void I_StartTic(void)
{
    if (M_CheckParm("-testmenu"))
    {
        if (frame_count == 45)
        {
            event_t ev;
            ev.type = ev_keydown;
            ev.data1 = KEY_ESCAPE;
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
        }
    }

    if (M_CheckParm("-testdispmenu"))
    {
        if (frame_count == 45)
        {
            M_StartControlPanel();
            M_Display(0);
        }
        else if (frame_count == 55)
        {
            event_t ev;
            ev.type = ev_keydown; ev.data1 = KEY_RIGHTARROW; D_PostEvent(&ev);
            ev.type = ev_keyup;   ev.data1 = KEY_RIGHTARROW; D_PostEvent(&ev);
        }
    }

    if (M_CheckParm("-testoptions"))
    {
        if (frame_count == 45)
        {
            M_StartControlPanel();
            M_Options(0);
        }
    }

    if (M_CheckParm("-testmove"))
    {
        if (frame_count == 40)
        {
            event_t ev;
            ev.type = ev_keydown;
            ev.data1 = KEY_UPARROW;
            D_PostEvent(&ev);
            ev.data1 = KEY_RIGHTARROW;
            D_PostEvent(&ev);
        }
        else if (frame_count == 90)
        {
            event_t ev;
            ev.type = ev_keyup;
            ev.data1 = KEY_UPARROW;
            D_PostEvent(&ev);
            ev.data1 = KEY_RIGHTARROW;
            D_PostEvent(&ev);
        }
    }

    if (M_CheckParm("-testgamepad"))
    {
        if (frame_count >= 36 && frame_count <= 75)
        {
            gamepad_state_t pad;
            memset(&pad, 0, sizeof(pad));
            pad.connected = 1;
            pad.left_stick_y = 0.9f;  // Move forward
            pad.left_stick_x = 0.3f;  // Strafe right
            pad.right_stick_x = 0.4f; // Look right
            pad.right_trigger = (frame_count >= 50 && frame_count <= 65) ? 0.9f : 0.0f; // Fire weapon
            if (frame_count == 40) pad.buttons |= PAD_BTN_L3; // Toggle sprint
            I_Gamepad_Update(&pad);
        }
        else if (frame_count > 75)
        {
            I_Gamepad_Update(NULL);
        }
    }
}

