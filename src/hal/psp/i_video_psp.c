// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// Video driver: 480x272 Widescreen display, double-buffered eDRAM VRAM,
// palette conversion, and PSP controller input integration.
//
// Uses the official PSPDEV SDK (https://pspdev.github.io)
//
//-----------------------------------------------------------------------------

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"
#include "d_event.h"
#include "../common/i_gamepad.h"

// PSP Screen Specifications
#define PSP_SCREEN_WIDTH   480
#define PSP_SCREEN_HEIGHT  272
#define PSP_BUF_STRIDE     512
#define PSP_FRAME_SIZE     (PSP_BUF_STRIDE * PSP_SCREEN_HEIGHT * sizeof(uint32_t))

// CPU writes to UNCACHED VRAM (0x44000000) to bypass CPU cache flushing
static uint32_t* vram_cpu[2] = { NULL, NULL };

// Display controller hardware reads from CACHED physical VRAM (0x04000000)
static void* vram_display[2] = { NULL, NULL };
static int current_buffer = 0;

// 256-color 32-bit RGBA palette lookup table
static uint32_t psp_palette[256];

// Scaling look-up tables (precomputed 426x200 -> 480x272)
static int scale_lut_x[PSP_SCREEN_WIDTH];
static int scale_lut_y[PSP_SCREEN_HEIGHT];
static boolean lut_initialized = false;

// Display aspect scaling mode: 0 = 480x272 Full Widescreen, 1 = 426x200 Centered Pixel-Perfect
static int psp_scaling_mode = 0;

// Previous controller state to detect button edges for menu events
static uint32_t last_buttons = 0;

// Display resolution globals
int current_resolution_index = 0;
int display_width = PSP_SCREEN_WIDTH;
int display_height = PSP_SCREEN_HEIGHT;
boolean display_fullscreen = true;

const display_resolution_t display_resolutions[NUM_DISPLAY_RESOLUTIONS] = {
    { 480, 272, "480x272 (Full Widescreen)" },
    { 426, 200, "426x200 (Pixel-Perfect)"   },
    { 480, 272, "480x272 (Preset 2)"        },
    { 480, 272, "480x272 (Preset 3)"        },
    { 480, 272, "480x272 (Preset 4)"        },
    { 480, 272, "480x272 (Preset 5)"        },
    { 480, 272, "480x272 (Preset 6)"        }
};

static void InitScalingLUT(void)
{
    if (lut_initialized)
        return;

    for (int x = 0; x < PSP_SCREEN_WIDTH; x++)
    {
        int sx = (x * SCREENWIDTH) / PSP_SCREEN_WIDTH;
        if (sx >= SCREENWIDTH) sx = SCREENWIDTH - 1;
        scale_lut_x[x] = sx;
    }

    for (int y = 0; y < PSP_SCREEN_HEIGHT; y++)
    {
        int sy = (y * SCREENHEIGHT) / PSP_SCREEN_HEIGHT;
        if (sy >= SCREENHEIGHT) sy = SCREENHEIGHT - 1;
        scale_lut_y[y] = sy;
    }

    lut_initialized = true;
}

void I_InitGraphics(void)
{
    void* edram = sceGeEdramGetAddr();
    if (!edram) edram = (void*)0x04000000;

    vram_display[0] = edram;
    vram_display[1] = (void*)((uint8_t*)edram + PSP_FRAME_SIZE);

    vram_cpu[0] = (uint32_t*)((uintptr_t)edram | 0x40000000);
    vram_cpu[1] = (uint32_t*)(((uintptr_t)edram + PSP_FRAME_SIZE) | 0x40000000);

    InitScalingLUT();

    // Set PSP Display mode to 480x272, 32-bit RGBA (8888)
    sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

    // Clear both framebuffers to solid black
    memset((void*)vram_cpu[0], 0, PSP_FRAME_SIZE);
    memset((void*)vram_cpu[1], 0, PSP_FRAME_SIZE);

    // Present initial buffer using physical VRAM address
    sceDisplaySetFrameBuf(vram_display[0], PSP_BUF_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
    current_buffer = 0;

    // Ensure DOOM refresh screens are allocated
    if (!screens[0]) screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[4]) screens[4] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0] || !screens[4])
    {
        I_Error("I_InitGraphics: Failed to allocate DOOM frame buffers");
    }

    // Initialize controller sampling with analog nub enabled
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Initialize unified gamepad abstraction
    I_Gamepad_Init();
}

void I_ShutdownGraphics(void)
{
    // screens[0..4] are allocated globally in V_Init() and should not be freed as interior pointers
}

