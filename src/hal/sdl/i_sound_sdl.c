// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Audio driver for Modern SDL2 (SDL audio callback with multichannel mixer).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"
#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_argv.h"
#include "../common/i_sound_mixer.h"

static SDL_AudioDeviceID audio_device = 0;
static boolean nosound = false;
static boolean nomusic = false;
static int music_vol = 15;

static void SDLAudioCallback(void* userdata, Uint8* stream, int len)
{
    (void)userdata;
    int samples_to_mix = len / (2 * sizeof(int16_t));
    I_Mixer_Mix((int16_t*)stream, samples_to_mix);
}

void I_InitSound(void)
{
    if (M_CheckParm("-nosound"))
    {
        nosound = true;
        nomusic = true;
        return;
    }

    I_Mixer_Init();

    SDL_AudioSpec wanted, obtained;
    memset(&wanted, 0, sizeof(wanted));
    wanted.freq = MIXER_SAMPLE_RATE;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    wanted.callback = SDLAudioCallback;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (audio_device != 0)
    {
        SDL_PauseAudioDevice(audio_device, 0); // Start playback
    }
}

void I_UpdateSound(void)
{
}

void I_SubmitSound(void)
{
}

void I_ShutdownSound(void)
{
    if (audio_device != 0)
    {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
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
    I_Mixer_SetMasterVolume(volume);
}

void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
int  I_RegisterSong(void* data) { (void)data; return 1; }
void I_PlaySong(int handle, int looping) { (void)handle; (void)looping; }
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }

