// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_VIDEO__
#define __I_VIDEO__


#include "doomtype.h"

#ifdef __GNUG__
#pragma interface
#endif


// Display output resolution descriptor
typedef struct
{
    int width;
    int height;
    const char* name;
} display_resolution_t;

#define NUM_DISPLAY_RESOLUTIONS 7
extern const display_resolution_t display_resolutions[NUM_DISPLAY_RESOLUTIONS];

extern int current_resolution_index;
extern int display_width;
extern int display_height;
extern boolean display_fullscreen;

// Dynamic display resolution and window mode switching
void I_SetResolution(int width, int height, boolean fullscreen);
void I_SetResolutionIndex(int index, boolean fullscreen);
void I_ToggleFullscreen(void);
void I_GetResolution(int* width, int* height, boolean* fullscreen);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics (void);


void I_ShutdownGraphics(void);

// Takes full 8 bit values.
void I_SetPalette (byte* palette);

void I_UpdateNoBlit (void);
void I_FinishUpdate (void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

void I_ReadScreen (byte* scr);

void I_BeginRead (void);
void I_EndRead (void);

void I_ClearFrame (void);

extern boolean wipe_active;

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
