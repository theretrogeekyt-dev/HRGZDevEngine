// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Hardware-accelerated native macOS video and input driver using Apple Metal.
//	Uses CAMetalLayer, Metal Shading Language (MSL) pipeline, hardware V-Sync,
//	streaming texture upload, and integer/4:3 CRT aspect ratio letterboxing.
//	Gracefully falls back to CoreGraphics if Metal is unavailable.
//	Zero external dependencies!
//
//-----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <GameController/GameController.h>
#import <simd/simd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i_gamepad.h"

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "i_video_common.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#define DEFAULT_SCALE 3

static NSWindow*                   doom_window = nil;
static uint32_t                    rgba_palette[256];
static uint32_t                    rgba_framebuffer[SCREENWIDTH * SCREENHEIGHT];
static boolean                     graphics_inited = false;
static boolean                     mouse_captured = false;
static int                         mouse_buttons = 0;
static int                         accum_mouse_dx = 0;
static int                         accum_mouse_dy = 0;
static boolean                     shift_pressed = false;
static boolean                     ctrl_pressed = false;
static boolean                     alt_pressed = false;

// Metal Hardware Acceleration State
static id<MTLDevice>              metal_device = nil;
static id<MTLCommandQueue>        metal_queue = nil;
static id<MTLRenderPipelineState> metal_pipeline = nil;
static id<MTLTexture>             metal_texture = nil;
static id<MTLSamplerState>        metal_sampler = nil;
static CAMetalLayer*              metal_layer = nil;
static boolean                     metal_initialized = false;

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

static boolean InitMetalPipeline(void)
{
    metal_device = MTLCreateSystemDefaultDevice();
    if (!metal_device)
    {
        printf("I_InitGraphics: Metal unsupported on this system, falling back to CoreGraphics\n");
        return false;
    }

    metal_queue = [metal_device newCommandQueue];
    if (!metal_queue)
    {
        printf("I_InitGraphics: Failed to create Metal command queue\n");
        return false;
    }

    // Metal Shading Language: 2D Quad Vertex and Texture Sampling Fragment Shaders
    NSString* shaderSource = @""
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoord;\n"
    "};\n"
    "vertex VertexOut vertexShader(uint vertexID [[vertex_id]],\n"
    "                             constant float2 *positions [[buffer(0)]],\n"
    "                             constant float2 *texCoords [[buffer(1)]]) {\n"
    "    VertexOut out;\n"
    "    out.position = float4(positions[vertexID], 0.0, 1.0);\n"
    "    out.texCoord = texCoords[vertexID];\n"
    "    return out;\n"
    "}\n"
    "fragment float4 fragmentShader(VertexOut in [[stage_in]],\n"
    "                              texture2d<float> colorTexture [[texture(0)]],\n"
    "                              sampler textureSampler [[sampler(0)]]) {\n"
    "    return colorTexture.sample(textureSampler, in.texCoord);\n"
    "}\n";

    NSError* error = nil;
    id<MTLLibrary> library = [metal_device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library)
    {
        printf("I_InitGraphics: Metal shader compilation failed: %s\n",
               [[error localizedDescription] UTF8String]);
        return false;
    }

    id<MTLFunction> vertFunc = [library newFunctionWithName:@"vertexShader"];
    id<MTLFunction> fragFunc = [library newFunctionWithName:@"fragmentShader"];

    MTLRenderPipelineDescriptor* pDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pDesc.vertexFunction = vertFunc;
    pDesc.fragmentFunction = fragFunc;
    pDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    metal_pipeline = [metal_device newRenderPipelineStateWithDescriptor:pDesc error:&error];
    if (!metal_pipeline)
    {
        printf("I_InitGraphics: Failed to create Metal render pipeline: %s\n",
               [[error localizedDescription] UTF8String]);
        return false;
    }

    // Allocate 320x200 32-bit BGRA texture
    MTLTextureDescriptor* tDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:SCREENWIDTH
                                    height:SCREENHEIGHT
                                 mipmapped:NO];
    tDesc.usage = MTLTextureUsageShaderRead;
    tDesc.storageMode = MTLStorageModeShared;
    metal_texture = [metal_device newTextureWithDescriptor:tDesc];
    if (!metal_texture)
    {
        printf("I_InitGraphics: Failed to allocate Metal texture\n");
        return false;
    }

    // Nearest-neighbor sampler for authentic pixel-crisp rendering
    MTLSamplerDescriptor* sDesc = [[MTLSamplerDescriptor alloc] init];
    sDesc.minFilter = MTLSamplerMinMagFilterNearest;
    sDesc.magFilter = MTLSamplerMinMagFilterNearest;
    sDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    metal_sampler = [metal_device newSamplerStateWithDescriptor:sDesc];

    metal_initialized = true;
    printf("I_InitGraphics: Apple Metal hardware acceleration enabled (GPU: %s, V-Sync ON)\n",
           [[metal_device name] UTF8String]);
    return true;
}

