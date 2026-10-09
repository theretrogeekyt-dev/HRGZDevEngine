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
#include <mmsystem.h>
#include <GL/gl.h>
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
#include "i_game_config.h"

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
static int        win_width = SCREENWIDTH * DEFAULT_SCALE;
static int        win_height = SCREENHEIGHT * DEFAULT_SCALE;

static BITMAPINFO bmi;
static uint32_t   argb_palette[256];
static uint32_t   argb_framebuffer[SCREENWIDTH * SCREENHEIGHT];
static boolean    window_active = false;
static boolean    mouse_captured = false;

// XInput dynamic structures
typedef struct {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD_DOOM;

typedef struct {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_DOOM Gamepad;
} XINPUT_STATE_DOOM;

typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE_DOOM* pState);

static HMODULE            h_xinput = NULL;
static PFN_XInputGetState pfn_XInputGetState = NULL;

static void Win32_InitGamepad(void)
{
    I_Gamepad_Init();

    const char* dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll", NULL };
    for (int i = 0; dlls[i] != NULL; i++)
    {
        h_xinput = LoadLibraryA(dlls[i]);
        if (h_xinput)
        {
            pfn_XInputGetState = (PFN_XInputGetState)GetProcAddress(h_xinput, "XInputGetState");
            if (pfn_XInputGetState)
            {
                printf("I_InitGraphics: Loaded %s for Xbox controller support\n", dlls[i]);
                break;
            }
            FreeLibrary(h_xinput);
            h_xinput = NULL;
        }
    }
}

