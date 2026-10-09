// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// Network driver (Single-player local setup and menu lobby stubs).
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
#include "i_net_ip.h"

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

//-----------------------------------------------------------------------------
// Sockets Subsystem Stubs
//-----------------------------------------------------------------------------

boolean I_Net_InitSockets(void)
{
    return false;
}

void I_Net_ShutdownSockets(void)
{
}

//-----------------------------------------------------------------------------
// Lobby Management Stubs
//-----------------------------------------------------------------------------

boolean I_Net_HostGame(int max_players, int port, int skill, int episode, int map, int dm)
{
    (void)max_players; (void)port; (void)skill; (void)episode; (void)map; (void)dm;
    return false;
}

boolean I_Net_JoinGame(const char* host_ip, int port)
{
    (void)host_ip; (void)port;
    return false;
}

void I_Net_CancelLobby(void)
{
}

//-----------------------------------------------------------------------------
// Tickers Polled From Menu Loop
//-----------------------------------------------------------------------------

int I_Net_HostLobbyTicker(void)
{
    return 0;
}

boolean I_Net_ClientLobbyTicker(void)
{
    return false;
}

//-----------------------------------------------------------------------------
// Host Launch Stub
//-----------------------------------------------------------------------------

boolean I_Net_HostLaunch(void)
{
    return false;
}

//-----------------------------------------------------------------------------
// Status Queries For Menu Rendering
//-----------------------------------------------------------------------------

net_state_t I_Net_GetState(void)
{
    return NET_STATE_OFFLINE;
}

int I_Net_GetConnectedCount(void)
{
    return 1;
}

int I_Net_GetMaxPlayers(void)
{
    return 1;
}

const char* I_Net_GetHostStatusMsg(void)
{
    return "Multiplayer unavailable on PSP";
}

const char* I_Net_GetClientStatusMsg(void)
{
    return "Multiplayer unavailable on PSP";
}

const char* I_Net_GetLocalIP(void)
{
    return "127.0.0.1";
}

const char* I_Net_GetClientNodeIP(int node)
{
    (void)node;
    return "127.0.0.1";
}

int I_Net_GetClientNode(void)
{
    return 0;
}

//-----------------------------------------------------------------------------
// Game Parameters Received By Client
//-----------------------------------------------------------------------------

int I_Net_GetGameSkill(void)
{
    return 2;
}

int I_Net_GetGameEpisode(void)
{
    return 1;
}

int I_Net_GetGameMap(void)
{
    return 1;
}

int I_Net_GetGameDeathmatch(void)
{
    return 0;
}

//-----------------------------------------------------------------------------
// Standard Net Driver Callbacks
//-----------------------------------------------------------------------------

void I_Net_Init(void)
{
    I_InitNetwork();
}

void I_Net_Cmd(void)
{
    I_NetCmd();
}
