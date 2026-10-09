//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM - Xbox 360 HAL: Network Driver
// Loopback & Peer Pressure System Link driver
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

    printf("I_InitNetwork (Xbox 360): Local network loopback initialized\n");
}

void I_NetCmd(void)
{
    if (doomcom->command == CMD_SEND)
    {
        doomcom->remotenode = 0;
    }
}