@interface DoomView : NSView
@end

@implementation DoomView

- (CALayer *)makeBackingLayer
{
    if (metal_initialized && metal_device)
    {
        CAMetalLayer *layer = [CAMetalLayer layer];
        layer.device = metal_device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
        layer.displaySyncEnabled = YES; // Hardware V-Sync (60Hz / 120Hz ProMotion)
        metal_layer = layer;
        return layer;
    }
    return [super makeBackingLayer];
}

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        [self setWantsLayer:YES];
    }
    return self;
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    if (metal_layer)
    {
        CGFloat scale = self.window ? [self.window backingScaleFactor] : 2.0;
        metal_layer.contentsScale = scale;
        metal_layer.drawableSize = CGSizeMake(newSize.width * scale, newSize.height * scale);
    }
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    if (self.window && metal_layer)
    {
        CGFloat scale = [self.window backingScaleFactor];
        metal_layer.contentsScale = scale;
        metal_layer.drawableSize = CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);
    }
}

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
    if (metal_initialized)
    {
        // Handled via CAMetalLayer presentation
        return;
    }

    // CoreGraphics Software Fallback
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    if (!ctx) return;

    CGContextSetRGBFillColor(ctx, 0.0, 0.0, 0.0, 1.0);
    CGContextFillRect(ctx, NSRectToCGRect([self bounds]));

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
        CGFloat aspect = (CGFloat)SCREENWIDTH / ((CGFloat)SCREENHEIGHT * 1.2);
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
- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    (void)notification;
    display_fullscreen = true;
    if (doom_window && metal_layer)
    {
        NSSize sz = [doom_window contentView].bounds.size;
        CGFloat scale = [doom_window backingScaleFactor];
        metal_layer.contentsScale = scale;
        metal_layer.drawableSize = CGSizeMake(sz.width * scale, sz.height * scale);
    }
}
- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    (void)notification;
    display_fullscreen = false;
    if (doom_window && metal_layer)
    {
        NSSize sz = [doom_window contentView].bounds.size;
        CGFloat scale = [doom_window backingScaleFactor];
        metal_layer.contentsScale = scale;
        metal_layer.drawableSize = CGSizeMake(sz.width * scale, sz.height * scale);
    }
}
- (void)windowDidResize:(NSNotification *)notification
{
    (void)notification;
    if (doom_window && metal_layer)
    {
        NSSize sz = [doom_window contentView].bounds.size;
        display_width = (int)sz.width;
        display_height = (int)sz.height;
        CGFloat scale = [doom_window backingScaleFactor];
        metal_layer.contentsScale = scale;
        metal_layer.drawableSize = CGSizeMake(sz.width * scale, sz.height * scale);
    }
}
@end

static DoomView*           doom_view = nil;
static DoomWindowDelegate* doom_delegate = nil;