static void Win32_PollGamepad(void)
{
    gamepad_state_t state;
    memset(&state, 0, sizeof(state));
    int found = 0;

    // 1. Check XInput (Xbox controllers, or PlayStation via Steam/DS4Windows)
    if (pfn_XInputGetState)
    {
        for (DWORD user = 0; user < 4; user++)
        {
            XINPUT_STATE_DOOM xs;
            if (pfn_XInputGetState(user, &xs) == 0)
            {
                found = 1;
                state.connected = 1;

                state.left_stick_x  = (float)xs.Gamepad.sThumbLX / (xs.Gamepad.sThumbLX < 0 ? 32768.0f : 32767.0f);
                state.left_stick_y  = (float)xs.Gamepad.sThumbLY / (xs.Gamepad.sThumbLY < 0 ? 32768.0f : 32767.0f);
                state.right_stick_x = (float)xs.Gamepad.sThumbRX / (xs.Gamepad.sThumbRX < 0 ? 32768.0f : 32767.0f);
                state.right_stick_y = (float)xs.Gamepad.sThumbRY / (xs.Gamepad.sThumbRY < 0 ? 32768.0f : 32767.0f);

                state.left_trigger  = (float)xs.Gamepad.bLeftTrigger / 255.0f;
                state.right_trigger = (float)xs.Gamepad.bRightTrigger / 255.0f;

                WORD b = xs.Gamepad.wButtons;
                if (b & 0x1000) state.buttons |= PAD_BTN_A;
                if (b & 0x2000) state.buttons |= PAD_BTN_B;
                if (b & 0x4000) state.buttons |= PAD_BTN_X;
                if (b & 0x8000) state.buttons |= PAD_BTN_Y;

                if (b & 0x0100) state.buttons |= PAD_BTN_LB;
                if (b & 0x0200) state.buttons |= PAD_BTN_RB;

                if (b & 0x0010) state.buttons |= PAD_BTN_START;
                if (b & 0x0020) state.buttons |= PAD_BTN_BACK;

                if (b & 0x0040) state.buttons |= PAD_BTN_L3;
                if (b & 0x0080) state.buttons |= PAD_BTN_R3;

                if (b & 0x0001) state.buttons |= PAD_BTN_DPAD_UP;
                if (b & 0x0002) state.buttons |= PAD_BTN_DPAD_DN;
                if (b & 0x0004) state.buttons |= PAD_BTN_DPAD_LF;
                if (b & 0x0008) state.buttons |= PAD_BTN_DPAD_RT;

                break;
            }
        }
    }

    // 2. DirectInput / WinMM fallback (Native PlayStation DualShock 4 / DualSense)
    if (!found)
    {
        JOYINFOEX jie;
        memset(&jie, 0, sizeof(jie));
        jie.dwSize = sizeof(JOYINFOEX);
        jie.dwFlags = JOY_RETURNALL;

        for (UINT j = JOYSTICKID1; j <= JOYSTICKID2; j++)
        {
            if (joyGetPosEx(j, &jie) == JOYERR_NOERROR)
            {
                found = 1;
                state.connected = 1;

                state.left_stick_x  = ((float)jie.dwXpos - 32767.5f) / 32767.5f;
                state.left_stick_y  = -((float)jie.dwYpos - 32767.5f) / 32767.5f;
                state.right_stick_x = ((float)jie.dwZpos - 32767.5f) / 32767.5f;
                state.right_stick_y = -((float)jie.dwRpos - 32767.5f) / 32767.5f;

                state.left_trigger  = ((float)jie.dwUpos) / 65535.0f;
                state.right_trigger = ((float)jie.dwVpos) / 65535.0f;

                DWORD btns = jie.dwButtons;
                if (btns & (1 << 0)) state.buttons |= PAD_BTN_X;        // PS Square
                if (btns & (1 << 1)) state.buttons |= PAD_BTN_A;        // PS Cross
                if (btns & (1 << 2)) state.buttons |= PAD_BTN_B;        // PS Circle
                if (btns & (1 << 3)) state.buttons |= PAD_BTN_Y;        // PS Triangle
                if (btns & (1 << 4)) state.buttons |= PAD_BTN_LB;       // PS L1
                if (btns & (1 << 5)) state.buttons |= PAD_BTN_RB;       // PS R1
                if (btns & (1 << 6)) state.left_trigger = 1.0f;         // PS L2
                if (btns & (1 << 7)) state.right_trigger = 1.0f;        // PS R2
                if (btns & (1 << 8)) state.buttons |= PAD_BTN_BACK;     // PS Share
                if (btns & (1 << 9)) state.buttons |= PAD_BTN_START;    // PS Options
                if (btns & (1 << 10)) state.buttons |= PAD_BTN_L3;      // PS L3
                if (btns & (1 << 11)) state.buttons |= PAD_BTN_R3;      // PS R3

                if (jie.dwPOV != 0xFFFF)
                {
                    if (jie.dwPOV == 0 || jie.dwPOV == 31500 || jie.dwPOV == 4500)
                        state.buttons |= PAD_BTN_DPAD_UP;
                    if (jie.dwPOV == 18000 || jie.dwPOV == 13500 || jie.dwPOV == 22500)
                        state.buttons |= PAD_BTN_DPAD_DN;
                    if (jie.dwPOV == 27000 || jie.dwPOV == 22500 || jie.dwPOV == 31500)
                        state.buttons |= PAD_BTN_DPAD_LF;
                    if (jie.dwPOV == 9000 || jie.dwPOV == 4500 || jie.dwPOV == 13500)
                        state.buttons |= PAD_BTN_DPAD_RT;
                }

                break;
            }
        }
    }

    I_Gamepad_Update(found ? &state : NULL);
}

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
                event_t ev_rel;
                ev_rel.type = ev_mouse;
                ev_rel.data1 = 0;
                ev_rel.data2 = 0;
                ev_rel.data3 = 0;
                D_PostEvent(&ev_rel);
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

static RECT windowed_rect = { 0, 0, 1280, 720 };

