// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Native macOS Cocoa/CoreGraphics video and input driver.
//	Uses NSWindow, NSView, and CoreGraphics drawing with 4:3 aspect ratio.
//	Zero external dependencies!
//
//-----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
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

#define DEFAULT_SCALE 3

static NSWindow*      doom_window = nil;
static uint32_t       rgba_palette[256];
static uint32_t       rgba_framebuffer[SCREENWIDTH * SCREENHEIGHT];
static boolean        graphics_inited = false;
static boolean        mouse_captured = false;
static int            mouse_buttons = 0;
static int            accum_mouse_dx = 0;
static int            accum_mouse_dy = 0;
static boolean        shift_pressed = false;
static boolean        ctrl_pressed = false;
static boolean        alt_pressed = false;

static int TranslateMacKeyCode(unsigned short keyCode, NSString* chars)
{
    switch (keyCode)
    {
        case kVK_LeftArrow:  return KEY_LEFTARROW;
        case kVK_RightArrow: return KEY_RIGHTARROW;
        case kVK_DownArrow:  return KEY_DOWNARROW;
        case kVK_UpArrow:    return KEY_UPARROW;
        case kVK_Escape:     return KEY_ESCAPE;
        case kVK_Return:     return KEY_ENTER;
        case kVK_ANSI_KeypadEnter: return KEY_ENTER;
        case kVK_Tab:        return KEY_TAB;
        case kVK_Delete:     return KEY_BACKSPACE;
        case kVK_Space:      return ' ';
        case kVK_F1:         return KEY_F1;
        case kVK_F2:         return KEY_F2;
        case kVK_F3:         return KEY_F3;
        case kVK_F4:         return KEY_F4;
        case kVK_F5:         return KEY_F5;
        case kVK_F6:         return KEY_F6;
        case kVK_F7:         return KEY_F7;
        case kVK_F8:         return KEY_F8;
        case kVK_F9:         return KEY_F9;
        case kVK_F10:        return KEY_F10;
        case kVK_F11:        return KEY_F11;
        case kVK_F12:        return KEY_F12;
        case kVK_ANSI_Minus: return KEY_MINUS;
        case kVK_ANSI_Equal: return KEY_EQUALS;
        case kVK_ANSI_Comma: return ',';
        case kVK_ANSI_Period:return '.';
        default:
            if (chars && [chars length] > 0)
            {
                unichar c = [chars characterAtIndex:0];
                if (c >= 'A' && c <= 'Z')
                    return (int)(c - 'A' + 'a');
                if (c >= 'a' && c <= 'z')
                    return (int)c;
                if (c >= '0' && c <= '9')
                    return (int)c;
            }
            return 0;
    }
}

@interface DoomView : NSView
@end

@implementation DoomView

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    if (!ctx) return;

    // Fill letterbox border with black
    CGContextSetRGBFillColor(ctx, 0.0, 0.0, 0.0, 1.0);
    CGContextFillRect(ctx, NSRectToCGRect([self bounds]));

    // Build CGImage from 32-bit RGBA framebuffer
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL,
        rgba_framebuffer,
        SCREENWIDTH * SCREENHEIGHT * 4,
        NULL
    );

    CGImageRef image = CGImageCreate(
        SCREENWIDTH, SCREENHEIGHT,
        8, 32,
        SCREENWIDTH * 4,
        colorSpace,
        kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big,
        provider,
        NULL,
        false,
        kCGRenderingIntentDefault
    );

    if (image)
    {
        NSRect bounds = [self bounds];
        CGFloat aspect = 4.0 / 3.0; // 4:3 CRT aspect ratio
        CGFloat targetW = bounds.size.width;
        CGFloat targetH = targetW / aspect;
        if (targetH > bounds.size.height)
        {
            targetH = bounds.size.height;
            targetW = targetH * aspect;
        }
        CGFloat x = (bounds.size.width - targetW) / 2.0;
        CGFloat y = (bounds.size.height - targetH) / 2.0;
        CGRect dstRect = CGRectMake(x, y, targetW, targetH);

        CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
        CGContextDrawImage(ctx, dstRect, image);
        CGImageRelease(image);
    }

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
}

- (void)keyDown:(NSEvent *)event
{
    int doom_key = TranslateMacKeyCode([event keyCode], [event characters]);
    if (doom_key)
    {
        event_t ev;
        ev.type = ev_keydown;
        ev.data1 = doom_key;
        D_PostEvent(&ev);
    }
}

- (void)keyUp:(NSEvent *)event
{
    int doom_key = TranslateMacKeyCode([event keyCode], [event characters]);
    if (doom_key)
    {
        event_t ev;
        ev.type = ev_keyup;
        ev.data1 = doom_key;
        D_PostEvent(&ev);
    }
}

- (void)flagsChanged:(NSEvent *)event
{
    NSEventModifierFlags flags = [event modifierFlags];

    boolean shift_now = (flags & NSEventModifierFlagShift) != 0;
    if (shift_now != shift_pressed)
    {
        event_t ev;
        ev.type = shift_now ? ev_keydown : ev_keyup;
        ev.data1 = KEY_RSHIFT;
        D_PostEvent(&ev);
        shift_pressed = shift_now;
    }

    boolean ctrl_now = (flags & NSEventModifierFlagControl) != 0;
    if (ctrl_now != ctrl_pressed)
    {
        event_t ev;
        ev.type = ctrl_now ? ev_keydown : ev_keyup;
        ev.data1 = KEY_RCTRL;
        D_PostEvent(&ev);
        ctrl_pressed = ctrl_now;
    }

    boolean alt_now = (flags & NSEventModifierFlagOption) != 0;
    if (alt_now != alt_pressed)
    {
        event_t ev;
        ev.type = alt_now ? ev_keydown : ev_keyup;
        ev.data1 = KEY_RALT;
        D_PostEvent(&ev);
        alt_pressed = alt_now;
    }
}

