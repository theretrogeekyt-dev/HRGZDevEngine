// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DOOM sound and music driver for MS-DOS (Sound Blaster DMA SFX + AdLib OPL FM).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"
#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_argv.h"
#include "../common/i_sound_mixer.h"
#include "i_sb_dos.h"
#include "i_opl_dos.h"

static boolean nosound = false;
static boolean nomusic = false;
static int music_vol = 15;

void I_InitSound(void)
{
    if (M_CheckParm("-nosound"))
    {
        nosound = true;
        nomusic = true;
        return;
    }

    if (M_CheckParm("-nosfx"))
    {
        nosound = true;
    }

    if (!nosound)
    {
        I_Mixer_Init();
        if (I_SB_Init())
        {
            printf("I_InitSound: Sound Blaster digital sound FX active.\n");
        }
        else
        {
            printf("I_InitSound: Sound Blaster not found. Digital sound FX disabled.\n");
        }
    }

    I_InitMusic();
}

#if defined(__DJGPP__)
static uclock_t last_uclock = 0;
static uclock_t uclock_accum = 0;
// 1,193,180 / 140 = 8522.714
#define UCLOCKS_PER_140HZ (UCLOCKS_PER_SEC / 140)
#else
static int last_music_tic = -1;
#endif

void I_UpdateSound(void)
{
    // Advance AdLib FM music sequencer at authentic 140Hz MUS tempo
    if (!nomusic)
    {
#if defined(__DJGPP__)
        uclock_t cur_uclock = uclock();
        if (last_uclock == 0)
        {
            last_uclock = cur_uclock;
            return;
        }

        uclock_t elapsed = cur_uclock - last_uclock;
        last_uclock = cur_uclock;

        uclock_accum += elapsed;
        int tics = (int)(uclock_accum / UCLOCKS_PER_140HZ);
        if (tics > 0)
        {
            uclock_accum %= UCLOCKS_PER_140HZ;
            if (tics > 28) tics = 28; // Cap at ~200ms in case of frame stall
            I_OPL_Update(tics);
        }
#else
        int cur_tic = I_GetTime();
        if (last_music_tic < 0)
        {
            last_music_tic = cur_tic;
        }
        int delta = cur_tic - last_music_tic;
        if (delta > 0)
        {
            if (delta > 35) delta = 35; // Clamp in case of long stall
            I_OPL_Update(delta * 4);
            last_music_tic = cur_tic;
        }
#endif
    }
}

void I_SubmitSound(void)
{
    // Stream Sound Blaster DMA double-buffer
    if (!nosound)
    {
        I_SB_Update();
    }
}

void I_ShutdownSound(void)
{
    I_SB_Shutdown();
    I_Mixer_Shutdown();
    I_OPL_Shutdown();
}

void I_SetChannels(void)
{
}

void I_SetSfxVolume(int volume)
{
    I_Mixer_SetMasterVolume(volume);
}

int I_GetSfxLumpNum(sfxinfo_t* sfxinfo)
{
    char name[9];
    sprintf(name, "ds%s", sfxinfo->name);
    return W_GetNumForName(name);
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    if (nosound || id <= 0 || id >= NUMSFX)
        return 0;

    sfxinfo_t* sfx = &S_sfx[id];
    if (!sfx->data)
    {
        int lump = I_GetSfxLumpNum(sfx);
        sfx->data = (void*)W_CacheLumpNum(lump, PU_STATIC);
    }

    int lump = I_GetSfxLumpNum(sfx);
    size_t len = W_LumpLength(lump);

    return I_Mixer_StartSound((const uint8_t*)sfx->data, len, vol, sep, pitch, priority);
}

void I_StopSound(int handle)
{
    if (!nosound)
        I_Mixer_StopSound(handle);
}

int I_SoundIsPlaying(int handle)
{
    if (nosound) return 0;
    return I_Mixer_SoundIsPlaying(handle);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    if (!nosound)
        I_Mixer_UpdateSoundParams(handle, vol, sep, pitch);
}

//
// MUSIC (AdLib / OPL2 / OPL3 FM Synthesis)
//

void I_InitMusic(void)
{
    if (M_CheckParm("-nomusic") || M_CheckParm("-nosound"))
    {
        nomusic = true;
        return;
    }

    if (I_OPL_Init())
    {
        printf("I_InitMusic: AdLib / OPL2 FM music subsystem active.\n");
        I_OPL_SetMusicVolume(music_vol);
    }
    else
    {
        nomusic = true;
        printf("I_InitMusic: AdLib / OPL not found. Music disabled.\n");
    }
}

void I_ShutdownMusic(void)
{
    I_OPL_Shutdown();
}

void I_SetMusicVolume(int volume)
{
    music_vol = volume;
    if (!nomusic)
    {
        I_OPL_SetMusicVolume(volume);
    }
}

void I_PauseSong(int handle)
{
    if (!nomusic)
    {
#if defined(__DJGPP__)
        last_uclock = 0;
        uclock_accum = 0;
#endif
        I_OPL_PauseSong(handle);
    }
}

void I_ResumeSong(int handle)
{
    if (!nomusic)
    {
#if defined(__DJGPP__)
        last_uclock = 0;
        uclock_accum = 0;
#endif
        I_OPL_ResumeSong(handle);
    }
}

int I_RegisterSong(void* data)
{
    if (nomusic || !data)
        return 0;

    return I_OPL_RegisterSong(data, 0);
}

void I_PlaySong(int handle, int looping)
{
    if (!nomusic && handle)
    {
#if defined(__DJGPP__)
        last_uclock = 0;
        uclock_accum = 0;
#else
        last_music_tic = -1;
#endif
        I_OPL_PlaySong(handle, looping);
    }
}

void I_StopSong(int handle)
{
    if (!nomusic)
    {
        I_OPL_StopSong(handle);
    }
}

void I_UnRegisterSong(int handle)
{
    if (!nomusic)
    {
        I_OPL_UnRegisterSong(handle);
    }
}
