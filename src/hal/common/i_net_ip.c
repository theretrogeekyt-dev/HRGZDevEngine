// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Cross-platform IP (UDP) network driver & in-game lobby for HRGZDevEngine DOOM.
//	Native macOS & Modern Windows compatible.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__MSDOS__) && !defined(MSDOS)

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#if !defined(_SOCKLEN_T) && !defined(__socklen_t_defined) && !defined(_SSIZE_T_DEFINED) && defined(_MSC_VER) && (_MSC_VER < 1900)
typedef int socklen_t;
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket(s) close(s)
#endif

#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "i_system.h"
#include "i_net.h"
#include "i_net_ip.h"

static boolean sockets_initialized = false;
static SOCKET  net_socket = INVALID_SOCKET;
static net_state_t current_net_state = NET_STATE_OFFLINE;

static struct sockaddr_in peer_addrs[MAXNETNODES];
static char peer_ip_strings[MAXNETNODES][32];

static int host_target_players = 2;
static int host_port = DEFAULT_DOOM_PORT;
static int host_skill_val = 2;
static int host_episode_val = 1;
static int host_map_val = 1;
static int host_deathmatch_val = 0;

static int client_assigned_node = -1;
static struct sockaddr_in host_dest_addr;
static char client_host_ip_str[64] = "127.0.0.1";
static int  client_host_port_val = DEFAULT_DOOM_PORT;

static int client_game_skill = 2;
static int client_game_episode = 1;
static int client_game_map = 1;
static int client_game_deathmatch = 0;

static int connected_players = 1;
static int last_hello_time = 0;

static char local_ip_cached[32] = "127.0.0.1";
static char status_message[128] = "";

static void SetSocketNonBlocking(SOCKET s)
{
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

boolean I_Net_InitSockets(void)
{
    if (sockets_initialized)
        return true;

#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("I_Net_InitSockets: WSAStartup failed!\n");
        return false;
    }
#endif

    sockets_initialized = true;

    // Detect primary local LAN IP
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        struct hostent* he = gethostbyname(hostname);
        if (he && he->h_addr_list[0])
        {
            for (int i = 0; he->h_addr_list[i] != NULL; i++)
            {
                struct in_addr addr = *(struct in_addr*)he->h_addr_list[i];
                char* ip = inet_ntoa(addr);
                if (ip && strncmp(ip, "127.", 4) != 0)
                {
                    strncpy(local_ip_cached, ip, sizeof(local_ip_cached) - 1);
                    break;
                }
            }
        }
    }

    return true;
}

void I_Net_ShutdownSockets(void)
{
    if (net_socket != INVALID_SOCKET)
    {
        closesocket(net_socket);
        net_socket = INVALID_SOCKET;
    }

#if defined(_WIN32)
    if (sockets_initialized)
    {
        WSACleanup();
    }
#endif

    sockets_initialized = false;
    current_net_state = NET_STATE_OFFLINE;
}

net_state_t I_Net_GetState(void)
{
    return current_net_state;
}

int I_Net_GetConnectedCount(void)
{
    return connected_players;
}

int I_Net_GetMaxPlayers(void)
{
    return host_target_players;
}

const char* I_Net_GetLocalIP(void)
{
    return local_ip_cached;
}

const char* I_Net_GetClientNodeIP(int node)
{
    if (node >= 0 && node < MAXNETNODES)
        return peer_ip_strings[node];
    return "UNKNOWN";
}

const char* I_Net_GetHostStatusMsg(void)
{
    return status_message;
}

const char* I_Net_GetClientStatusMsg(void)
{
    return status_message;
}

int I_Net_GetGameSkill(void)      { return client_game_skill; }
int I_Net_GetGameEpisode(void)    { return client_game_episode; }
int I_Net_GetGameMap(void)        { return client_game_map; }
int I_Net_GetGameDeathmatch(void) { return client_game_deathmatch; }
int I_Net_GetClientNode(void)      { return client_assigned_node; }

