// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Native macOS network driver linking to unified IP (UDP) engine.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "d_net.h"
#include "i_net.h"
#include "../common/i_net_ip.h"

void I_InitNetwork(void)
{
    I_Net_Init();
}

void I_NetCmd(void)
{
    I_Net_Cmd();
}