void I_SetPalette(byte* palette)
{
    if (!palette)
        return;

    int g = usegamma;
    if (g < 0) g = 0;
    if (g > 4) g = 4;

    // Convert 256 8-bit RGB triplets into 32-bit RGBA (0xAABBGGRR in little-endian Allegrex MIPS)
    for (int i = 0; i < 256; i++)
    {
        byte r = gammatable[g][*palette++];
        byte g_val = gammatable[g][*palette++];
        byte b = gammatable[g][*palette++];

        psp_palette[i] = (0xFF << 24) | ((uint32_t)b << 16) | ((uint32_t)g_val << 8) | (uint32_t)r;
    }
}

void I_StartFrame(void)
{
}

void I_StartTic(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    gamepad_state_t state;
    memset(&state, 0, sizeof(state));
    state.connected = 1;

    int in_menu = (menuactive || gamestate != GS_LEVEL || demoplayback);

    if (in_menu)
    {
        // -------------------------------------------------------------
        // Menu Navigation Mode:
        // Direct digital input with single event dispatch and debounce
        // -------------------------------------------------------------
        if (pad.Buttons & PSP_CTRL_UP)        state.buttons |= PAD_BTN_DPAD_UP;
        if (pad.Buttons & PSP_CTRL_DOWN)      state.buttons |= PAD_BTN_DPAD_DN;
        if (pad.Buttons & PSP_CTRL_LEFT)      state.buttons |= PAD_BTN_DPAD_LF;
        if (pad.Buttons & PSP_CTRL_RIGHT)     state.buttons |= PAD_BTN_DPAD_RT;

        // Also allow Nub in menus
        float ny = ((float)pad.Ly - 128.0f) / 128.0f;
        float nx = ((float)pad.Lx - 128.0f) / 128.0f;
        if (ny < -0.45f) state.buttons |= PAD_BTN_DPAD_UP;
        else if (ny > 0.45f) state.buttons |= PAD_BTN_DPAD_DN;
        if (nx < -0.45f) state.buttons |= PAD_BTN_DPAD_LF;
        else if (nx > 0.45f) state.buttons |= PAD_BTN_DPAD_RT;

        // Cross: Confirm / Select
        if (pad.Buttons & PSP_CTRL_CROSS)     state.buttons |= PAD_BTN_A;

        // Circle or Triangle: Back / Cancel
        if (pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE)) state.buttons |= PAD_BTN_B;

        // Start: Close menu / Resume
        if (pad.Buttons & PSP_CTRL_START)     state.buttons |= PAD_BTN_START;
    }
    else
    {
        // -------------------------------------------------------------
        // In-Game Playsim Mode:
        // -------------------------------------------------------------

        // 1. Analog Nub (center ~128) -> [-1.0f, +1.0f]
        float nx = ((float)pad.Lx - 128.0f) / 128.0f;
        float ny = ((float)pad.Ly - 128.0f) / 128.0f;

        // Deadzone filter for worn PSP analog nubs
        if (nx > -0.18f && nx < 0.18f) nx = 0.0f;
        if (ny > -0.18f && ny < 0.18f) ny = 0.0f;

        // Forward / Backward movement from Nub Y
        state.left_stick_y = -ny;

        // Strafe Modifier: Holding L-Trigger switches horizontal input from Turning to Strafing
        boolean strafe_mode = (pad.Buttons & PSP_CTRL_LTRIGGER) != 0;

        if (strafe_mode)
        {
            state.left_stick_x = nx;
            state.right_stick_x = 0.0f;
        }
        else
        {
            state.left_stick_x = 0.0f;
            state.right_stick_x = nx; // Smooth analog turning / aiming
        }

        // 2. D-Pad Movement (classic alternative to analog nub)
        if (pad.Buttons & PSP_CTRL_UP)    state.left_stick_y = 1.0f;
        if (pad.Buttons & PSP_CTRL_DOWN)  state.left_stick_y = -1.0f;
        if (pad.Buttons & PSP_CTRL_LEFT)
        {
            if (strafe_mode) state.left_stick_x = -1.0f;
            else             state.right_stick_x = -1.0f;
        }
        if (pad.Buttons & PSP_CTRL_RIGHT)
        {
            if (strafe_mode) state.left_stick_x = 1.0f;
            else             state.right_stick_x = 1.0f;
        }

        // 3. Actions & Weapons
        // Primary Fire: R-Trigger OR Cross button
        if (pad.Buttons & (PSP_CTRL_RTRIGGER | PSP_CTRL_CROSS))
        {
            state.right_trigger = 1.0f;
        }

        // Action / Use / Open Door / Switch: Square button
        if (pad.Buttons & PSP_CTRL_SQUARE)
        {
            state.buttons |= PAD_BTN_X;
        }

        // Weapon Cycling: Triangle = Next Weapon, Circle = Previous Weapon
        if (pad.Buttons & PSP_CTRL_TRIANGLE)
        {
            state.buttons |= PAD_BTN_RB;
        }
        if (pad.Buttons & PSP_CTRL_CIRCLE)
        {
            state.buttons |= PAD_BTN_LB;
        }

        // Automap: Select button
        if (pad.Buttons & PSP_CTRL_SELECT)
        {
            state.buttons |= PAD_BTN_BACK;
        }

        // Options / Pause: Start button
        if (pad.Buttons & PSP_CTRL_START)
        {
            state.buttons |= PAD_BTN_START;
        }
    }

    last_buttons = pad.Buttons;

    // Update unified gamepad subsystem for playsim ticcmd generation & menu navigation
    I_Gamepad_Update(&state);
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    // Draw into the off-screen buffer (uncached CPU pointer)
    int draw_buf_idx = 1 - current_buffer;
    uint32_t* dst_base = vram_cpu[draw_buf_idx];

    if (psp_scaling_mode == 0)
    {
        // Full Widescreen 480x272 Stretch (authentic edge-to-edge 16:9 on PSP display)
        for (int y = 0; y < PSP_SCREEN_HEIGHT; y++)
        {
            const byte* src_row = screens[0] + scale_lut_y[y] * SCREENWIDTH;
            uint32_t* dst_row = dst_base + y * PSP_BUF_STRIDE;

            for (int x = 0; x < PSP_SCREEN_WIDTH; x++)
            {
                dst_row[x] = psp_palette[src_row[scale_lut_x[x]]];
            }
        }
    }
    else
    {
        // 426x200 Pixel-Perfect Centered (with 27px horizontal and 36px vertical borders)
        const int off_x = (PSP_SCREEN_WIDTH - SCREENWIDTH) / 2;  // 27
        const int off_y = (PSP_SCREEN_HEIGHT - SCREENHEIGHT) / 2; // 36

        // Clear top and bottom letterbox borders
        memset(dst_base, 0, off_y * PSP_BUF_STRIDE * sizeof(uint32_t));
        memset(dst_base + (off_y + SCREENHEIGHT) * PSP_BUF_STRIDE, 0, (PSP_SCREEN_HEIGHT - (off_y + SCREENHEIGHT)) * PSP_BUF_STRIDE * sizeof(uint32_t));

        for (int y = 0; y < SCREENHEIGHT; y++)
        {
            const byte* src_row = screens[0] + y * SCREENWIDTH;
            uint32_t* dst_row = dst_base + (y + off_y) * PSP_BUF_STRIDE;

            // Clear left pillarbox
            for (int x = 0; x < off_x; x++) dst_row[x] = 0;

            // Copy 426 pixels directly
            for (int x = 0; x < SCREENWIDTH; x++)
            {
                dst_row[off_x + x] = psp_palette[src_row[x]];
            }

            // Clear right pillarbox
            for (int x = off_x + SCREENWIDTH; x < PSP_SCREEN_WIDTH; x++) dst_row[x] = 0;
        }
    }

    // Wait for vertical blanking to eliminate screen tearing
    sceDisplayWaitVblankStart();

    // Flip framebuffer using 0x04000000 physical VRAM address with NEXTFRAME sync
    sceDisplaySetFrameBuf(vram_display[draw_buf_idx], PSP_BUF_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    current_buffer = draw_buf_idx;

    // Frame refresh system:
    // Retain screens[0] in screens[4] for wipes, then clear screens[0] for the next frame
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
    while (count-- > 0)
    {
        sceDisplayWaitVblankStart();
    }
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

void I_SetResolution(int width, int height, boolean fullscreen)
{
    (void)width; (void)height; (void)fullscreen;
}

void I_SetResolutionIndex(int index, boolean fullscreen)
{
    (void)fullscreen;
    current_resolution_index = index;
    psp_scaling_mode = (index == 1) ? 1 : 0;
}

void I_ToggleFullscreen(void)
{
    psp_scaling_mode = 1 - psp_scaling_mode;
    current_resolution_index = psp_scaling_mode;
}

void I_GetResolution(int* width, int* height, boolean* fullscreen)
{
    if (width) *width = PSP_SCREEN_WIDTH;
    if (height) *height = PSP_SCREEN_HEIGHT;
    if (fullscreen) *fullscreen = true;
}

