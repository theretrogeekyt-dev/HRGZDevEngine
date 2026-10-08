// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DOOM hardware video driver for MS-DOS (VGA Mode 13h: 320x200 256 colors).
//	Supports DJGPP (CWSDPMI) and OpenWatcom DOS extenders (DOS/4GW).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__DJGPP__)
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <conio.h>
#elif defined(__WATCOMC__)
#include <i86.h>
#include <conio.h>
#define inportb inp
#define outportb outp
#endif

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

// VGA Mode 13h parameters
#define VGA_WIDTH       320
#define VGA_HEIGHT      200
#define VGA_SCREEN_SIZE (VGA_WIDTH * VGA_HEIGHT)
#define VGA_MEM_BASE    0xA0000

// VGA Port I/O addresses
#define VGA_DAC_INDEX   0x3C8
#define VGA_DAC_DATA    0x3C9
#define VGA_INPUT_STAT  0x3DA

static boolean graphics_started = false;

// Raw keyboard interrupt tracking (IRQ 1 / int 9h)
#if defined(__DJGPP__)
static _go32_dpmi_seginfo old_kbd_handler;
static _go32_dpmi_seginfo new_kbd_handler;
static volatile uint8_t key_states[128];
static boolean kbd_hooked = false;

// Scan code to DOOM key translation table
static const int scancode_to_doom[128] = {
    0,            KEY_ESCAPE,   '1',          '2',          // 0x00 - 0x03
    '3',          '4',          '5',          '6',          // 0x04 - 0x07
    '7',          '8',          '9',          '0',          // 0x08 - 0x0B
    KEY_MINUS,    KEY_EQUALS,   KEY_BACKSPACE,KEY_TAB,      // 0x0C - 0x0F
    'q',          'w',          'e',          'r',          // 0x10 - 0x13
    't',          'y',          'u',          'i',          // 0x14 - 0x17
    'o',          'p',          '[',          ']',          // 0x18 - 0x1B
    KEY_ENTER,    KEY_RCTRL,    'a',          's',          // 0x1C - 0x1F
    'd',          'f',          'g',          'h',          // 0x20 - 0x23
    'j',          'k',          'l',          ';',          // 0x24 - 0x27
    '\'',         '`',          KEY_RSHIFT,   '\\',         // 0x28 - 0x2B
    'z',          'x',          'c',          'v',          // 0x2C - 0x2F
    'b',          'n',          'm',          ',',          // 0x30 - 0x33
    '.',          '/',          KEY_RSHIFT,   '*',          // 0x34 - 0x37
    KEY_RALT,     ' ',          0,            KEY_F1,       // 0x38 - 0x3B
    KEY_F2,       KEY_F3,       KEY_F4,       KEY_F5,       // 0x3C - 0x3F
    KEY_F6,       KEY_F7,       KEY_F8,       KEY_F9,       // 0x40 - 0x43
    KEY_F10,      KEY_PAUSE,    0,            KEY_UPARROW,  // 0x44 - 0x47
    KEY_UPARROW,  0,            KEY_MINUS,    KEY_LEFTARROW,// 0x48 - 0x4B
    0,            KEY_RIGHTARROW,KEY_EQUALS,  0,            // 0x4C - 0x4F
    KEY_DOWNARROW,0,            0,            0,            // 0x50 - 0x53
    0,            0,            0,            KEY_F11,      // 0x54 - 0x57
    KEY_F12,      0,            0,            0             // 0x58 - 0x5B
};

static void dos_keyboard_isr(void)
{
    uint8_t scancode = inportb(0x60);
    outportb(0x20, 0x20); // Send EOI to PIC

    uint8_t key = scancode & 0x7F;
    if (scancode & 0x80)
        key_states[key] = 0; // Released
    else
        key_states[key] = 1; // Pressed
}
static void dos_keyboard_isr_end(void) {}