void I_SetResolution(int width, int height, boolean fullscreen)
{
    if (!doom_window)
        return;

    display_width = width;
    display_height = height;

    boolean is_currently_fs = (([doom_window styleMask] & NSWindowStyleMaskFullScreen) != 0);

    if (fullscreen)
    {
        if (!is_currently_fs)
        {
            [doom_window toggleFullScreen:nil];
        }
    }
    else
    {
        if (is_currently_fs)
        {
            [doom_window toggleFullScreen:nil];
        }

        [doom_window setContentSize:NSMakeSize(width, height)];
        [doom_window center];

        if (metal_layer)
        {
            CGFloat scale = [doom_window backingScaleFactor];
            metal_layer.contentsScale = scale;
            metal_layer.drawableSize = CGSizeMake(width * scale, height * scale);
        }
    }

    display_fullscreen = fullscreen;
    current_resolution_index = I_FindResolutionIndex(width, height);
    printf("I_SetResolution: %dx%d (fullscreen: %s)\n", width, height, fullscreen ? "YES" : "NO");
}

void I_ToggleFullscreen(void)
{
    if (!doom_window)
        return;

    [doom_window toggleFullScreen:nil];
}

void I_InitGraphics(void)
{
    if (graphics_inited)
        return;

    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0])
        I_Error("I_InitGraphics: Failed to allocate framebuffer");

    int win_w = 1280;
    int win_h = 720;
    boolean init_fullscreen = false;
    I_ParseDisplayParams(&win_w, &win_h, &init_fullscreen);

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

    // Initialize Metal pipeline before creating window/view
    InitMetalPipeline();

    NSRect frame = NSMakeRect(0, 0, win_w, win_h);
    doom_window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable |
                            NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    [doom_window setTitle:@"HRGZDevEngine DOOM (Apple Metal)"];
    [doom_window setAcceptsMouseMovedEvents:YES];
    [doom_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

    doom_delegate = [[DoomWindowDelegate alloc] init];
    [doom_window setDelegate:doom_delegate];

    doom_view = [[DoomView alloc] initWithFrame:frame];
    [doom_window setContentView:doom_view];
    [doom_window makeFirstResponder:doom_view];
    [doom_window center];
    [doom_window makeKeyAndOrderFront:nil];

    if (init_fullscreen)
    {
        [doom_window toggleFullScreen:nil];
    }

    [NSApp activateIgnoringOtherApps:YES];

    if (!M_CheckParm("-nomouse"))
    {
        CGAssociateMouseAndMouseCursorPosition(false);
        [NSCursor hide];
        mouse_captured = true;
    }

    I_Gamepad_Init();

    graphics_inited = true;
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

    metal_pipeline = nil;
    metal_texture = nil;
    metal_sampler = nil;
    metal_queue = nil;
    metal_device = nil;
    metal_layer = nil;
    metal_initialized = false;

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
        // Memory layout for MTLPixelFormatBGRA8Unorm on Little-Endian:
        // Byte 0 = B, Byte 1 = G, Byte 2 = R, Byte 3 = 0xFF
        rgba_palette[i] = (0xFF000000) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void I_UpdateNoBlit(void)
{
}

static void RenderMetalFrame(void)
{
    if (!metal_layer || !metal_pipeline || !metal_texture || !metal_queue)
        return;

    @autoreleasepool {
        id<CAMetalDrawable> drawable = [metal_layer nextDrawable];
        if (!drawable)
            return;

        // Upload updated 320x200 DOOM framebuffer to GPU texture
        [metal_texture replaceRegion:MTLRegionMake2D(0, 0, SCREENWIDTH, SCREENHEIGHT)
                         mipmapLevel:0
                           withBytes:rgba_framebuffer
                         bytesPerRow:SCREENWIDTH * sizeof(uint32_t)];

        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0); // Black letterbox
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLCommandBuffer> cmd = [metal_queue commandBuffer];
        id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];

        CGSize drawSize = metal_layer.drawableSize;
        CGFloat w = drawSize.width;
        CGFloat h = drawSize.height;
        if (w > 0 && h > 0)
        {
            // Maintain authentic display aspect ratio with letterboxing/pillarboxing
            CGFloat aspect = (CGFloat)SCREENWIDTH / ((CGFloat)SCREENHEIGHT * 1.2);
            CGFloat targetW = w;
            CGFloat targetH = targetW / aspect;
            if (targetH > h)
            {
                targetH = h;
                targetW = targetH * aspect;
            }
            float normX = (float)(targetW / w);
            float normY = (float)(targetH / h);

            simd_float2 positions[6] = {
                { -normX, -normY },
                {  normX, -normY },
                { -normX,  normY },
                { -normX,  normY },
                {  normX, -normY },
                {  normX,  normY }
            };
            simd_float2 texCoords[6] = {
                { 0.0f, 1.0f },
                { 1.0f, 1.0f },
                { 0.0f, 0.0f },
                { 0.0f, 0.0f },
                { 1.0f, 1.0f },
                { 1.0f, 0.0f }
            };

            [enc setRenderPipelineState:metal_pipeline];
            [enc setVertexBytes:positions length:sizeof(positions) atIndex:0];
            [enc setVertexBytes:texCoords length:sizeof(texCoords) atIndex:1];
            [enc setFragmentTexture:metal_texture atIndex:0];
            [enc setFragmentSamplerState:metal_sampler atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        }

        [enc endEncoding];
        [cmd presentDrawable:drawable];
        [cmd commit];
    }
}

