//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM Gamepad Driver
// Common gamepad abstraction implementation
//
// Handles:
// - Analog left-stick movement (forward/backward & strafing)
// - Analog right-stick smooth camera rotation (look/aim with deadzone & curve)
// - 180° quick-turn on R3 click
// - Always-run toggle on L3 click
// - Analog trigger fire & sprint (RT/R2, LT/L2)
// - Face buttons (A/Cross: Use/Enter, B/Circle: Back/Run, X/Square: Use, Y/Triangle: Map)
// - Shoulder buttons (RB/R1: Next Weapon, LB/L1: Previous Weapon)
// - Menu navigation (D-pad & Left Stick with repeat debounce)
// - Zero external dependencies (shared across macOS, Windows, Linux)
//
//-----------------------------------------------------------------------------

#include "i_gamepad.h"
#include "doomdef.h"
#include "d_event.h"
#include "d_main.h"
#include "doomstat.h"
#include "d_player.h"
#include "d_items.h"
#include "m_misc.h"
#include "i_system.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Configuration & Internal State
static int        s_connected        = 0;
static uint32_t   s_prev_buttons     = 0;
static ticcmd_t   s_gamepad_cmd;
static float      s_look_sens        = 1.0f;
static float      s_deadzone         = 0.15f;
static int        s_always_run       = 1; // Default to modern Always Run ON

// Menu repeat debounce state
static int        s_menu_last_key    = 0;
static int        s_menu_repeat_tic  = 0;

static void PostKeyStroke(int key)
{
    event_t ev_down;
    ev_down.type = ev_keydown;
    ev_down.data1 = key;
    ev_down.data2 = 0;
    ev_down.data3 = 0;
    D_PostEvent(&ev_down);

    event_t ev_up;
    ev_up.type = ev_keyup;
    ev_up.data1 = key;
    ev_up.data2 = 0;
    ev_up.data3 = 0;
    D_PostEvent(&ev_up);
}

static void ApplyRadialDeadzone(float in_x, float in_y, float* out_x, float* out_y, float dz, int quadratic_curve)
{
    float mag = sqrtf(in_x * in_x + in_y * in_y);
    if (mag <= dz)
    {
        *out_x = 0.0f;
        *out_y = 0.0f;
        return;
    }

    float scaled = (mag - dz) / (1.0f - dz);
    if (scaled > 1.0f) scaled = 1.0f;

    // Optional quadratic curve for fine-aim precision
    float factor = quadratic_curve ? (scaled * scaled) : scaled;

    *out_x = (in_x / mag) * factor;
    *out_y = (in_y / mag) * factor;
}

static void CycleWeapon(int dir)
{
    if (gamestate != GS_LEVEL)
        return;

    player_t* player = &players[consoleplayer];
    if (!player || !player->mo)
        return;

    static const weapontype_t weapon_order[] = {
        wp_fist,
        wp_chainsaw,
        wp_pistol,
        wp_shotgun,
        wp_supershotgun,
        wp_chaingun,
        wp_missile,
        wp_plasma,
        wp_bfg
    };
    const int total = sizeof(weapon_order) / sizeof(weapon_order[0]);

    int current_idx = 0;
    for (int i = 0; i < total; i++)
    {
        if (weapon_order[i] == player->readyweapon)
        {
            current_idx = i;
            break;
        }
    }

    for (int step = 1; step <= total; step++)
    {
        int next_idx = (current_idx + dir * step + total * 10) % total;
        weapontype_t candidate = weapon_order[next_idx];

        if (!player->weaponowned[candidate])
            continue;

        ammotype_t ammo = weaponinfo[candidate].ammo;
        if (ammo != am_noammo && player->ammo[ammo] < 1)
            continue;

        // Weapon is owned and has ammunition available
        s_gamepad_cmd.buttons |= BT_CHANGE | ((byte)candidate << BT_WEAPONSHIFT);
        player->pendingweapon = candidate;
        break;
    }
}