static void dos_hook_keyboard(void)
{
    if (kbd_hooked) return;
    memset((void*)key_states, 0, sizeof(key_states));

    _go32_dpmi_lock_data((void*)key_states, sizeof(key_states));
    _go32_dpmi_lock_code((void*)(uintptr_t)dos_keyboard_isr,
        (unsigned long)((uintptr_t)dos_keyboard_isr_end - (uintptr_t)dos_keyboard_isr));

    _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd_handler);
    new_kbd_handler.pm_offset = (unsigned long)(uintptr_t)dos_keyboard_isr;
    new_kbd_handler.pm_selector = _go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&new_kbd_handler);
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &new_kbd_handler);
    kbd_hooked = true;
}

static void dos_unhook_keyboard(void)
{
    if (!kbd_hooked) return;
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd_handler);
    _go32_dpmi_free_iret_wrapper(&new_kbd_handler);
    kbd_hooked = false;
}
#endif

// Set video mode using BIOS int 10h
static void set_vga_mode(uint16_t mode)
{
#if defined(__DJGPP__)
    __dpmi_regs r;
    memset(&r, 0, sizeof(r));
    r.x.ax = mode;
    __dpmi_int(0x10, &r);
#elif defined(__WATCOMC__)
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = mode;
    int386(0x10, &r, &r);
#endif
}

void I_InitGraphics(void)
{
    if (graphics_started)
        return;

    // Allocate 320x200 software framebuffers (screens[0] is active frame)
    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0])
        I_Error("I_InitGraphics: Could not allocate primary screen buffer");

    // Mode 13h: 320x200 256 colors
    set_vga_mode(0x0013);
    graphics_started = true;

#if defined(__DJGPP__)
    dos_hook_keyboard();
#endif
}

void I_ShutdownGraphics(void)
{
    if (!graphics_started)
        return;

#if defined(__DJGPP__)
    dos_unhook_keyboard();
#endif

    // Restore text mode 80x25 (mode 3)
    set_vga_mode(0x0003);
    graphics_started = false;
}

void I_SetPalette(byte* palette)
{
#if defined(__DJGPP__) || defined(__WATCOMC__)
    outportb(VGA_DAC_INDEX, 0);
    // VGA DAC expects 6-bit color values (0..63)
    for (int i = 0; i < 256 * 3; i++)
    {
        outportb(VGA_DAC_DATA, palette[i] >> 2);
    }
#endif
}

void I_UpdateNoBlit(void)
{
    // Empty on double buffered displays
}

void I_FinishUpdate(void)
{
    if (!graphics_started || !screens[0])
        return;

#if defined(__DJGPP__)
    dosmemput(screens[0], VGA_SCREEN_SIZE, VGA_MEM_BASE);
#elif defined(__WATCOMC__)
    memcpy((void*)VGA_MEM_BASE, screens[0], VGA_SCREEN_SIZE);
#endif
}

void I_WaitVBL(int count)
{
#if defined(__DJGPP__) || defined(__WATCOMC__)
    while (count-- > 0)
    {
        while ((inportb(VGA_INPUT_STAT) & 8) != 0); // Wait for current retrace to end
        while ((inportb(VGA_INPUT_STAT) & 8) == 0); // Wait for next retrace to start
    }
#endif
}

void I_ReadScreen(byte* scr)
{
    if (screens[0] && scr)
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
#if defined(__DJGPP__)
    static uint8_t prev_key_states[128];
    event_t ev;

    for (int i = 1; i < 128; i++)
    {
        if (key_states[i] != prev_key_states[i])
        {
            prev_key_states[i] = key_states[i];
            int doom_key = scancode_to_doom[i];
            if (doom_key != 0)
            {
                ev.type = key_states[i] ? ev_keydown : ev_keyup;
                ev.data1 = doom_key;
                ev.data2 = 0;
                ev.data3 = 0;
                D_PostEvent(&ev);
            }
        }
    }
#endif
}