void I_FinishUpdate(void)
{
    if (!graphics_inited || !screens[0])
        return;

    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
    {
        rgba_framebuffer[i] = rgba_palette[screens[0][i]];
    }

    if (metal_initialized)
    {
        RenderMetalFrame();
    }
    else
    {
        [doom_view setNeedsDisplay:YES];
        [doom_view displayIfNeeded];
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

    // Poll Apple GameController (DualShock 4, DualSense PS5, Xbox Series/One/Elite)
    @autoreleasepool {
        GCController* controller = [GCController current];
        if (!controller)
        {
            NSArray<GCController*>* all_controllers = [GCController controllers];
            if (all_controllers.count > 0)
            {
                controller = all_controllers.firstObject;
            }
        }

        if (controller && controller.extendedGamepad)
        {
            GCExtendedGamepad* pad = controller.extendedGamepad;
            gamepad_state_t pad_state;
            memset(&pad_state, 0, sizeof(pad_state));
            pad_state.connected = 1;

            pad_state.left_stick_x  = pad.leftThumbstick.xAxis.value;
            pad_state.left_stick_y  = pad.leftThumbstick.yAxis.value;
            pad_state.right_stick_x = pad.rightThumbstick.xAxis.value;
            pad_state.right_stick_y = pad.rightThumbstick.yAxis.value;

            pad_state.left_trigger  = pad.leftTrigger.value;
            pad_state.right_trigger = pad.rightTrigger.value;

            if (pad.buttonA.isPressed) pad_state.buttons |= PAD_BTN_A;
            if (pad.buttonB.isPressed) pad_state.buttons |= PAD_BTN_B;
            if (pad.buttonX.isPressed) pad_state.buttons |= PAD_BTN_X;
            if (pad.buttonY.isPressed) pad_state.buttons |= PAD_BTN_Y;

            if (pad.leftShoulder.isPressed)  pad_state.buttons |= PAD_BTN_LB;
            if (pad.rightShoulder.isPressed) pad_state.buttons |= PAD_BTN_RB;

            if (pad.buttonMenu.isPressed) pad_state.buttons |= PAD_BTN_START;
            if (pad.buttonOptions && pad.buttonOptions.isPressed) pad_state.buttons |= PAD_BTN_BACK;

            if (pad.leftThumbstickButton && pad.leftThumbstickButton.isPressed)
                pad_state.buttons |= PAD_BTN_L3;
            if (pad.rightThumbstickButton && pad.rightThumbstickButton.isPressed)
                pad_state.buttons |= PAD_BTN_R3;

            if (pad.dpad.up.isPressed)    pad_state.buttons |= PAD_BTN_DPAD_UP;
            if (pad.dpad.down.isPressed)  pad_state.buttons |= PAD_BTN_DPAD_DN;
            if (pad.dpad.left.isPressed)  pad_state.buttons |= PAD_BTN_DPAD_LF;
            if (pad.dpad.right.isPressed) pad_state.buttons |= PAD_BTN_DPAD_RT;

            I_Gamepad_Update(&pad_state);
        }
        else
        {
            I_Gamepad_Update(NULL);
        }
    }
}
