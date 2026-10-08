// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	AdLib / Yamaha OPL2 / OPL3 FM synthesis music driver for MS-DOS (DJGPP).
//
//-----------------------------------------------------------------------------

#ifndef __I_OPL_DOS_H__
#define __I_OPL_DOS_H__

#include "doomtype.h"

// Initialize AdLib / OPL chip and load GENMIDI instrument definitions
// Returns true if AdLib hardware is present and ready
boolean I_OPL_Init(void);

// Silence all OPL voices and reset synthesizer
void I_OPL_Shutdown(void);

// Register a MUS song lump for playback
int  I_OPL_RegisterSong(void* data, int len);

// Free registered song
void I_OPL_UnRegisterSong(int handle);

// Play registered song
void I_OPL_PlaySong(int handle, int looping);

// Stop playing song
void I_OPL_StopSong(int handle);

// Pause / resume current song
void I_OPL_PauseSong(int handle);
void I_OPL_ResumeSong(int handle);

// Set master music volume (0..15)
void I_OPL_SetMusicVolume(int volume);

// Update sequencer by 'tics' (140 Hz MUS tics)
void I_OPL_Update(int tics);

#endif // __I_OPL_DOS_H__

