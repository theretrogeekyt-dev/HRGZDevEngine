// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Hardware-accelerated 2D video and input driver using modern SDL2.
//	Supports hardware vsync, integer scaling, fullscreen, aspect ratio correction.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_gamepad.h"

static SDL_Window*         sdl_window = NULL;
static SDL_Renderer*       sdl_renderer = NULL;
static SDL_Texture*        sdl_texture = NULL;
static SDL_GameController* sdl_controller = NULL;
static uint32_t            sdl_palette[256];
static uint32_t            sdl_pixels[SCREENWIDTH * SCREENHEIGHT];

static void SDL_OpenController(int index)
{
    if (sdl_controller)
        return;
    if (SDL_IsGameController(index))
    {
        sdl_controller = SDL_GameControllerOpen(index);
        if (sdl_controller)
        {
            printf("I_InitGraphics: Connected GameController: %s\n", SDL_GameControllerName(sdl_controller));
        }
    }
}

static int TranslateSDLKey(SDL_Keycode sym)
{
    switch (sym)
    {
        case SDLK_LEFT:      return KEY_LEFTARROW;
        case SDLK_RIGHT:     return KEY_RIGHTARROW;
        case SDLK_DOWN:      return KEY_DOWNARROW;
        case SDLK_UP:        return KEY_UPARROW;
        case SDLK_ESCAPE:    return KEY_ESCAPE;
        case SDLK_RETURN:    return KEY_ENTER;
        case SDLK_TAB:       return KEY_TAB;
        case SDLK_F1:        return KEY_F1;
        case SDLK_F2:        return KEY_F2;
        case SDLK_F3:        return KEY_F3;
        case SDLK_F4:        return KEY_F4;
        case SDLK_F5:        return KEY_F5;
        case SDLK_F6:        return KEY_F6;
        case SDLK_F7:        return KEY_F7;
        case SDLK_F8:        return KEY_F8;
        case SDLK_F9:        return KEY_F9;
        case SDLK_F10:       return KEY_F10;
        case SDLK_F11:       return KEY_F11;
        case SDLK_F12:       return KEY_F12;
        case SDLK_BACKSPACE: return KEY_BACKSPACE;
        case SDLK_PAUSE:     return KEY_PAUSE;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:    return KEY_RSHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL:     return KEY_RCTRL;
        case SDLK_LALT:
        case SDLK_RALT:      return KEY_RALT;
        case SDLK_SPACE:     return ' ';
        case SDLK_MINUS:     return KEY_MINUS;
        case SDLK_EQUALS:    return KEY_EQUALS;
        case SDLK_COMMA:     return ',';
        case SDLK_PERIOD:    return '.';
        default:
            if (sym >= SDLK_a && sym <= SDLK_z)
                return (int)sym;
            if (sym >= SDLK_0 && sym <= SDLK_9)
                return (int)sym;
            return 0;
    }
}

void I_InitGraphics(void)
{
    screens[0] = (byte*)malloc(SCREENWIDTH * SCREENHEIGHT);
    if (!screens[0])
        I_Error("I_InitGraphics: Could not allocate primary screen buffer");

    int scale = 3;
    int p = M_CheckParm("-scale");
    if (p && p < myargc - 1)
        scale = atoi(myargv[p + 1]);
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;

    int win_w = SCREENWIDTH * scale;
    int win_h = (int)(SCREENHEIGHT * scale * 1.2);

    uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (M_CheckParm("-fullscreen"))
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    sdl_window = SDL_CreateWindow(
        "HRGZDevEngine DOOM (SDL2)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        flags
    );

    if (!sdl_window)
        I_Error("I_InitGraphics: SDL_CreateWindow failed: %s", SDL_GetError());

    sdl_renderer = SDL_CreateRenderer(
        sdl_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!sdl_renderer)
    {
        sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
    }

    if (!sdl_renderer)
        I_Error("I_InitGraphics: SDL_CreateRenderer failed: %s", SDL_GetError());

    // Maintain authentic display aspect ratio automatically in modern widescreen monitors
    SDL_RenderSetLogicalSize(sdl_renderer, SCREENWIDTH, (int)(SCREENHEIGHT * 1.2));

    sdl_texture = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREENWIDTH, SCREENHEIGHT
    );

    if (!sdl_texture)
        I_Error("I_InitGraphics: SDL_CreateTexture failed: %s", SDL_GetError());

    if (!M_CheckParm("-nomouse"))
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    I_Gamepad_Init();

    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        if (SDL_IsGameController(i))
        {
            SDL_OpenController(i);
            break;
        }
    }
}

void I_ShutdownGraphics(void)
{
    if (sdl_texture)
    {
        SDL_DestroyTexture(sdl_texture);
        sdl_texture = NULL;
    }
    if (sdl_renderer)
    {
        SDL_DestroyRenderer(sdl_renderer);
        sdl_renderer = NULL;
    }
    if (sdl_window)
    {
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
    }
    if (sdl_controller)
    {
        SDL_GameControllerClose(sdl_controller);
        sdl_controller = NULL;
    }
    if (screens[0])
    {
        free(screens[0]);
        screens[0] = NULL;
    }
}

void I_SetPalette(byte* palette)
{
    for (int i = 0; i < 256; i++)
    {
        uint8_t r = palette[i * 3 + 0];
        uint8_t g = palette[i * 3 + 1];
        uint8_t b = palette[i * 3 + 2];
        sdl_palette[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    if (!screens[0] || !sdl_renderer || !sdl_texture)
        return;

    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
    {
        sdl_pixels[i] = sdl_palette[screens[0][i]];
    }

    SDL_UpdateTexture(sdl_texture, NULL, sdl_pixels, SCREENWIDTH * sizeof(uint32_t));
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);

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
    SDL_Event e;
    event_t ev;

    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_KEYDOWN:
            {
                int k = TranslateSDLKey(e.key.keysym.sym);
                if (k)
                {
                    ev.type = ev_keydown;
                    ev.data1 = k;
                    ev.data2 = 0;
                    ev.data3 = 0;
                    D_PostEvent(&ev);
                }
                break;
            }
            case SDL_KEYUP:
            {
                int k = TranslateSDLKey(e.key.keysym.sym);
                if (k)
                {
                    ev.type = ev_keyup;
                    ev.data1 = k;
                    ev.data2 = 0;
                    ev.data3 = 0;
                    D_PostEvent(&ev);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                int buttons = 0;
                uint32_t state = SDL_GetMouseState(NULL, NULL);
                if (state & SDL_BUTTON(SDL_BUTTON_LEFT))   buttons |= 1;
                if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))  buttons |= 2;
                if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) buttons |= 4;

                ev.type = ev_mouse;
                ev.data1 = buttons;
                ev.data2 = 0;
                ev.data3 = 0;
                D_PostEvent(&ev);
                break;
            }
            case SDL_MOUSEMOTION:
            {
                if (!M_CheckParm("-nomouse"))
                {
                    int buttons = 0;
                    uint32_t state = SDL_GetMouseState(NULL, NULL);
                    if (state & SDL_BUTTON(SDL_BUTTON_LEFT))   buttons |= 1;
                    if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))  buttons |= 2;
                    if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) buttons |= 4;

                    ev.type = ev_mouse;
                    ev.data1 = buttons;
                    ev.data2 = e.motion.xrel * 8;
                    ev.data3 = -e.motion.yrel * 8;
                    D_PostEvent(&ev);
                }
                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
                SDL_OpenController(e.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (sdl_controller)
                {
                    SDL_Joystick* j = SDL_GameControllerGetJoystick(sdl_controller);
                    if (SDL_JoystickInstanceID(j) == e.cdevice.which)
                    {
                        SDL_GameControllerClose(sdl_controller);
                        sdl_controller = NULL;
                    }
                }
                break;
            case SDL_QUIT:
                I_Quit();
                break;
        }
    }

    // Poll SDL_GameController
    if (sdl_controller)
    {
        gamepad_state_t pad_state;
        memset(&pad_state, 0, sizeof(pad_state));
        pad_state.connected = 1;

        Sint16 lx = SDL_GameControllerGetAxis(sdl_controller, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ly = SDL_GameControllerGetAxis(sdl_controller, SDL_CONTROLLER_AXIS_LEFTY);
        Sint16 rx = SDL_GameControllerGetAxis(sdl_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        Sint16 ry = SDL_GameControllerGetAxis(sdl_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        Sint16 lt = SDL_GameControllerGetAxis(sdl_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        Sint16 rt = SDL_GameControllerGetAxis(sdl_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

        pad_state.left_stick_x  = (float)lx / (lx < 0 ? 32768.0f : 32767.0f);
        pad_state.left_stick_y  = -((float)ly / (ly < 0 ? 32768.0f : 32767.0f)); // Invert SDL Y
        pad_state.right_stick_x = (float)rx / (rx < 0 ? 32768.0f : 32767.0f);
        pad_state.right_stick_y = -((float)ry / (ry < 0 ? 32768.0f : 32767.0f));

        pad_state.left_trigger  = lt > 0 ? ((float)lt / 32767.0f) : 0.0f;
        pad_state.right_trigger = rt > 0 ? ((float)rt / 32767.0f) : 0.0f;

        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_A))
            pad_state.buttons |= PAD_BTN_A;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_B))
            pad_state.buttons |= PAD_BTN_B;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_X))
            pad_state.buttons |= PAD_BTN_X;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_Y))
            pad_state.buttons |= PAD_BTN_Y;

        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
            pad_state.buttons |= PAD_BTN_LB;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
            pad_state.buttons |= PAD_BTN_RB;

        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_START))
            pad_state.buttons |= PAD_BTN_START;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_BACK))
            pad_state.buttons |= PAD_BTN_BACK;

        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_LEFTSTICK))
            pad_state.buttons |= PAD_BTN_L3;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK))
            pad_state.buttons |= PAD_BTN_R3;

        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
            pad_state.buttons |= PAD_BTN_DPAD_UP;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            pad_state.buttons |= PAD_BTN_DPAD_DN;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            pad_state.buttons |= PAD_BTN_DPAD_LF;
        if (SDL_GameControllerGetButton(sdl_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            pad_state.buttons |= PAD_BTN_DPAD_RT;

        I_Gamepad_Update(&pad_state);
    }
    else
    {
        I_Gamepad_Update(NULL);
    }
}

