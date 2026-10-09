#include <string.h>
#include <stdlib.h>
#include "i_sound_mixer.h"
#include "m_swap.h"

static mixer_channel_t channels[MIXER_MAX_CHANNELS];
static int next_handle = 1;
static int master_sfx_volume = 15; // 0..15
static int current_output_rate = MIXER_SAMPLE_RATE;

void I_Mixer_InitRate(int sample_rate)
{
    memset(channels, 0, sizeof(channels));
    next_handle = 1;
    if (sample_rate <= 0) sample_rate = MIXER_SAMPLE_RATE;
    current_output_rate = sample_rate;
}

void I_Mixer_Init(void)
{
    I_Mixer_InitRate(MIXER_SAMPLE_RATE);
}

void I_Mixer_Shutdown(void)
{
    for (int i = 0; i < MIXER_MAX_CHANNELS; i++)
    {
        channels[i].active = false;
    }
}

void I_Mixer_SetMasterVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 15) volume = 15;
    master_sfx_volume = volume;
}

int I_Mixer_StartSound(const uint8_t* sfx_data, size_t sfx_len, int vol, int sep, int pitch, int priority)
{
    if (!sfx_data || sfx_len < sizeof(doom_sfx_header_t))
        return 0;

    const doom_sfx_header_t* hdr = (const doom_sfx_header_t*)sfx_data;
    uint32_t sample_rate = (uint32_t)SHORT(hdr->samplerate);
    uint32_t sample_count = (uint32_t)LONG(hdr->samplecount);

    if (sample_count == 0 || sample_count + sizeof(doom_sfx_header_t) > sfx_len)
    {
        // Fallback for non-standard lumps
        if (sfx_len > 8)
            sample_count = (uint32_t)(sfx_len - 8);
        else
            return 0;
    }

    if (sample_rate < 4000 || sample_rate > 48000)
        sample_rate = 11025;

    // Find free channel or preempt lower/same priority channel
    int chan_idx = -1;
    for (int i = 0; i < MIXER_MAX_CHANNELS; i++)
    {
        if (!channels[i].active)
        {
            chan_idx = i;
            break;
        }
    }

    if (chan_idx == -1)
    {
        int lowest_prio = priority;
        for (int i = 0; i < MIXER_MAX_CHANNELS; i++)
        {
            if (channels[i].priority <= lowest_prio)
            {
                lowest_prio = channels[i].priority;
                chan_idx = i;
            }
        }
    }

    if (chan_idx == -1)
        return 0;

    mixer_channel_t* ch = &channels[chan_idx];
    ch->data = sfx_data + sizeof(doom_sfx_header_t);
    ch->length = sample_count;
    ch->position = 0;

    // Compute playback step (16.16 fixed point)
    uint32_t step = (uint32_t)(((uint64_t)sample_rate << 16) / current_output_rate);
    if (pitch != 128 && pitch > 0)
    {
        step = (step * (uint32_t)pitch) / 128;
    }
    ch->step = step;

    // Stereo panning (centered at sep=128)
    if (sep < 0) sep = 0;
    if (sep > 255) sep = 255;
    if (vol < 0) vol = 0;
    if (vol > 15) vol = 15;

    int left_pan  = (sep <= 128) ? 256 : (255 - sep) * 2;
    int right_pan = (sep >= 128) ? 256 : sep * 2;
    ch->vol_left  = (vol * left_pan) / 256;
    ch->vol_right = (vol * right_pan) / 256;
    ch->priority = priority;
    ch->handle = next_handle++;
    if (next_handle > 0x7FFFFFFF) next_handle = 1;
    ch->active = true;

    return ch->handle;
}

void I_Mixer_StopSound(int handle)
{
    if (handle <= 0) return;
    for (int i = 0; i < MIXER_MAX_CHANNELS; i++)
    {
        if (channels[i].active && channels[i].handle == handle)
        {
            channels[i].active = false;
            break;
        }
    }
}

int I_Mixer_SoundIsPlaying(int handle)
{
    if (handle <= 0) return 0;
    for (int i = 0; i < MIXER_MAX_CHANNELS; i++)
    {
        if (channels[i].active && channels[i].handle == handle)
            return 1;
    }
    return 0;
}

