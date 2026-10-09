// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Common video resolution and display mode definitions across HAL drivers.
//
//-----------------------------------------------------------------------------

#ifndef __I_VIDEO_COMMON_H__
#define __I_VIDEO_COMMON_H__

#include "i_video.h"

int  I_FindResolutionIndex(int width, int height);
void I_ParseDisplayParams(int* out_width, int* out_height, boolean* out_fullscreen);

#endif