static void SelectWeaponSlot(weapontype_t primary, weapontype_t alt)
{
    if (gamestate != GS_LEVEL)
        return;

    player_t* player = &players[consoleplayer];
    if (!player || !player->mo)
        return;

    weapontype_t target = primary;
    if (player->weaponowned[alt] && (player->readyweapon == primary || !player->weaponowned[primary]))
    {
        target = alt;
    }

    if (player->weaponowned[target])
    {
        ammotype_t ammo = weaponinfo[target].ammo;
        if (ammo == am_noammo || player->ammo[ammo] > 0)
        {
            s_gamepad_cmd.buttons |= BT_CHANGE | ((byte)target << BT_WEAPONSHIFT);
            player->pendingweapon = target;
        }
    }
}

void I_Gamepad_Init(void)
{
    memset(&s_gamepad_cmd, 0, sizeof(s_gamepad_cmd));
    s_connected = 0;
    s_prev_buttons = 0;
    s_menu_last_key = 0;
    s_menu_repeat_tic = 0;
}

void I_Gamepad_Update(const gamepad_state_t* state)
{
    if (!state || !state->connected)
    {
        s_connected = 0;
        s_prev_buttons = 0;
        memset(&s_gamepad_cmd, 0, sizeof(s_gamepad_cmd));
        return;
    }

    s_connected = 1;
    uint32_t curr_btn = state->buttons;
    uint32_t pressed  = curr_btn & ~s_prev_buttons;
    s_prev_buttons    = curr_btn;

    int in_menu = menuactive;
    int current_tic = I_GetTime();

    //-------------------------------------------------------------------------
    // 1. Menu Navigation Mode
    //-------------------------------------------------------------------------
    if (in_menu)
    {
        // Zero out playsim ticcmd while in menu
        memset(&s_gamepad_cmd, 0, sizeof(s_gamepad_cmd));

        int nav_key = 0;
        if (curr_btn & PAD_BTN_DPAD_UP) nav_key = KEY_UPARROW;
        else if (curr_btn & PAD_BTN_DPAD_DN) nav_key = KEY_DOWNARROW;
        else if (curr_btn & PAD_BTN_DPAD_LF) nav_key = KEY_LEFTARROW;
        else if (curr_btn & PAD_BTN_DPAD_RT) nav_key = KEY_RIGHTARROW;
        else if (state->left_stick_y > 0.55f) nav_key = KEY_UPARROW;
        else if (state->left_stick_y < -0.55f) nav_key = KEY_DOWNARROW;
        else if (state->left_stick_x < -0.55f) nav_key = KEY_LEFTARROW;
        else if (state->left_stick_x > 0.55f) nav_key = KEY_RIGHTARROW;

        if (nav_key != 0)
        {
            if (nav_key != s_menu_last_key)
            {
                // First press: trigger immediately
                PostKeyStroke(nav_key);
                s_menu_last_key = nav_key;
                s_menu_repeat_tic = current_tic + 10; // Initial delay ~280ms
            }
            else if (current_tic >= s_menu_repeat_tic)
            {
                // Held: repeat trigger
                PostKeyStroke(nav_key);
                s_menu_repeat_tic = current_tic + 4; // Repeat delay ~110ms
            }
        }
        else
        {
            s_menu_last_key = 0;
        }

        // Confirm / Select / Use / Yes (A / Cross or X / Square)
        if (pressed & (PAD_BTN_A | PAD_BTN_X))
        {
            PostKeyStroke(KEY_ENTER);
        }

        // Cancel / Back / No (B / Circle or Y / Triangle)
        if (pressed & (PAD_BTN_B | PAD_BTN_Y))
        {
            PostKeyStroke(KEY_ESCAPE);
        }

        // Menu / Start toggle
        if (pressed & PAD_BTN_START)
        {
            PostKeyStroke(KEY_ESCAPE);
        }

        return;
    }

    // Reset menu repeat tracking
    s_menu_last_key = 0;

    //-------------------------------------------------------------------------
    // 2. In-Game Playsim Controls
    //-------------------------------------------------------------------------

    // Start / Menu button opens Doom Options Menu
    if (pressed & PAD_BTN_START)
    {
        PostKeyStroke(KEY_ESCAPE);
        return;
    }

    // Automap toggle (Y / Triangle or Back / View / Share / Touchpad)
    if (pressed & (PAD_BTN_Y | PAD_BTN_BACK))
    {
        PostKeyStroke(KEY_TAB);
    }

    // Left Stick Click (L3): Toggle Always Run
    if (pressed & PAD_BTN_L3)
    {
        s_always_run = !s_always_run;
        if (players[consoleplayer].mo)
        {
            players[consoleplayer].message = s_always_run ? "ALWAYS RUN: ON" : "ALWAYS RUN: OFF";
        }
    }

    // Right Stick Click (R3): 180° Quick Turn
    if (pressed & PAD_BTN_R3)
    {
        s_gamepad_cmd.angleturn += (short)0x8000;
    }

    // Shoulder Weapon Cycling
    if (pressed & PAD_BTN_RB)
    {
        CycleWeapon(1);  // Next Weapon
    }
    if (pressed & PAD_BTN_LB)
    {
        CycleWeapon(-1); // Previous Weapon
    }

    // In-Game D-Pad Quick Weapon Select
    if (pressed & PAD_BTN_DPAD_UP)
    {
        SelectWeaponSlot(wp_shotgun, wp_supershotgun);
    }
    else if (pressed & PAD_BTN_DPAD_DN)
    {
        SelectWeaponSlot(wp_chaingun, wp_chaingun);
    }
    else if (pressed & PAD_BTN_DPAD_LF)
    {
        SelectWeaponSlot(wp_missile, wp_plasma);
    }
    else if (pressed & PAD_BTN_DPAD_RT)
    {
        SelectWeaponSlot(wp_bfg, wp_chainsaw);
    }

    // Determine Run Speed
    int is_running = s_always_run;
    if (state->left_trigger > 0.3f || (curr_btn & PAD_BTN_B))
    {
        // Holding LT / L2 or B / Circle acts as Sprint / Turbo
        is_running = !s_always_run; // Inverts: if run was off -> sprints; if on -> precision walk
    }

    // Analog Left Stick: Movement (Y) and Strafe (X)
    float lx = 0.0f, ly = 0.0f;
    ApplyRadialDeadzone(state->left_stick_x, state->left_stick_y, &lx, &ly, s_deadzone, 0);

    const int max_fwd  = is_running ? 50 : 25;
    const int max_side = is_running ? 40 : 24;

    s_gamepad_cmd.forwardmove = (char)(ly * max_fwd);
    s_gamepad_cmd.sidemove    = (char)(lx * max_side);

    // Analog Right Stick: Smooth Sub-Pixel Aiming / Turning
    float rx = 0.0f, ry = 0.0f;
    ApplyRadialDeadzone(state->right_stick_x, state->right_stick_y, &rx, &ry, s_deadzone, 1);

    // Turn speed: ~1400 units per tic at max stick deflection
    float turn_delta = -rx * 1400.0f * s_look_sens;
    s_gamepad_cmd.angleturn += (short)turn_delta;

    // Triggers and Buttons
    s_gamepad_cmd.buttons &= ~(BT_ATTACK | BT_USE);

    // Right Trigger (RT / R2): Fire / Attack
    if (state->right_trigger > 0.25f)
    {
        s_gamepad_cmd.buttons |= BT_ATTACK;
    }

    // Use / Open Door (A / Cross or X / Square)
    if (curr_btn & (PAD_BTN_A | PAD_BTN_X))
    {
        s_gamepad_cmd.buttons |= BT_USE;
    }
}

ticcmd_t* I_Gamepad_BaseTiccmd(void)
{
    static ticcmd_t out_cmd;
    out_cmd = s_gamepad_cmd;

    // Reset per-tic transient deltas and one-shot weapon change bits
    s_gamepad_cmd.angleturn = 0;
    s_gamepad_cmd.buttons &= ~(BT_CHANGE | BT_WEAPONMASK);

    return &out_cmd;
}

void I_Gamepad_SetSensitivity(float look_sens)
{
    if (look_sens < 0.2f) look_sens = 0.2f;
    if (look_sens > 4.0f) look_sens = 4.0f;
    s_look_sens = look_sens;
}

void I_Gamepad_SetDeadzone(float deadzone)
{
    if (deadzone < 0.02f) deadzone = 0.02f;
    if (deadzone > 0.40f) deadzone = 0.40f;
    s_deadzone = deadzone;
}

void I_Gamepad_SetAlwaysRun(int enable)
{
    s_always_run = enable ? 1 : 0;
}

int I_Gamepad_GetAlwaysRun(void)
{
    return s_always_run;
}

int I_Gamepad_IsConnected(void)
{
    return s_connected;
}