- (void)mouseDown:(NSEvent *)event
{
    (void)event;
    if (!mouse_captured && !M_CheckParm("-nomouse"))
    {
        CGAssociateMouseAndMouseCursorPosition(false);
        [NSCursor hide];
        mouse_captured = true;
    }
    mouse_buttons |= 1;
}

- (void)mouseUp:(NSEvent *)event
{
    (void)event;
    mouse_buttons &= ~1;
}

- (void)rightMouseDown:(NSEvent *)event
{
    (void)event;
    mouse_buttons |= 2;
}

- (void)rightMouseUp:(NSEvent *)event
{
    (void)event;
    mouse_buttons &= ~2;
}

- (void)mouseMoved:(NSEvent *)event
{
    if (mouse_captured && !M_CheckParm("-nomouse"))
    {
        accum_mouse_dx += (int)[event deltaX];
        accum_mouse_dy -= (int)[event deltaY];
    }
}

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

@end

@interface DoomWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation DoomWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
    (void)sender;
    I_Quit();
    return YES;
}
- (void)windowDidResignKey:(NSNotification *)notification
{
    (void)notification;
    if (mouse_captured)
    {
        CGAssociateMouseAndMouseCursorPosition(true);
        [NSCursor unhide];
        mouse_captured = false;
    }
}
@end

static DoomView*           doom_view = nil;
static DoomWindowDelegate* doom_delegate = nil;

void I_InitGraphics(void)
{
    if (graphics_inited)
        return;

    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0])
        I_Error("I_InitGraphics: Failed to allocate framebuffer");

    int scale = DEFAULT_SCALE;
    int p = M_CheckParm("-scale");
    if (p && p < myargc - 1)
    {
        scale = atoi(myargv[p + 1]);
        if (scale < 1) scale = 1;
        if (scale > 6) scale = 6;
    }

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    // Setup main menu bar with Cmd+Q quit item
    NSMenu* menubar = [[NSMenu alloc] init];
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];

    NSMenu* appMenu = [[NSMenu alloc] init];
    NSMenuItem* quitMenuItem = [[NSMenuItem alloc]
        initWithTitle:@"Quit DOOM"
               action:@selector(terminate:)
        keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];

    NSRect frame = NSMakeRect(0, 0, SCREENWIDTH * scale, SCREENHEIGHT * scale);
    doom_window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable |
                            NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    [doom_window setTitle:@"HRGZDevEngine DOOM"];
    [doom_window setAcceptsMouseMovedEvents:YES];

    doom_delegate = [[DoomWindowDelegate alloc] init];
    [doom_window setDelegate:doom_delegate];

    doom_view = [[DoomView alloc] initWithFrame:frame];
    [doom_window setContentView:doom_view];
    [doom_window makeFirstResponder:doom_view];
    [doom_window center];
    [doom_window makeKeyAndOrderFront:nil];

    [NSApp activateIgnoringOtherApps:YES];

    if (!M_CheckParm("-nomouse"))
    {
        CGAssociateMouseAndMouseCursorPosition(false);
        [NSCursor hide];
        mouse_captured = true;
    }

    graphics_inited = true;
    printf("I_InitGraphics: Native macOS Cocoa window initialized (%dx%d, scale=%d)\n",
           SCREENWIDTH * scale, SCREENHEIGHT * scale, scale);
}

void I_ShutdownGraphics(void)
{
    if (!graphics_inited)
        return;

    if (mouse_captured)
    {
        CGAssociateMouseAndMouseCursorPosition(true);
        [NSCursor unhide];
        mouse_captured = false;
    }

    if (doom_window)
    {
        [doom_window close];
        doom_window = nil;
    }

    graphics_inited = false;
}

void I_SetPalette(byte* palette)
{
    for (int i = 0; i < 256; i++)
    {
        uint8_t r = gammatable[usegamma][*palette++];
        uint8_t g = gammatable[usegamma][*palette++];
        uint8_t b = gammatable[usegamma][*palette++];
        // On Little-Endian (macOS Apple Silicon & Intel), byte order in memory from lowest address is:
        // Byte 0: R, Byte 1: G, Byte 2: B, Byte 3: 0xFF
        rgba_palette[i] = (0xFF000000) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    if (!graphics_inited || !screens[0])
        return;

    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
    {
        rgba_framebuffer[i] = rgba_palette[screens[0][i]];
    }

    [doom_view setNeedsDisplay:YES];
    [doom_view displayIfNeeded];
}

void I_WaitVBL(int count)
{
    (void)count;
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
    @autoreleasepool {
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:nil
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]))
        {
            [NSApp sendEvent:event];
        }
    }

    // Post mouse movement and button states
    if (mouse_captured && !M_CheckParm("-nomouse"))
    {
        if (accum_mouse_dx != 0 || accum_mouse_dy != 0 || mouse_buttons != 0)
        {
            event_t ev;
            ev.type = ev_mouse;
            ev.data1 = mouse_buttons;
            ev.data2 = accum_mouse_dx * 2; // Sensitivity
            ev.data3 = 0; // Classic DOOM horizontal turning only
            D_PostEvent(&ev);
            accum_mouse_dx = 0;
            accum_mouse_dy = 0;
        }
    }
}

