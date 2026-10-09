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

// Uncached VRAM base address (0x44000000) avoids CPU cache flushing
#define PSP_VRAM_UNCACHED  ((uint32_t*)0x44000000)

// Double buffer framebuffers in eDRAM
// 512 * 272 * 4 = 557,056 bytes per framebuffer
static uint32_t* vram_buffer[2] = {
    PSP_VRAM_UNCACHED,
    PSP_VRAM_UNCACHED + (PSP_BUF_STRIDE * PSP_SCREEN_HEIGHT)
};
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

// Global wipe flag from i_video.h
boolean wipe_active = false;

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
    InitScalingLUT();

    // Set PSP Display mode to 480x272, 32-bit RGBA (8888)
    sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

    // Clear both framebuffers to solid black
    memset((void*)vram_buffer[0], 0, PSP_BUF_STRIDE * PSP_SCREEN_HEIGHT * sizeof(uint32_t));
    memset((void*)vram_buffer[1], 0, PSP_BUF_STRIDE * PSP_SCREEN_HEIGHT * sizeof(uint32_t));

    // Present the initial buffer
    sceDisplaySetFrameBuf((void*)vram_buffer[0], PSP_BUF_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
    current_buffer = 0;

    // Allocate DOOM refresh screens
    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    screens[4] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0] || !screens[4])
    {
        I_Error("I_InitGraphics: Failed to allocate DOOM frame buffers");
    }
    memset(screens[0], 0, SCREENWIDTH * SCREENHEIGHT);
    memset(screens[4], 0, SCREENWIDTH * SCREENHEIGHT);

    // Initialize controller sampling with analog nub enabled
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Initialize unified gamepad abstraction
    I_Gamepad_Init();
}

void I_ShutdownGraphics(void)
{
    if (screens[0]) { free(screens[0]); screens[0] = NULL; }
    if (screens[4]) { free(screens[4]); screens[4] = NULL; }
}

void I_SetPalette(byte* palette)
{
    // Convert 256 8-bit RGB triplets into 32-bit RGBA (0xAABBGGRR in little-endian Allegrex MIPS)
    for (int i = 0; i < 256; i++)
    {
        byte r = gammatable[usegamma][*palette++];
        byte g = gammatable[usegamma][*palette++];
        byte b = gammatable[usegamma][*palette++];

        psp_palette[i] = (0xFF << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }
}

void I_StartFrame(void)
{
}

void I_StartTic(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    // Populate common unified gamepad state
    gamepad_state_t state;
    memset(&state, 0, sizeof(state));
    state.connected = 1;

    // Analog Nub: normalize 0..255 (center ~128) to [-1.0f, +1.0f]
    float nx = ((float)pad.Lx - 128.0f) / 128.0f;
    float ny = ((float)pad.Ly - 128.0f) / 128.0f;

    // Deadzone filter for worn PSP analog nubs
    if (nx > -0.18f && nx < 0.18f) nx = 0.0f;
    if (ny > -0.18f && ny < 0.18f) ny = 0.0f;

    state.left_stick_x = nx;  // Turn / strafe
    state.left_stick_y = -ny; // Invert Y: pushing nub forward is -ny, mapped to +forward

    // Digital Buttons Mapping to Unified Gamepad
    if (pad.Buttons & PSP_CTRL_CROSS)     state.buttons |= PAD_BTN_A;     // Fire / Confirm
    if (pad.Buttons & PSP_CTRL_CIRCLE)    state.buttons |= PAD_BTN_B;     // Cancel / Sprint
    if (pad.Buttons & PSP_CTRL_SQUARE)    state.buttons |= PAD_BTN_X;     // Use / Open Door
    if (pad.Buttons & PSP_CTRL_TRIANGLE)  state.buttons |= PAD_BTN_Y;     // Automap
    if (pad.Buttons & PSP_CTRL_LTRIGGER)  state.buttons |= PAD_BTN_LB;    // Strafe Left / Prev Weapon
    if (pad.Buttons & PSP_CTRL_RTRIGGER)
    {
        state.buttons |= PAD_BTN_RB;    // Next Weapon
        state.right_trigger = 1.0f;     // Primary Fire via Right Shoulder
    }
    if (pad.Buttons & PSP_CTRL_START)     state.buttons |= PAD_BTN_START; // Menu / Pause
    if (pad.Buttons & PSP_CTRL_SELECT)    state.buttons |= PAD_BTN_BACK;  // Automap Toggle
    if (pad.Buttons & PSP_CTRL_UP)        state.buttons |= PAD_BTN_DPAD_UP;
    if (pad.Buttons & PSP_CTRL_DOWN)      state.buttons |= PAD_BTN_DPAD_DN;
    if (pad.Buttons & PSP_CTRL_LEFT)      state.buttons |= PAD_BTN_DPAD_LF;
    if (pad.Buttons & PSP_CTRL_RIGHT)     state.buttons |= PAD_BTN_DPAD_RT;

    // Dispatch button events to DOOM engine (handles menus & text navigation)
    uint32_t pressed = pad.Buttons & ~last_buttons;
    uint32_t released = last_buttons & ~pad.Buttons;

    struct { uint32_t psp_btn; int doom_key; } keymap[] = {
        { PSP_CTRL_START,    KEY_ESCAPE },
        { PSP_CTRL_SELECT,   KEY_TAB },
        { PSP_CTRL_UP,       KEY_UPARROW },
        { PSP_CTRL_DOWN,     KEY_DOWNARROW },
        { PSP_CTRL_LEFT,     KEY_LEFTARROW },
        { PSP_CTRL_RIGHT,    KEY_RIGHTARROW },
        { PSP_CTRL_CROSS,    KEY_ENTER },
        { PSP_CTRL_TRIANGLE, KEY_ESCAPE },
        { PSP_CTRL_SQUARE,   ' ' },
        { 0, 0 }
    };

    for (int i = 0; keymap[i].psp_btn != 0; i++)
    {
        if (pressed & keymap[i].psp_btn)
        {
            event_t ev;
            ev.type = ev_keydown;
            ev.data1 = keymap[i].doom_key;
            ev.data2 = ev.data3 = 0;
            D_PostEvent(&ev);
        }
        if (released & keymap[i].psp_btn)
        {
            event_t ev;
            ev.type = ev_keyup;
            ev.data1 = keymap[i].doom_key;
            ev.data2 = ev.data3 = 0;
            D_PostEvent(&ev);
        }
    }

    last_buttons = pad.Buttons;

    // Update unified gamepad subsystem for playsim ticcmd generation
    I_Gamepad_Update(&state);
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    // Draw into the off-screen buffer
    int draw_buf_idx = 1 - current_buffer;
    uint32_t* dst_base = vram_buffer[draw_buf_idx];

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

    // Flip framebuffer
    sceDisplaySetFrameBuf((void*)dst_base, PSP_BUF_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
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