void I_SetResolution(int width, int height, boolean fullscreen)
{
    if (!hwnd_main)
        return;

    display_width = width;
    display_height = height;
    display_fullscreen = fullscreen;
    current_resolution_index = I_FindResolutionIndex(width, height);

    if (fullscreen)
    {
        GetWindowRect(hwnd_main, &windowed_rect);
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongPtr(hwnd_main, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd_main, HWND_TOP, 0, 0, screen_w, screen_h,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        RECT rect = { 0, 0, width, height };
        AdjustWindowRect(&rect, style, FALSE);
        int win_w = rect.right - rect.left;
        int win_h = rect.bottom - rect.top;

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        int pos_x = (screen_w - win_w) / 2;
        int pos_y = (screen_h - win_h) / 2;
        if (pos_x < 0) pos_x = 0;
        if (pos_y < 0) pos_y = 0;

        SetWindowLongPtr(hwnd_main, GWL_STYLE, style);
        SetWindowPos(hwnd_main, HWND_NOTOPMOST, pos_x, pos_y, win_w, win_h,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    printf("I_SetResolution: %dx%d (fullscreen: %s)\n", width, height, fullscreen ? "YES" : "NO");
}

void I_ToggleFullscreen(void)
{
    I_SetResolution(display_width, display_height, !display_fullscreen);
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

    int win_w = 1280;
    int win_h = 720;
    boolean init_fullscreen = false;
    I_ParseDisplayParams(&win_w, &win_h, &init_fullscreen);

    win_width = win_w;
    win_height = win_h;

    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    int pos_x = 0, pos_y = 0;
    int final_w = win_w, final_h = win_h;

    if (init_fullscreen)
    {
        style = WS_POPUP | WS_VISIBLE;
        final_w = GetSystemMetrics(SM_CXSCREEN);
        final_h = GetSystemMetrics(SM_CYSCREEN);
        pos_x = 0;
        pos_y = 0;
    }
    else
    {
        RECT rect = {0, 0, win_w, win_h};
        AdjustWindowRect(&rect, style, FALSE);
        final_w = rect.right - rect.left;
        final_h = rect.bottom - rect.top;

        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        int screen_h = GetSystemMetrics(SM_CYSCREEN);
        pos_x = (screen_w - final_w) / 2;
        pos_y = (screen_h - final_h) / 2;
        if (pos_x < 0) pos_x = 0;
        if (pos_y < 0) pos_y = 0;
    }

    hwnd_main = CreateWindowExA(
        0,
        DOOM_WINDOW_CLASS,
        I_GetGameTitle(),
        style,
        pos_x, pos_y,
        final_w, final_h,
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

    Win32_InitGamepad();

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
    if (h_xinput)
    {
        FreeLibrary(h_xinput);
        h_xinput = NULL;
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

    // Convert 8-bit paletted DOOM screen to 32-bit framebuffer (8x unrolled)
    const byte* src = screens[0];
    const int total = SCREENWIDTH * SCREENHEIGHT;
    for (int i = 0; i < total; i += 8)
    {
        argb_framebuffer[i + 0] = argb_palette[src[i + 0]];
        argb_framebuffer[i + 1] = argb_palette[src[i + 1]];
        argb_framebuffer[i + 2] = argb_palette[src[i + 2]];
        argb_framebuffer[i + 3] = argb_palette[src[i + 3]];
        argb_framebuffer[i + 4] = argb_palette[src[i + 4]];
        argb_framebuffer[i + 5] = argb_palette[src[i + 5]];
        argb_framebuffer[i + 6] = argb_palette[src[i + 6]];
        argb_framebuffer[i + 7] = argb_palette[src[i + 7]];
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
    static int last_win_buttons = 0;
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

        int cur_buttons = 0;
        if (GetKeyState(VK_LBUTTON) & 0x8000) cur_buttons |= 1;
        if (GetKeyState(VK_RBUTTON) & 0x8000) cur_buttons |= 2;
        if (GetKeyState(VK_MBUTTON) & 0x8000) cur_buttons |= 4;

        if (dx != 0 || dy != 0 || cur_buttons != last_win_buttons)
        {
            if (dx != 0 || dy != 0)
                SetCursorPos(center_x, center_y);

            event_t ev;
            ev.type = ev_mouse;
            ev.data1 = cur_buttons;
            ev.data2 = dx * 8;
            ev.data3 = -dy * 8;
            D_PostEvent(&ev);
            last_win_buttons = cur_buttons;
        }
    }
    else if (last_win_buttons != 0)
    {
        event_t ev;
        ev.type = ev_mouse;
        ev.data1 = 0;
        ev.data2 = 0;
        ev.data3 = 0;
        D_PostEvent(&ev);
        last_win_buttons = 0;
    }

    Win32_PollGamepad();
#endif
}
