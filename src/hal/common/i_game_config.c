// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM Game Distribution System
// Game Configuration, Branding & Auto-Discovery Subsystem
//
//-----------------------------------------------------------------------------

#include "i_game_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#define mkdir_compat(p) _mkdir(p)
#else
#include <unistd.h>
#define mkdir_compat(p) mkdir(p, 0755)
#endif

static char s_game_title[256] = "HRGZDevEngine DOOM";
static char s_game_id[128] = "hrgzdoom";
static char s_game_mode[64] = "";
static char s_game_wad[512] = "";
static char s_save_dir[512] = "";
static bool s_initialized = false;

static bool FileExists(const char* path)
{
    if (!path || path[0] == '\0')
        return false;
    FILE* f = fopen(path, "rb");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

static void ExtractJsonString(const char* json, const char* key, char* out, size_t out_max)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char* p = strstr(json, search);
    if (!p)
        return;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':'))
        p++;
    if (*p == '\"')
    {
        p++;
        size_t idx = 0;
        while (*p && *p != '\"' && idx < out_max - 1)
        {
            if (*p == '\\' && *(p + 1))
                p++;
            out[idx++] = *p++;
        }
        out[idx] = '\0';
    }
}

static void LoadManifest(const char* manifest_path)
{
    FILE* f = fopen(manifest_path, "rb");
    if (!f)
        return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz > 0 && sz < 65536)
    {
        char* buf = (char*)malloc(sz + 1);
        if (buf)
        {
            size_t read_bytes = fread(buf, 1, sz, f);
            buf[read_bytes] = '\0';

            char temp[256];
            temp[0] = '\0';
            ExtractJsonString(buf, "title", temp, sizeof(temp));
            if (temp[0] != '\0')
                strncpy(s_game_title, temp, sizeof(s_game_title) - 1);

            temp[0] = '\0';
            ExtractJsonString(buf, "id", temp, sizeof(temp));
            if (temp[0] != '\0')
                strncpy(s_game_id, temp, sizeof(s_game_id) - 1);

            temp[0] = '\0';
            ExtractJsonString(buf, "wadPath", temp, sizeof(temp));
            if (temp[0] != '\0' && FileExists(temp))
                strncpy(s_game_wad, temp, sizeof(s_game_wad) - 1);

            temp[0] = '\0';
            ExtractJsonString(buf, "wad", temp, sizeof(temp));
            if (temp[0] != '\0' && FileExists(temp))
                strncpy(s_game_wad, temp, sizeof(s_game_wad) - 1);

            temp[0] = '\0';
            ExtractJsonString(buf, "gameMode", temp, sizeof(temp));
            if (temp[0] != '\0')
                strncpy(s_game_mode, temp, sizeof(s_game_mode) - 1);
            else
            {
                ExtractJsonString(buf, "gamemode", temp, sizeof(temp));
                if (temp[0] != '\0')
                    strncpy(s_game_mode, temp, sizeof(s_game_mode) - 1);
            }

            free(buf);
        }
    }
    fclose(f);
}

