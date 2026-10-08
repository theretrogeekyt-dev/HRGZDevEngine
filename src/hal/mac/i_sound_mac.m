// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Native macOS sound and music driver.
//	Uses AudioQueue for 16-bit 11025 Hz multichannel sound effects,
//	and AudioToolbox MusicPlayer/MusicSequence for built-in General MIDI playback.
//	Zero external library dependencies!
//
//-----------------------------------------------------------------------------

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreFoundation/CoreFoundation.h>

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
#define NUM_AUDIO_BUFS    3

static AudioQueueRef       audio_queue = NULL;
static AudioQueueBufferRef audio_buffers[NUM_AUDIO_BUFS];
static boolean             sound_inited = false;
static boolean             nosound = false;
static boolean             nomusic = false;
static int                 music_volume = 15;

static MusicPlayer         music_player = NULL;
static MusicSequence       current_sequence = NULL;

static void AudioQueueCallback(void *custom_data, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
    (void)custom_data;
    int samples_to_mix = buffer->mAudioDataBytesCapacity / (sizeof(int16_t) * 2);
    I_Mixer_Mix((int16_t *)buffer->mAudioData, samples_to_mix);
    buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
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

    AudioStreamBasicDescription desc;
    memset(&desc, 0, sizeof(desc));
    desc.mSampleRate = (Float64)MIXER_SAMPLE_RATE; // 11025 Hz
    desc.mFormatID = kAudioFormatLinearPCM;
    desc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    desc.mBitsPerChannel = 16;
    desc.mChannelsPerFrame = 2; // Stereo
    desc.mFramesPerPacket = 1;
    desc.mBytesPerFrame = 4; // 2 channels * 2 bytes
    desc.mBytesPerPacket = 4;

    OSStatus status = AudioQueueNewOutput(&desc, AudioQueueCallback, NULL, NULL, NULL, 0, &audio_queue);
    if (status == noErr)
    {
        UInt32 buffer_size = AUDIO_BUF_SAMPLES * 2 * sizeof(int16_t);
        for (int i = 0; i < NUM_AUDIO_BUFS; i++)
        {
            AudioQueueAllocateBuffer(audio_queue, buffer_size, &audio_buffers[i]);
            AudioQueueCallback(NULL, audio_queue, audio_buffers[i]);
        }
        AudioQueueStart(audio_queue, NULL);
        sound_inited = true;
        printf("I_InitSound: macOS AudioQueue output started (%d Hz stereo)\n", MIXER_SAMPLE_RATE);
    }
    else
    {
        printf("I_InitSound: AudioQueueNewOutput failed with status %d\n", (int)status);
    }

    I_InitMusic();
}

void I_UpdateSound(void)
{
}

void I_SubmitSound(void)
{
}

void I_ShutdownSound(void)
{
    if (sound_inited && audio_queue)
    {
        AudioQueueStop(audio_queue, true);
        AudioQueueDispose(audio_queue, true);
        audio_queue = NULL;
        sound_inited = false;
    }
    I_ShutdownMusic();
}

void I_SetSfxVolume(int volume)
{
    I_Mixer_SetMasterVolume(volume);
}

void I_SetChannels(void)
{
}

int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    if (nosound || !sound_inited)
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
// MUSIC (Native AudioToolbox General MIDI)
//

void I_InitMusic(void)
{
    if (M_CheckParm("-nomusic"))
    {
        nomusic = true;
        return;
    }

    if (NewMusicPlayer(&music_player) != noErr)
    {
        music_player = NULL;
    }
    else
    {
        printf("I_InitMusic: macOS AudioToolbox General MIDI player initialized.\n");
    }
}

void I_ShutdownMusic(void)
{
    if (music_player)
    {
        MusicPlayerStop(music_player);
        DisposeMusicPlayer(music_player);
        music_player = NULL;
    }
    if (current_sequence)
    {
        DisposeMusicSequence(current_sequence);
        current_sequence = NULL;
    }
}

