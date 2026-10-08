// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Hardware-accelerated native Win32 video and input driver for Modern Windows.
//	Uses OpenGL hardware acceleration (WGL context, streaming texture upload,
//	hardware V-Sync, and 4:3 CRT aspect ratio letterboxing).
//	Gracefully falls back to GDI StretchDIBits if OpenGL is unavailable.
//	Zero external DLL dependencies!
//
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
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

#define DOOM_WINDOW_CLASS "HRGZDevEngine_DOOM"
#define DEFAULT_SCALE     3

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALEXTPROC)(int interval);

static HWND       hwnd_main = NULL;
static HDC        hdc_main = NULL;
static HGLRC      hglrc_main = NULL;
static GLuint     gl_texture_id = 0;
static boolean    opengl_enabled = false;

static BITMAPINFO bmi;
static uint32_t   argb_palette[256];
static uint32_t   argb_framebuffer[SCREENWIDTH * SCREENHEIGHT];
static boolean    window_active = false;
static boolean    mouse_captured = false;

static int win_width = SCREENWIDTH * DEFAULT_SCALE;
static int win_height = SCREENHEIGHT * DEFAULT_SCALE;

static int TranslateWinKey(WPARAM vk)
{
    switch (vk)
    {
        case VK_LEFT:     return KEY_LEFTARROW;
        case VK_RIGHT:    return KEY_RIGHTARROW;
        case VK_DOWN:     return KEY_DOWNARROW;
        case VK_UP:       return KEY_UPARROW;
        case VK_ESCAPE:   return KEY_ESCAPE;
        case VK_RETURN:   return KEY_ENTER;
        case VK_TAB:      return KEY_TAB;
        case VK_F1:       return KEY_F1;
        case VK_F2:       return KEY_F2;
        case VK_F3:       return KEY_F3;
        case VK_F4:       return KEY_F4;
        case VK_F5:       return KEY_F5;
        case VK_F6:       return KEY_F6;
        case VK_F7:       return KEY_F7;
        case VK_F8:       return KEY_F8;
        case VK_F9:       return KEY_F9;
        case VK_F10:      return KEY_F10;
        case VK_F11:      return KEY_F11;
        case VK_F12:      return KEY_F12;
        case VK_BACK:     return KEY_BACKSPACE;
        case VK_PAUSE:    return KEY_PAUSE;
        case VK_SHIFT:    return KEY_RSHIFT;
        case VK_CONTROL:  return KEY_RCTRL;
        case VK_MENU:     return KEY_RALT;
        case VK_SPACE:    return ' ';
        case VK_OEM_MINUS:return KEY_MINUS;
        case VK_OEM_PLUS: return KEY_EQUALS;
        case VK_OEM_COMMA:return ',';
        case VK_OEM_PERIOD:return '.';
        default:
            if (vk >= 'A' && vk <= 'Z')
                return (int)(vk - 'A' + 'a');
            if (vk >= '0' && vk <= '9')
                return (int)vk;
            return 0;
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    event_t ev;

    switch (msg)
    {
        case WM_ACTIVATE:
            window_active = (LOWORD(wParam) != WA_INACTIVE);
            if (window_active && !M_CheckParm("-nomouse"))
            {
                SetCapture(hWnd);
                ShowCursor(FALSE);
                mouse_captured = true;
            }
            else
            {
                ReleaseCapture();
                ShowCursor(TRUE);
                mouse_captured = false;
            }
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            int key = TranslateWinKey(wParam);
            if (key)
            {
                ev.type = ev_keydown;
                ev.data1 = key;
                ev.data2 = 0;
                ev.data3 = 0;
                D_PostEvent(&ev);
            }
            if (wParam == VK_F4 && (GetKeyState(VK_MENU) & 0x8000))
                I_Quit();
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            int key = TranslateWinKey(wParam);
            if (key)
            {
                ev.type = ev_keyup;
                ev.data1 = key;
                ev.data2 = 0;
                ev.data3 = 0;
                D_PostEvent(&ev);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        {
            int buttons = 0;
            if (wParam & MK_LBUTTON) buttons |= 1;
            if (wParam & MK_RBUTTON) buttons |= 2;
            if (wParam & MK_MBUTTON) buttons |= 4;

            ev.type = ev_mouse;
            ev.data1 = buttons;
            ev.data2 = 0;
            ev.data3 = 0;
            D_PostEvent(&ev);
            return 0;
        }

        case WM_SIZE:
            win_width = LOWORD(lParam);
            win_height = HIWORD(lParam);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            I_Quit();
            return 0;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void I_InitGraphics(void)
{
    // Allocate 320x200 software framebuffer
    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0])
        I_Error("I_InitGraphics: Could not allocate primary framebuffer");

#if defined(_WIN32)
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = DOOM_WINDOW_CLASS;

    RegisterClassExA(&wc);

    int scale = DEFAULT_SCALE;
    int p = M_CheckParm("-scale");
    if (p && p < myargc - 1)
        scale = atoi(myargv[p + 1]);
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;

    win_width = SCREENWIDTH * scale;
    win_height = (int)(SCREENHEIGHT * scale * 1.2);

    RECT rect = {0, 0, win_width, win_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (screen_w - (rect.right - rect.left)) / 2;
    int pos_y = (screen_h - (rect.bottom - rect.top)) / 2;

    hwnd_main = CreateWindowExA(
        0,
        DOOM_WINDOW_CLASS,
        "HRGZDevEngine DOOM (OpenGL)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        pos_x, pos_y,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd_main)
        I_Error("I_InitGraphics: CreateWindowEx failed");

    hdc_main = GetDC(hwnd_main);

    // Initialize OpenGL Hardware Acceleration
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(hdc_main, &pfd);
    if (format)
    {
        SetPixelFormat(hdc_main, format, &pfd);
        hglrc_main = wglCreateContext(hdc_main);
        if (hglrc_main)
        {
            wglMakeCurrent(hdc_main, hglrc_main);
            opengl_enabled = true;

            // Enable hardware V-Sync if extension supported
            PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
                (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
            if (wglSwapIntervalEXT)
            {
                wglSwapIntervalEXT(1);
            }

            // Create streaming texture for DOOM framebuffer
            glGenTextures(1, &gl_texture_id);
            glBindTexture(GL_TEXTURE_2D, gl_texture_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREENWIDTH, SCREENHEIGHT, 0,
                         GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);

            printf("I_InitGraphics: Modern Windows OpenGL hardware acceleration enabled (V-Sync ON)\n");
        }
    }

    if (!opengl_enabled)
    {
        // GDI Fallback Setup
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = SCREENWIDTH;
        bmi.bmiHeader.biHeight = -SCREENHEIGHT; // negative = top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        printf("I_InitGraphics: OpenGL unavailable, using GDI StretchDIBits fallback\n");
    }

    UpdateWindow(hwnd_main);
#endif
}

void I_ShutdownGraphics(void)
{
#if defined(_WIN32)
    if (mouse_captured)
    {
        ReleaseCapture();
        ShowCursor(TRUE);
        mouse_captured = false;
    }
    if (gl_texture_id)
    {
        glDeleteTextures(1, &gl_texture_id);
        gl_texture_id = 0;
    }
    if (hglrc_main)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hglrc_main);
        hglrc_main = NULL;
    }
    if (hdc_main && hwnd_main)
    {
        ReleaseDC(hwnd_main, hdc_main);
        hdc_main = NULL;
    }
    if (hwnd_main)
    {
        DestroyWindow(hwnd_main);
        hwnd_main = NULL;
    }
#endif
    if (screens[0])
    {
        free(screens[0]);
        screens[0] = NULL;
    }
}

void I_SetPalette(byte* palette)
{
    // Convert 8-bit RGB DOOM palette to 32-bit ARGB/BGRA
    for (int i = 0; i < 256; i++)
    {
        uint8_t r = gammatable[usegamma][*palette++];
        uint8_t g = gammatable[usegamma][*palette++];
        uint8_t b = gammatable[usegamma][*palette++];
        argb_palette[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    if (!screens[0] || !hdc_main)
        return;

    // Convert 8-bit paletted DOOM screen to 32-bit framebuffer
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
    {
        argb_framebuffer[i] = argb_palette[screens[0][i]];
    }

#if defined(_WIN32)
    RECT client_rect;
    GetClientRect(hwnd_main, &client_rect);
    int cw = client_rect.right - client_rect.left;
    int ch = client_rect.bottom - client_rect.top;

    int dst_w = cw;
#if SCREENWIDTH > 320
    int dst_h = (cw * 9) / 16;
    if (dst_h > ch)
    {
        dst_h = ch;
        dst_w = (ch * 16) / 9;
    }
#else
    int dst_h = (cw * 3) / 4;
    if (dst_h > ch)
    {
        dst_h = ch;
        dst_w = (ch * 4) / 3;
    }
#endif
    int dst_x = (cw - dst_w) / 2;
    int dst_y = (ch - dst_h) / 2;

    if (opengl_enabled && gl_texture_id)
    {
        // Update GPU texture
        glBindTexture(GL_TEXTURE_2D, gl_texture_id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREENWIDTH, SCREENHEIGHT,
                        GL_BGRA_EXT, GL_UNSIGNED_BYTE, argb_framebuffer);

        // Letterbox / Pillarbox clear
        glViewport(0, 0, cw, ch);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render 4:3 textured quad
        glViewport(dst_x, dst_y, dst_w, dst_h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, gl_texture_id);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 0.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 1.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        SwapBuffers(hdc_main);
    }
    else
    {
        // GDI Software Fallback
        SetStretchBltMode(hdc_main, COLORONCOLOR);
        StretchDIBits(
            hdc_main,
            dst_x, dst_y, dst_w, dst_h,
            0, 0, SCREENWIDTH, SCREENHEIGHT,
            argb_framebuffer,
            &bmi,
            DIB_RGB_COLORS,
            SRCCOPY
        );
    }
#endif

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
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    // Relative mouse motion
    if (mouse_captured && window_active && !M_CheckParm("-nomouse"))
    {
        RECT rect;
        GetWindowRect(hwnd_main, &rect);
        int center_x = rect.left + (rect.right - rect.left) / 2;
        int center_y = rect.top + (rect.bottom - rect.top) / 2;

        POINT cur;
        GetCursorPos(&cur);

        int dx = cur.x - center_x;
        int dy = cur.y - center_y;

        if (dx != 0 || dy != 0)
        {
            SetCursorPos(center_x, center_y);

            event_t ev;
            ev.type = ev_mouse;
            ev.data1 = 0;
            if (GetKeyState(VK_LBUTTON) & 0x8000) ev.data1 |= 1;
            if (GetKeyState(VK_RBUTTON) & 0x8000) ev.data1 |= 2;
            if (GetKeyState(VK_MBUTTON) & 0x8000) ev.data1 |= 4;
            ev.data2 = dx * 8;
            ev.data3 = -dy * 8;
            D_PostEvent(&ev);
        }
    }
#endif
}
