#ifndef __I_SOUND_MIXER_H__
#define __I_SOUND_MIXER_H__

#include <stdint.h>
#include <stddef.h>
#include "doomtype.h"
#include "sounds.h"

#define MIXER_MAX_CHANNELS 16
#define MIXER_SAMPLE_RATE  11025

#pragma pack(push, 1)
typedef struct
{
    uint16_t format;
    uint16_t samplerate;
    uint32_t samplecount;
} doom_sfx_header_t;
#pragma pack(pop)

typedef struct
{
    const uint8_t* data;
    uint32_t       length;
    uint32_t       position; // 16.16 fixed point
    uint32_t       step;     // 16.16 fixed point
    int            vol_left;
    int            vol_right;
    int            handle;
    int            priority;
    boolean        active;
} mixer_channel_t;

void I_Mixer_Init(void);
void I_Mixer_Shutdown(void);
int  I_Mixer_StartSound(const uint8_t* sfx_data, size_t sfx_len, int vol, int sep, int pitch, int priority);
void I_Mixer_StopSound(int handle);
int  I_Mixer_SoundIsPlaying(int handle);
void I_Mixer_UpdateSoundParams(int handle, int vol, int sep, int pitch);
void I_Mixer_SetMasterVolume(int volume); // 0 to 15
void I_Mixer_Mix(int16_t* output_buffer, int samples_to_mix); // interleaved stereo 16-bit
void I_Mixer_Mix8(uint8_t* output_buffer, int samples_to_mix); // mono 8-bit unsigned (128 = silence)

#endif // __I_SOUND_MIXER_H__