boolean I_Net_HostGame(int max_players, int port, int skill, int episode, int map, int dm)
{
    I_Net_InitSockets();

    if (net_socket != INVALID_SOCKET)
    {
        closesocket(net_socket);
        net_socket = INVALID_SOCKET;
    }

    net_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (net_socket == INVALID_SOCKET)
    {
        printf("I_Net_HostGame: Failed to create UDP socket!\n");
        return false;
    }

    int opt = 1;
    setsockopt(net_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    SetSocketNonBlocking(net_socket);

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(port);

    if (bind(net_socket, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == SOCKET_ERROR)
    {
        printf("I_Net_HostGame: Failed to bind to port %d!\n", port);
        closesocket(net_socket);
        net_socket = INVALID_SOCKET;
        return false;
    }

    host_target_players = (max_players < 2) ? 2 : (max_players > 4 ? 4 : max_players);
    host_port = port;
    host_skill_val = skill;
    host_episode_val = episode;
    host_map_val = map;
    host_deathmatch_val = dm;

    connected_players = 1; // Host is player 1
    current_net_state = NET_STATE_HOST_LOBBY;

    memset(peer_addrs, 0, sizeof(peer_addrs));
    memset(peer_ip_strings, 0, sizeof(peer_ip_strings));

    // Node 0 is Host (local loopback)
    peer_addrs[0].sin_family = AF_INET;
    peer_addrs[0].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    peer_addrs[0].sin_port = htons(port);
    strncpy(peer_ip_strings[0], "127.0.0.1 (YOU)", sizeof(peer_ip_strings[0]) - 1);

    sprintf(status_message, "WAITING FOR PLAYERS (1/%d)...", host_target_players);
    printf("I_Net_HostGame: Listening on UDP port %d for %d players...\n", port, host_target_players);
    return true;
}

boolean I_Net_JoinGame(const char* host_ip, int port)
{
    I_Net_InitSockets();

    if (net_socket != INVALID_SOCKET)
    {
        closesocket(net_socket);
        net_socket = INVALID_SOCKET;
    }

    net_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (net_socket == INVALID_SOCKET)
    {
        printf("I_Net_JoinGame: Failed to create UDP socket!\n");
        return false;
    }

    SetSocketNonBlocking(net_socket);

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(0); // Bind ephemeral port

    if (bind(net_socket, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == SOCKET_ERROR)
    {
        printf("I_Net_JoinGame: Failed to bind client socket!\n");
        closesocket(net_socket);
        net_socket = INVALID_SOCKET;
        return false;
    }

    memset(&host_dest_addr, 0, sizeof(host_dest_addr));
    host_dest_addr.sin_family = AF_INET;
    host_dest_addr.sin_port = htons(port);
    host_dest_addr.sin_addr.s_addr = inet_addr(host_ip);

    if (host_dest_addr.sin_addr.s_addr == INADDR_NONE)
    {
        struct hostent* he = gethostbyname(host_ip);
        if (he && he->h_addr_list[0])
            host_dest_addr.sin_addr = *(struct in_addr*)he->h_addr_list[0];
        else
        {
            printf("I_Net_JoinGame: Could not resolve host IP: %s\n", host_ip);
            closesocket(net_socket);
            net_socket = INVALID_SOCKET;
            return false;
        }
    }

    strncpy(client_host_ip_str, host_ip, sizeof(client_host_ip_str) - 1);
    client_host_port_val = port;
    client_assigned_node = -1;
    current_net_state = NET_STATE_CLIENT_LOBBY;
    last_hello_time = 0;

    sprintf(status_message, "CONNECTING TO %s:%d...", host_ip, port);
    printf("I_Net_JoinGame: Connecting to %s:%d...\n", host_ip, port);
    return true;
}

void I_Net_CancelLobby(void)
{
    if (net_socket != INVALID_SOCKET)
    {
        closesocket(net_socket);
        net_socket = INVALID_SOCKET;
    }
    current_net_state = NET_STATE_OFFLINE;
    connected_players = 1;
}

int I_Net_HostLobbyTicker(void)
{
    if (current_net_state != NET_STATE_HOST_LOBBY || net_socket == INVALID_SOCKET)
        return connected_players;

    char buf[512];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    int len;
    while ((len = recvfrom(net_socket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &from_len)) > 0)
    {
        if (len < (int)sizeof(net_pkt_connect_req_t))
            continue;

        uint32_t magic;
        memcpy(&magic, buf, sizeof(magic));
        if (magic != NET_LOBBY_MAGIC)
            continue;

        uint8_t type = buf[4];
        if (type == PKT_CONNECT_REQ)
        {
            // Check if this client IP/port is already registered
            int node_idx = -1;
            for (int i = 1; i < connected_players; i++)
            {
                if (peer_addrs[i].sin_addr.s_addr == from.sin_addr.s_addr &&
                    peer_addrs[i].sin_port == from.sin_port)
                {
                    node_idx = i;
                    break;
                }
            }

            if (node_idx == -1 && connected_players < host_target_players)
            {
                node_idx = connected_players++;
                peer_addrs[node_idx] = from;
                char* ip = inet_ntoa(from.sin_addr);
                snprintf(peer_ip_strings[node_idx], sizeof(peer_ip_strings[node_idx]), "%s:%d",
                         ip ? ip : "UNKNOWN", ntohs(from.sin_port));
                printf("I_Net_HostLobbyTicker: Player %d joined from %s\n",
                       node_idx + 1, peer_ip_strings[node_idx]);
            }

            if (node_idx != -1)
            {
                // Send CONNECT_ACK
                net_pkt_connect_ack_t ack;
                ack.magic = NET_LOBBY_MAGIC;
                ack.type = PKT_CONNECT_ACK;
                ack.assigned_node = (uint8_t)node_idx;
                ack.total_players = (uint8_t)host_target_players;
                ack.gamemode = (uint8_t)host_deathmatch_val;
                ack.episode = (uint8_t)host_episode_val;
                ack.map = (uint8_t)host_map_val;
                ack.skill = (uint8_t)host_skill_val;

                sendto(net_socket, (const char*)&ack, sizeof(ack), 0, (struct sockaddr*)&from, sizeof(from));
            }
        }
    }

    if (connected_players >= host_target_players)
    {
        sprintf(status_message, "ALL PLAYERS CONNECTED (%d/%d)! PRESS ENTER TO START",
                connected_players, host_target_players);
    }
    else
    {
        sprintf(status_message, "WAITING FOR PLAYERS (%d/%d)...",
                connected_players, host_target_players);
    }

    return connected_players;
}

boolean I_Net_ClientLobbyTicker(void)
{
    if (current_net_state != NET_STATE_CLIENT_LOBBY || net_socket == INVALID_SOCKET)
        return false;

    int now = I_GetTime();
    if (now - last_hello_time > 15 || last_hello_time == 0) // Every ~400ms
    {
        net_pkt_connect_req_t req;
        req.magic = NET_LOBBY_MAGIC;
        req.type = PKT_CONNECT_REQ;
        req.client_node = 0;
        strncpy(req.player_name, "PLAYER", sizeof(req.player_name));

        sendto(net_socket, (const char*)&req, sizeof(req), 0,
               (struct sockaddr*)&host_dest_addr, sizeof(host_dest_addr));
        last_hello_time = now;
    }

    char buf[512];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    int len;
    while ((len = recvfrom(net_socket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &from_len)) > 0)
    {
        if (len < 5) continue;

        uint32_t magic;
        memcpy(&magic, buf, sizeof(magic));
        if (magic != NET_LOBBY_MAGIC) continue;

        uint8_t type = buf[4];
        if (type == PKT_CONNECT_ACK && len >= (int)sizeof(net_pkt_connect_ack_t))
        {
            net_pkt_connect_ack_t ack;
            memcpy(&ack, buf, sizeof(ack));
            client_assigned_node = ack.assigned_node;
            host_target_players = ack.total_players;
            sprintf(status_message, "CONNECTED AS PLAYER %d! WAITING FOR HOST TO START...",
                    client_assigned_node + 1);
        }
        else if (type == PKT_START_GAME && len >= (int)sizeof(net_pkt_start_game_t))
        {
            net_pkt_start_game_t start;
            memcpy(&start, buf, sizeof(start));

            client_game_skill = start.skill;
            client_game_episode = start.episode;
            client_game_map = start.map;
            client_game_deathmatch = start.gamemode;
            host_target_players = start.total_players;

            memset(peer_addrs, 0, sizeof(peer_addrs));
            for (int i = 0; i < start.total_players; i++)
            {
                peer_addrs[i].sin_family = AF_INET;
                peer_addrs[i].sin_addr.s_addr = start.peer_ip[i];
                peer_addrs[i].sin_port = start.peer_port[i];
            }

            // Node 0 is Host
            peer_addrs[0] = from;

            // Send acknowledge back
            net_pkt_start_ack_t s_ack;
            s_ack.magic = NET_LOBBY_MAGIC;
            s_ack.type = PKT_START_ACK;
            s_ack.node = (uint8_t)client_assigned_node;
            sendto(net_socket, (const char*)&s_ack, sizeof(s_ack), 0,
                   (struct sockaddr*)&host_dest_addr, sizeof(host_dest_addr));

            // Set up DOOM networking state
            current_net_state = NET_STATE_ACTIVE;
            netgame = true;
            deathmatch = client_game_deathmatch;
            startskill = client_game_skill;
            startepisode = client_game_episode;
            startmap = client_game_map;

            I_Net_Init();
            doomcom->consoleplayer = client_assigned_node;
            doomcom->numnodes = start.total_players;
            doomcom->numplayers = start.total_players;

            printf("I_Net_ClientLobbyTicker: Game starting! Node %d of %d, Map E%dM%d\n",
                   client_assigned_node + 1, start.total_players,
                   client_game_episode, client_game_map);
            return true;
        }
    }

    return false;
}

boolean I_Net_HostLaunch(void)
{
    if (current_net_state != NET_STATE_HOST_LOBBY || net_socket == INVALID_SOCKET)
        return false;

    if (connected_players < 2)
    {
        printf("I_Net_HostLaunch: Need at least 2 players to start multiplayer!\n");
        return false;
    }

    net_pkt_start_game_t start;
    start.magic = NET_LOBBY_MAGIC;
    start.type = PKT_START_GAME;
    start.total_players = (uint8_t)connected_players;
    start.gamemode = (uint8_t)host_deathmatch_val;
    start.episode = (uint8_t)host_episode_val;
    start.map = (uint8_t)host_map_val;
    start.skill = (uint8_t)host_skill_val;

    for (int i = 0; i < MAXNETNODES; i++)
    {
        if (i < connected_players)
        {
            start.peer_ip[i] = peer_addrs[i].sin_addr.s_addr;
            start.peer_port[i] = peer_addrs[i].sin_port;
        }
        else
        {
            start.peer_ip[i] = 0;
            start.peer_port[i] = 0;
        }
    }

    // Send START_GAME packet to all connected clients multiple times for reliability
    for (int retry = 0; retry < 5; retry++)
    {
        for (int i = 1; i < connected_players; i++)
        {
            sendto(net_socket, (const char*)&start, sizeof(start), 0,
                   (struct sockaddr*)&peer_addrs[i], sizeof(peer_addrs[i]));
        }
    }

    current_net_state = NET_STATE_ACTIVE;
    netgame = true;
    deathmatch = host_deathmatch_val;
    startskill = host_skill_val;
    startepisode = host_episode_val;
    startmap = host_map_val;

    I_Net_Init();
    doomcom->consoleplayer = 0;
    doomcom->numnodes = connected_players;
    doomcom->numplayers = connected_players;

    printf("I_Net_HostLaunch: Host started game! %d players, E%dM%d, Skill %d, DM %d\n",
           connected_players, host_episode_val, host_map_val, host_skill_val, host_deathmatch_val);
    return true;
}

//
// Standard DOOM Network Interface
//
void I_Net_Init(void)
{
    if (!doomcom)
    {
        doomcom = (doomcom_t*)malloc(sizeof(*doomcom));
    }
    memset(doomcom, 0, sizeof(*doomcom));

    doomcom->id = DOOMCOM_ID;
    doomcom->ticdup = 1;
    doomcom->extratics = 1;

    if (current_net_state == NET_STATE_ACTIVE)
    {
        doomcom->numplayers = (short)connected_players;
        doomcom->numnodes = (short)connected_players;
        doomcom->consoleplayer = (short)(client_assigned_node >= 0 ? client_assigned_node : 0);
        netgame = true;
    }
    else
    {
        doomcom->numplayers = 1;
        doomcom->numnodes = 1;
        doomcom->consoleplayer = 0;
        netgame = false;
    }
}

void I_Net_Cmd(void)
{
    if (net_socket == INVALID_SOCKET || !netgame)
    {
        if (doomcom->command == CMD_SEND)
            doomcom->remotenode = 0;
        else if (doomcom->command == CMD_GET)
            doomcom->remotenode = -1;
        return;
    }

    if (doomcom->command == CMD_SEND)
    {
        int target = doomcom->remotenode;
        if (target >= 0 && target < doomcom->numnodes && target != doomcom->consoleplayer)
        {
            sendto(net_socket, (const char*)&doomcom->data, doomcom->datalength, 0,
                   (struct sockaddr*)&peer_addrs[target], sizeof(peer_addrs[target]));
        }
    }
    else if (doomcom->command == CMD_GET)
    {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);

        int len = recvfrom(net_socket, (char*)&doomcom->data, sizeof(doomcom->data), 0,
                           (struct sockaddr*)&from, &from_len);

        if (len > 0)
        {
            int sender_node = -1;
            for (int i = 0; i < doomcom->numnodes; i++)
            {
                if (peer_addrs[i].sin_addr.s_addr == from.sin_addr.s_addr &&
                    peer_addrs[i].sin_port == from.sin_port)
                {
                    sender_node = i;
                    break;
                }
            }

            if (sender_node != -1)
            {
                doomcom->remotenode = (short)sender_node;
                doomcom->datalength = (short)len;
                return;
            }
        }

        doomcom->remotenode = -1;
    }
}

#else

//
// MS-DOS Stubs (DOS uses i_net_dos.c IPX driver)
//
#include "doomdef.h"
#include "d_net.h"
#include "i_net_ip.h"

boolean I_Net_InitSockets(void) { return false; }
void    I_Net_ShutdownSockets(void) {}
boolean I_Net_HostGame(int max_players, int port, int skill, int episode, int map, int dm) { return false; }
boolean I_Net_JoinGame(const char* host_ip, int port) { return false; }
void    I_Net_CancelLobby(void) {}
int     I_Net_HostLobbyTicker(void) { return 1; }
boolean I_Net_ClientLobbyTicker(void) { return false; }
boolean I_Net_HostLaunch(void) { return false; }
net_state_t I_Net_GetState(void) { return NET_STATE_OFFLINE; }
int     I_Net_GetConnectedCount(void) { return 1; }
int     I_Net_GetMaxPlayers(void) { return 1; }
const char* I_Net_GetHostStatusMsg(void) { return ""; }
const char* I_Net_GetClientStatusMsg(void) { return ""; }
const char* I_Net_GetLocalIP(void) { return "127.0.0.1"; }
const char* I_Net_GetClientNodeIP(int node) { return "127.0.0.1"; }
int     I_Net_GetClientNode(void) { return -1; }
int     I_Net_GetGameSkill(void) { return 2; }
int     I_Net_GetGameEpisode(void) { return 1; }
int     I_Net_GetGameMap(void) { return 1; }
int     I_Net_GetGameDeathmatch(void) { return 0; }
void    I_Net_Init(void) {}
void    I_Net_Cmd(void) {}

#endif
