// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Native Windows sound and music driver (WinMM waveOut + WinMM General MIDI).
//	Zero external DLLs required! Uses built-in Microsoft GS Wavetable Synth.
//
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
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
#include "../common/i_mus2midi.h"

#define AUDIO_BUF_SAMPLES 1024
#define NUM_AUDIO_BUFS    2

#if defined(_WIN32)
static HWAVEOUT  h_wave_out = NULL;
static WAVEHDR   wave_headers[NUM_AUDIO_BUFS];
static int16_t   wave_buffers[NUM_AUDIO_BUFS][AUDIO_BUF_SAMPLES * 2];
static int       current_buf = 0;
static HMIDIOUT  h_midi_out = NULL;
#endif

static boolean   sound_inited = false;
static boolean   nosound = false;
static boolean   nomusic = false;
static int       music_volume = 15;

void I_InitSound(void)
{
    if (M_CheckParm("-nosound"))
    {
        nosound = true;
        nomusic = true;
        return;
    }

    I_Mixer_Init();

#if defined(_WIN32)
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2; // Stereo
    wfx.nSamplesPerSec = MIXER_SAMPLE_RATE; // 11025 Hz
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&h_wave_out, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR)
    {
        for (int i = 0; i < NUM_AUDIO_BUFS; i++)
        {
            memset(&wave_headers[i], 0, sizeof(WAVEHDR));
            wave_headers[i].lpData = (LPSTR)wave_buffers[i];
            wave_headers[i].dwBufferLength = AUDIO_BUF_SAMPLES * 2 * sizeof(int16_t);
            waveOutPrepareHeader(h_wave_out, &wave_headers[i], sizeof(WAVEHDR));
            wave_headers[i].dwFlags |= WHDR_DONE;
        }
        sound_inited = true;
    }
#endif

    I_InitMusic();
}

void I_UpdateSound(void)
{
}

void I_SubmitSound(void)
{
#if defined(_WIN32)
    if (!sound_inited || !h_wave_out)
        return;

    WAVEHDR* hdr = &wave_headers[current_buf];
    if (hdr->dwFlags & WHDR_DONE)
    {
        I_Mixer_Mix(wave_buffers[current_buf], AUDIO_BUF_SAMPLES);
        hdr->dwFlags &= ~WHDR_DONE;
        waveOutWrite(h_wave_out, hdr, sizeof(WAVEHDR));
        current_buf = (current_buf + 1) % NUM_AUDIO_BUFS;
    }
#endif
}

void I_ShutdownSound(void)
{
#if defined(_WIN32)
    if (h_wave_out)
    {
        waveOutReset(h_wave_out);
        for (int i = 0; i < NUM_AUDIO_BUFS; i++)
        {
            waveOutUnprepareHeader(h_wave_out, &wave_headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(h_wave_out);
        h_wave_out = NULL;
    }
#endif
    I_ShutdownMusic();
    I_Mixer_Shutdown();
    sound_inited = false;
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
// MUSIC (WinMM Native MIDI)
//

void I_InitMusic(void)
{
    if (M_CheckParm("-nomusic"))
    {
        nomusic = true;
        return;
    }

#if defined(_WIN32)
    if (midiOutOpen(&h_midi_out, MIDI_MAPPER, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        h_midi_out = NULL;
    }
#endif
}

void I_ShutdownMusic(void)
{
#if defined(_WIN32)
    if (h_midi_out)
    {
        midiOutReset(h_midi_out);
        midiOutClose(h_midi_out);
        h_midi_out = NULL;
    }
#endif
}

void I_SetMusicVolume(int volume)
{
    music_volume = volume;
    I_Mixer_SetMasterVolume(volume);
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
#if defined(_WIN32)
    if (h_midi_out)
    {
        midiOutReset(h_midi_out);
    }
#endif
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
}

