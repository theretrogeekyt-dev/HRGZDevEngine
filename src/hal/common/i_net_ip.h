// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Cross-platform IP (UDP) network driver & lobby for HRGZDevEngine DOOM.
//	Supports Native macOS and Modern Windows with direct IP gameplay.
//
//-----------------------------------------------------------------------------

#ifndef __I_NET_IP__
#define __I_NET_IP__

#include <stdint.h>
#include "doomdef.h"
#include "d_net.h"

#define DEFAULT_DOOM_PORT 5029
#define NET_LOBBY_MAGIC   0x4852475AU // "HRGZ" in ASCII

// Lobby packet types
typedef enum
{
    PKT_CONNECT_REQ = 1,
    PKT_CONNECT_ACK = 2,
    PKT_START_GAME  = 3,
    PKT_START_ACK   = 4
} net_pkt_type_t;

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint8_t  type;
    uint8_t  client_node;
    char     player_name[16];
} net_pkt_connect_req_t;

typedef struct
{
    uint32_t magic;
    uint8_t  type;
    uint8_t  assigned_node; // 1..3
    uint8_t  total_players;
    uint8_t  gamemode;      // 0=coop, 1=dm, 2=altdeath
    uint8_t  episode;
    uint8_t  map;
    uint8_t  skill;
} net_pkt_connect_ack_t;

typedef struct
{
    uint32_t magic;
    uint8_t  type;
    uint8_t  total_players;
    uint8_t  gamemode;
    uint8_t  episode;
    uint8_t  map;
    uint8_t  skill;
    uint32_t peer_ip[MAXNETNODES];   // in network byte order
    uint16_t peer_port[MAXNETNODES]; // in network byte order
} net_pkt_start_game_t;

typedef struct
{
    uint32_t magic;
    uint8_t  type;
    uint8_t  node;
} net_pkt_start_ack_t;
#pragma pack(pop)

typedef enum
{
    NET_STATE_OFFLINE = 0,
    NET_STATE_HOST_LOBBY,
    NET_STATE_CLIENT_LOBBY,
    NET_STATE_ACTIVE
} net_state_t;

// Sockets subsystem
boolean I_Net_InitSockets(void);
void    I_Net_ShutdownSockets(void);

// Lobby management
boolean I_Net_HostGame(int max_players, int port, int skill, int episode, int map, int dm);
boolean I_Net_JoinGame(const char* host_ip, int port);
void    I_Net_CancelLobby(void);

// Tickers polled from menu loop
int     I_Net_HostLobbyTicker(void);   // returns number of connected players
boolean I_Net_ClientLobbyTicker(void); // returns true if host triggered game start

// Launch from host lobby
boolean I_Net_HostLaunch(void);

// Status queries for menu rendering
net_state_t I_Net_GetState(void);
int         I_Net_GetConnectedCount(void);
int         I_Net_GetMaxPlayers(void);
const char* I_Net_GetHostStatusMsg(void);
const char* I_Net_GetClientStatusMsg(void);
const char* I_Net_GetLocalIP(void);
const char* I_Net_GetClientNodeIP(int node);
int         I_Net_GetClientNode(void);

// Game parameters received by client
int I_Net_GetGameSkill(void);
int I_Net_GetGameEpisode(void);
int I_Net_GetGameMap(void);
int I_Net_GetGameDeathmatch(void);

// Standard DOOM net driver interface
void I_Net_Init(void);
void I_Net_Cmd(void);

#endif
