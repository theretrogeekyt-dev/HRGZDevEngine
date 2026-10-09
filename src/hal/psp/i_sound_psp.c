// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// HRGZDevEngine DOOM for PlayStation Portable (PSP)
// Audio driver: Hardware audio channel streaming via libpspaudio, dedicated
// audio worker thread, and software multichannel sound effects & music mixing.
//
// Uses the official PSPDEV SDK (https://pspdev.github.io)
//
//-----------------------------------------------------------------------------

#include <pspkernel.h>
#include <pspaudio.h>
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

#define PSP_AUDIO_SAMPLES 1024

static int audio_channel = -1;
static SceUID audio_thread_id = -1;
static volatile boolean audio_running = false;
static boolean nosound = false;
static boolean nomusic = false;
static int music_vol = 15;

// Audio worker thread: continuously mixes audio and submits to PSP hardware
static int AudioThread(SceSize args, void* argp)
{
    (void)args; (void)argp;
    // 64-byte alignment required by PSP hardware DMA and libpspaudio
    __attribute__((aligned(64))) static int16_t stream_buf[PSP_AUDIO_SAMPLES * 2];

    while (audio_running)
    {
        // Mix next 1024 stereo samples (23.2ms of audio at 44100Hz)
        I_Mixer_Mix(stream_buf, PSP_AUDIO_SAMPLES);

        // Blocking output sends samples directly to PSP DAC / headphone jack
        if (audio_channel >= 0)
        {
            int ret = sceAudioOutputBlocking(audio_channel, PSP_AUDIO_VOLUME_MAX, stream_buf);
            if (ret < 0)
            {
                // Fallback sleep if hardware audio output failed so we don't spin-lock
                sceKernelDelayThread(10000);
            }
        }
        else
        {
            sceKernelDelayThread(10000);
        }
    }

    return 0;
}

void I_InitSound(void)
{
    if (M_CheckParm("-nosound"))
    {
        nosound = true;
        nomusic = true;
        return;
    }

    // Initialize software mixer at Sony PSP hardware native 44,100 Hz
    I_Mixer_InitRate(44100);

    // Reserve hardware audio channel
    audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, PSP_AUDIO_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
    if (audio_channel < 0)
    {
        fprintf(stderr, "PSP Audio: Failed to reserve hardware channel (%d)\n", audio_channel);
        return;
    }

    // Spawn dedicated audio mixing thread at high priority 0x12 (above main thread 0x18 to prevent pops)
    audio_running = true;
    audio_thread_id = sceKernelCreateThread("doom_audio_thread", AudioThread, 0x12, 0x10000, 0, NULL);
    if (audio_thread_id >= 0)
    {
        sceKernelStartThread(audio_thread_id, 0, NULL);
    }
    else
    {
        fprintf(stderr, "PSP Audio: Failed to start audio thread\n");
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
    if (audio_running)
    {
        audio_running = false;
        if (audio_thread_id >= 0)
        {
            sceKernelWaitThreadEnd(audio_thread_id, NULL);
            sceKernelDeleteThread(audio_thread_id);
            audio_thread_id = -1;
        }
    }

    if (audio_channel >= 0)
    {
        sceAudioChRelease(audio_channel);
        audio_channel = -1;
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
    return W_CheckNumForName(name);
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    if (nosound || id <= 0 || id >= NUMSFX)
        return 0;

    sfxinfo_t* sfx = &S_sfx[id];
    if (sfx->lumpnum < 0)
        sfx->lumpnum = I_GetSfxLumpNum(sfx);

    if (sfx->lumpnum < 0)
        return 0;

    int sfx_len = W_LumpLength(sfx->lumpnum);
    const uint8_t* sfx_data = (const uint8_t*)W_CacheLumpNum(sfx->lumpnum, PU_CACHE);

    return I_Mixer_StartSound(sfx_data, sfx_len, vol, sep, pitch, priority);
}

void I_StopSound(int handle)
{
    I_Mixer_StopSound(handle);
}

int I_SoundIsPlaying(int handle)
{
    return I_Mixer_SoundIsPlaying(handle);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
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
    (void)handle; (void)looping;
}

void I_StopSong(int handle)
{
    (void)handle;
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
}

