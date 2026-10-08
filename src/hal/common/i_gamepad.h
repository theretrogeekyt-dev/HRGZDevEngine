//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM Gamepad Driver
// Common gamepad abstraction for Xbox & PlayStation controllers
//
// Supports:
// - PlayStation DualShock 4 & DualSense (PS5)
// - Xbox One, Xbox Series X|S, Xbox 360 & Elite Controllers
// - macOS (GameController.framework)
// - Windows Native (XInput + WinMM DirectInput)
// - Linux Native (SDL2 GameController)
//
//-----------------------------------------------------------------------------

#ifndef __I_GAMEPAD_H__
#define __I_GAMEPAD_H__

#include <stdint.h>
#include <stdbool.h>
#include "d_ticcmd.h"

// Standard Unified Gamepad Button Bitmask
#define PAD_BTN_A        (1 << 0)  // Xbox A / PS Cross (Select / Confirm / Use)
#define PAD_BTN_B        (1 << 1)  // Xbox B / PS Circle (Back / Cancel / Sprint)
#define PAD_BTN_X        (1 << 2)  // Xbox X / PS Square (Use / Open)
#define PAD_BTN_Y        (1 << 3)  // Xbox Y / PS Triangle (Automap)
#define PAD_BTN_LB       (1 << 4)  // Left Bumper (L1) - Previous Weapon
#define PAD_BTN_RB       (1 << 5)  // Right Bumper (R1) - Next Weapon
#define PAD_BTN_BACK     (1 << 6)  // View / Back / Share / Touchpad - Automap
#define PAD_BTN_START    (1 << 7)  // Menu / Start / Options - Menu / Pause
#define PAD_BTN_L3       (1 << 8)  // Left Stick Click - Sprint Toggle (Always Run)
#define PAD_BTN_R3       (1 << 9)  // Right Stick Click - 180° Quick Turn
#define PAD_BTN_DPAD_UP  (1 << 10) // D-Pad Up
#define PAD_BTN_DPAD_DN  (1 << 11) // D-Pad Down
#define PAD_BTN_DPAD_LF  (1 << 12) // D-Pad Left
#define PAD_BTN_DPAD_RT  (1 << 13) // D-Pad Right

typedef struct
{
    int   connected;        // 1 if active, 0 if disconnected

    // Analog sticks in range [-1.0f, +1.0f]
    float left_stick_x;     // Strafe: Left (-1.0) .. Right (+1.0)
    float left_stick_y;     // Move: Backward (-1.0) .. Forward (+1.0)
    float right_stick_x;    // Turn: Left (-1.0) .. Right (+1.0)
    float right_stick_y;    // Pitch: Down (-1.0) .. Up (+1.0)

    // Analog triggers in range [0.0f, 1.0f]
    float left_trigger;     // L2 / LT (Sprint / Speed)
    float right_trigger;    // R2 / RT (Attack / Fire)

    // Digital button bitmask
    uint32_t buttons;
} gamepad_state_t;

// Gamepad lifecycle
void I_Gamepad_Init(void);
void I_Gamepad_Update(const gamepad_state_t* state);

// Called by platform I_BaseTiccmd()
ticcmd_t* I_Gamepad_BaseTiccmd(void);

// Tuning & configuration
void I_Gamepad_SetSensitivity(float look_sens);
void I_Gamepad_SetDeadzone(float deadzone);
void I_Gamepad_SetAlwaysRun(int enable);
int  I_Gamepad_GetAlwaysRun(void);
int  I_Gamepad_IsConnected(void);

#endif // __I_GAMEPAD_H__
