// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	DOOM sound and music driver for MS-DOS (Sound Blaster / PC Speaker / OPL).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"
#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_argv.h"
#include "../common/i_sound_mixer.h"

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

    I_Mixer_Init();
    printf("I_InitSound: DOS Audio subsystem initialized.\n");
}

void I_UpdateSound(void)
{
    // Synchronous sound updates if needed
}

void I_SubmitSound(void)
{
    // Write mixed PCM buffer to sound card DMA
}

void I_ShutdownSound(void)
{
    I_Mixer_Shutdown();
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
// MUSIC
//

void I_InitMusic(void)
{
    if (M_CheckParm("-nomusic"))
        nomusic = true;
}

void I_ShutdownMusic(void)
{
}

void I_SetMusicVolume(int volume)
{
    music_vol = volume;
}

void I_PauseSong(int handle)
{
    (void)handle;
}

void I_ResumeSong(int handle)
{
    (void)handle;
}

int I_RegisterSong(void* data)
{
    (void)data;
    return 1;
}

void I_PlaySong(int handle, int looping)
{
    (void)handle;
    (void)looping;
}

void I_StopSong(int handle)
{
    (void)handle;
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
}