void I_Mixer_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    if (handle <= 0) return;
    for (int i = 0; i < MIXER_MAX_CHANNELS; i++)
    {
        if (channels[i].active && channels[i].handle == handle)
        {
            if (sep < 0) sep = 0;
            if (sep > 255) sep = 255;
            if (vol < 0) vol = 0;
            if (vol > 15) vol = 15;
            int left_pan  = (sep <= 128) ? 256 : (255 - sep) * 2;
            int right_pan = (sep >= 128) ? 256 : sep * 2;
            channels[i].vol_left  = (vol * left_pan) / 256;
            channels[i].vol_right = (vol * right_pan) / 256;
            break;
        }
    }
}

void I_Mixer_Mix(int16_t* output_buffer, int samples_to_mix)
{
    if (!output_buffer || samples_to_mix <= 0) return;

    memset(output_buffer, 0, samples_to_mix * 2 * sizeof(int16_t));

    if (master_sfx_volume == 0) return;

    // Collect active channels and pre-scale volume factors once per buffer
    mixer_channel_t* active_chans[MIXER_MAX_CHANNELS];
    int32_t pre_vol_l[MIXER_MAX_CHANNELS];
    int32_t pre_vol_r[MIXER_MAX_CHANNELS];
    int num_active = 0;

    for (int c = 0; c < MIXER_MAX_CHANNELS; c++)
    {
        mixer_channel_t* ch = &channels[c];
        if (ch->active)
        {
            if ((ch->position >> 16) >= ch->length)
            {
                ch->active = false;
            }
            else
            {
                active_chans[num_active] = ch;
                pre_vol_l[num_active] = (int32_t)ch->vol_left * master_sfx_volume;
                pre_vol_r[num_active] = (int32_t)ch->vol_right * master_sfx_volume;
                num_active++;
            }
        }
    }

    if (num_active == 0)
        return;

    for (int s = 0; s < samples_to_mix; s++)
    {
        int32_t left_acc = 0;
        int32_t right_acc = 0;

        for (int c = 0; c < num_active; c++)
        {
            mixer_channel_t* ch = active_chans[c];
            if (!ch->active) continue;

            uint32_t sample_idx = ch->position >> 16;
            if (sample_idx >= ch->length)
            {
                ch->active = false;
                continue;
            }

            // Convert unsigned 8-bit sample [0..255] (128 = 0) to signed [-128..127]
            int32_t sample = (int32_t)ch->data[sample_idx] - 128;

            left_acc  += sample * pre_vol_l[c];
            right_acc += sample * pre_vol_r[c];

            ch->position += ch->step;
        }

        // Clamp left channel to signed 16-bit
        if (left_acc > 32767) left_acc = 32767;
        else if (left_acc < -32768) left_acc = -32768;

        // Clamp right channel to signed 16-bit
        if (right_acc > 32767) right_acc = 32767;
        else if (right_acc < -32768) right_acc = -32768;

        output_buffer[s * 2 + 0] = (int16_t)left_acc;
        output_buffer[s * 2 + 1] = (int16_t)right_acc;
    }
}

void I_Mixer_Mix8(uint8_t* output_buffer, int samples_to_mix)
{
    if (!output_buffer || samples_to_mix <= 0) return;

    memset(output_buffer, 128, samples_to_mix); // 128 = silence in unsigned 8-bit PCM

    if (master_sfx_volume == 0) return;

    mixer_channel_t* active_chans[MIXER_MAX_CHANNELS];
    int32_t pre_vol[MIXER_MAX_CHANNELS];
    int num_active = 0;

    for (int c = 0; c < MIXER_MAX_CHANNELS; c++)
    {
        mixer_channel_t* ch = &channels[c];
        if (ch->active)
        {
            if ((ch->position >> 16) >= ch->length)
            {
                ch->active = false;
            }
            else
            {
                active_chans[num_active] = ch;
                int vol = (ch->vol_left + ch->vol_right) / 2;
                pre_vol[num_active] = (int32_t)vol * master_sfx_volume;
                num_active++;
            }
        }
    }

    if (num_active == 0)
        return;

    for (int s = 0; s < samples_to_mix; s++)
    {
        int32_t acc = 0;

        for (int c = 0; c < num_active; c++)
        {
            mixer_channel_t* ch = active_chans[c];
            if (!ch->active) continue;

            uint32_t sample_idx = ch->position >> 16;
            if (sample_idx >= ch->length)
            {
                ch->active = false;
                continue;
            }

            int32_t sample = (int32_t)ch->data[sample_idx] - 128;
            acc += sample * pre_vol[c];

            ch->position += ch->step;
        }

        // Scale and add bias of 128
        int32_t out = (acc / (15 * 16)) + 128;
        if (out > 255) out = 255;
        else if (out < 0) out = 0;

        output_buffer[s] = (uint8_t)out;
    }
}

