// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Sound Blaster DSP & 8-bit Auto-Init DMA audio driver for MS-DOS (DJGPP).
//
//-----------------------------------------------------------------------------

#ifndef __I_SB_DOS_H__
#define __I_SB_DOS_H__

#include "doomtype.h"

// Initialize Sound Blaster DSP and DMA channel
// Returns true if Sound Blaster hardware detected and initialized
boolean I_SB_Init(void);

// Shutdown Sound Blaster DSP and stop DMA playback
void I_SB_Shutdown(void);

// Call periodically (e.g. every frame in I_SubmitSound) to stream DMA audio
void I_SB_Update(void);

#endif // __I_SB_DOS_H__

