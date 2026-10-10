// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM Game Distribution System
// Game Configuration, Branding & Auto-Discovery Subsystem
//
//-----------------------------------------------------------------------------

#ifndef __I_GAME_CONFIG_H__
#define __I_GAME_CONFIG_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns the display title for the game window and banners
const char* I_GetGameTitle(void);

// Returns the game identifier slug (used for save directory separation)
const char* I_GetGameId(void);

// Returns the path to the auto-discovered game WAD (or NULL if not found)
const char* I_GetGameWadPath(void);

// Returns the configured game mode string (shareware, registered, retail, commercial) or NULL
const char* I_GetGameModeString(void);

// Returns the full directory path for saves and configuration
const char* I_GetSaveDir(void);

// Initialize game configuration (reads game.json / bundle resources if present)
void I_InitGameConfig(void);

#ifdef __cplusplus
}
#endif

#endif // __I_GAME_CONFIG_H__

