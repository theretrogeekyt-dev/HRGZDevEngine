// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DOOM network interface for MS-DOS (local single-player loopback stub).
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
        // Loopback send to self
        doomcom->remotenode = 0;
    }
}