void I_InitGameConfig(void)
{
    if (s_initialized)
        return;
    s_initialized = true;

#ifdef HRGZ_GAME_TITLE
    strncpy(s_game_title, HRGZ_GAME_TITLE, sizeof(s_game_title) - 1);
#endif

#ifdef HRGZ_GAME_ID
    strncpy(s_game_id, HRGZ_GAME_ID, sizeof(s_game_id) - 1);
#endif

    // 1. Look for game.json in DOOMWADDIR if set
    const char* env_dir = getenv("DOOMWADDIR");
    if (env_dir && env_dir[0] != '\0')
    {
        char temp_path[512];
        snprintf(temp_path, sizeof(temp_path), "%s/game.json", env_dir);
        if (FileExists(temp_path))
            LoadManifest(temp_path);
        
        if (s_game_wad[0] == '\0')
        {
            snprintf(temp_path, sizeof(temp_path), "%s/game.wad", env_dir);
            if (FileExists(temp_path))
                strncpy(s_game_wad, temp_path, sizeof(s_game_wad) - 1);
            else
            {
                snprintf(temp_path, sizeof(temp_path), "%s/GAME.WAD", env_dir);
                if (FileExists(temp_path))
                    strncpy(s_game_wad, temp_path, sizeof(s_game_wad) - 1);
            }
        }
    }

    // 2. Look for game.json manifest in current directory and parent resources
    if (FileExists("game.json"))
        LoadManifest("game.json");
    else if (FileExists("../Resources/game.json"))
        LoadManifest("../Resources/game.json");
    else if (FileExists("hrgz.json"))
        LoadManifest("hrgz.json");

    // 3. Auto-discover bundled game.wad if not yet resolved
    if (s_game_wad[0] == '\0')
    {
#ifdef HRGZ_GAME_WAD
        if (FileExists(HRGZ_GAME_WAD))
            strncpy(s_game_wad, HRGZ_GAME_WAD, sizeof(s_game_wad) - 1);
#endif
        if (s_game_wad[0] == '\0')
        {
            if (FileExists("game.wad"))
                strncpy(s_game_wad, "game.wad", sizeof(s_game_wad) - 1);
            else if (FileExists("GAME.WAD"))
                strncpy(s_game_wad, "GAME.WAD", sizeof(s_game_wad) - 1);
            else if (FileExists("../Resources/game.wad"))
                strncpy(s_game_wad, "../Resources/game.wad", sizeof(s_game_wad) - 1);
            else if (FileExists("../Resources/GAME.WAD"))
                strncpy(s_game_wad, "../Resources/GAME.WAD", sizeof(s_game_wad) - 1);
            else if (FileExists("project.wad"))
                strncpy(s_game_wad, "project.wad", sizeof(s_game_wad) - 1);
            else if (FileExists("doom1.wad"))
                strncpy(s_game_wad, "doom1.wad", sizeof(s_game_wad) - 1);
            else if (FileExists("DOOM1.WAD"))
                strncpy(s_game_wad, "DOOM1.WAD", sizeof(s_game_wad) - 1);
            else if (FileExists("../Resources/doom1.wad"))
                strncpy(s_game_wad, "../Resources/doom1.wad", sizeof(s_game_wad) - 1);
        }
    }

    // 3. Setup application data / save directory
#if defined(_WIN32) || defined(__WIN32__)
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK)
    {
        snprintf(s_save_dir, sizeof(s_save_dir), "%s\\HRGZDevEngine", appdata);
        mkdir_compat(s_save_dir);
        snprintf(s_save_dir, sizeof(s_save_dir), "%s\\HRGZDevEngine\\%s", appdata, s_game_id);
        mkdir_compat(s_save_dir);
    }
    else
    {
        strcpy(s_save_dir, ".");
    }
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home)
    {
        snprintf(s_save_dir, sizeof(s_save_dir), "%s/Library/Application Support/HRGZDevEngine", home);
        mkdir_compat(s_save_dir);
        snprintf(s_save_dir, sizeof(s_save_dir), "%s/Library/Application Support/HRGZDevEngine/%s", home, s_game_id);
        mkdir_compat(s_save_dir);
    }
    else
    {
        strcpy(s_save_dir, ".");
    }
#else
    const char* xdg = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");
    if (xdg && xdg[0] != '\0')
    {
        snprintf(s_save_dir, sizeof(s_save_dir), "%s/HRGZDevEngine/%s", xdg, s_game_id);
        mkdir_compat(s_save_dir);
    }
    else if (home)
    {
        snprintf(s_save_dir, sizeof(s_save_dir), "%s/.local/share/HRGZDevEngine", home);
        mkdir_compat(s_save_dir);
        snprintf(s_save_dir, sizeof(s_save_dir), "%s/.local/share/HRGZDevEngine/%s", home, s_game_id);
        mkdir_compat(s_save_dir);
    }
    else
    {
        strcpy(s_save_dir, ".");
    }
#endif
}

const char* I_GetGameTitle(void)
{
    if (!s_initialized)
        I_InitGameConfig();
    return s_game_title;
}

const char* I_GetGameId(void)
{
    if (!s_initialized)
        I_InitGameConfig();
    return s_game_id;
}

const char* I_GetGameWadPath(void)
{
    if (!s_initialized)
        I_InitGameConfig();
    return s_game_wad[0] != '\0' ? s_game_wad : NULL;
}

const char* I_GetGameModeString(void)
{
    if (!s_initialized)
        I_InitGameConfig();
    return s_game_mode[0] != '\0' ? s_game_mode : NULL;
}

const char* I_GetSaveDir(void)
{
    if (!s_initialized)
        I_InitGameConfig();
    return s_save_dir[0] != '\0' ? s_save_dir : ".";
}