void I_SetMusicVolume(int volume)
{
    music_volume = volume;
    if (current_sequence)
    {
        AUGraph graph = NULL;
        MusicSequenceGetAUGraph(current_sequence, &graph);
        if (graph)
        {
            AUNode outNode;
            if (AUGraphGetIndNode(graph, 1, &outNode) == noErr)
            {
                AudioUnit outUnit;
                if (AUGraphNodeInfo(graph, outNode, NULL, &outUnit) == noErr)
                {
                    // Scale 0..15 volume to linear gain 0.0 .. 0.70f for balanced levels
                    Float32 vol_scalar = ((Float32)volume / 15.0f) * 0.70f;
                    if (vol_scalar < 0.0f) vol_scalar = 0.0f;
                    if (vol_scalar > 1.0f) vol_scalar = 1.0f;
                    AudioUnitSetParameter(outUnit, 14, kAudioUnitScope_Global, 0, vol_scalar, 0);
                }
            }
        }
    }
}

void I_PauseSong(int handle)
{
    (void)handle;
    if (music_player)
        MusicPlayerStop(music_player);
}

void I_ResumeSong(int handle)
{
    (void)handle;
    if (music_player)
        MusicPlayerStart(music_player);
}

int I_RegisterSong(void* data)
{
    if (nomusic || !music_player || !data)
        return 1;

    size_t midi_len = 0;
    uint8_t* midi_data = I_MusToMidi((const uint8_t*)data, 65536, &midi_len);
    if (midi_data && midi_len > 0)
    {
        if (current_sequence)
        {
            MusicPlayerStop(music_player);
            DisposeMusicSequence(current_sequence);
            current_sequence = NULL;
        }

        if (NewMusicSequence(&current_sequence) == noErr)
        {
            CFDataRef cf_data = CFDataCreate(kCFAllocatorDefault, midi_data, (CFIndex)midi_len);
            if (cf_data)
            {
                MusicSequenceFileLoadData(current_sequence, cf_data, kMusicSequenceFile_MIDIType, 0);
                CFRelease(cf_data);

                AUGraph graph = NULL;
                MusicSequenceGetAUGraph(current_sequence, &graph);
                if (graph)
                {
                    AUGraphOpen(graph);
                    AUGraphInitialize(graph);
                    AUGraphStart(graph);
                }

                MusicPlayerSetSequence(music_player, current_sequence);
            }
        }
        free(midi_data);
    }

    return 1;
}

void I_PlaySong(int handle, int looping)
{
    (void)handle;
    if (nomusic || !music_player || !current_sequence)
        return;

    AUGraph graph = NULL;
    MusicSequenceGetAUGraph(current_sequence, &graph);
    if (graph)
    {
        Boolean isRunning = false;
        AUGraphIsRunning(graph, &isRunning);
        if (!isRunning)
        {
            AUGraphStart(graph);
        }
    }

    if (looping)
    {
        UInt32 numTracks = 0;
        MusicSequenceGetTrackCount(current_sequence, &numTracks);
        for (UInt32 i = 0; i < numTracks; i++)
        {
            MusicTrack track;
            MusicSequenceGetIndTrack(current_sequence, i, &track);
            MusicTimeStamp trackLength = 0;
            UInt32 propSize = sizeof(trackLength);
            MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLength, &propSize);

            MusicTrackLoopInfo loopInfo;
            loopInfo.loopDuration = trackLength;
            loopInfo.numberOfLoops = 0; // Infinitely loop
            MusicTrackSetProperty(track, kSequenceTrackProperty_LoopInfo, &loopInfo, sizeof(loopInfo));
        }
    }

    MusicPlayerSetTime(music_player, 0);
    I_SetMusicVolume(music_volume);
    MusicPlayerStart(music_player);
}

void I_StopSong(int handle)
{
    (void)handle;
    if (music_player)
        MusicPlayerStop(music_player);
}

void I_UnRegisterSong(int handle)
{
    (void)handle;
    if (current_sequence)
    {
        if (music_player)
            MusicPlayerStop(music_player);
        DisposeMusicSequence(current_sequence);
        current_sequence = NULL;
    }
}

int I_QrySongPlaying(int handle)
{
    (void)handle;
    if (!music_player) return 0;
    Boolean is_playing = false;
    MusicPlayerIsPlaying(music_player, &is_playing);
    return is_playing ? 1 : 0;
}
