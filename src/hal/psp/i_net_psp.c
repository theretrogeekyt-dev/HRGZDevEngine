// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// Network driver (Single-player local setup).
//
// Uses the official PSPDEV SDK (https://pspdev.github.io)
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "i_system.h"
#include "i_net.h"

void I_InitNetwork(void)
{
    doomcom = (doomcom_t*)malloc(sizeof(*doomcom));
    if (!doomcom)
        I_Error("I_InitNetwork: Failed to allocate doomcom");

    memset(doomcom, 0, sizeof(*doomcom));

    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = 1;
    doomcom->numnodes = 1;
    doomcom->consoleplayer = 0;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
    netgame = false;
}

void I_NetCmd(void)
{
    if (doomcom->command == CMD_SEND)
    {
        doomcom->remotenode = 0;
    }
}
