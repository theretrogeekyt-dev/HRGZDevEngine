// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Yamaha OPL2 (AdLib) & OPL3 FM synthesis music driver for MS-DOS (DJGPP).
//	Authentic Doom DMX synthesis architecture with accurate frequency curves,
//	double-voice instruments, pitch bending, percussion, and voice allocation.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__DJGPP__)
#include <pc.h>
#include <dos.h>
#include <unistd.h>
#endif

#include "doomdef.h"
#include "i_system.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_opl_dos.h"
#include "frequency_curve.h"

#define OPL_PORT_BASE_ADDR   0x388
#define OPL_PORT_BASE_DATA   0x389
#define OPL_PORT_OPL3_ADDR   0x38A
#define OPL_PORT_OPL3_DATA   0x38B

#define OPL_NUM_VOICES_BANK  9
#define OPL_MAX_TOTAL_VOICES 18
#define MUS_NUM_CHANS        16
#define MAX_SONG_SLOTS       16

#define GENMIDI_NUM_INSTRS     128
#define GENMIDI_NUM_PERCUSSION 47

#define GENMIDI_FLAG_FIXED  0x0001
#define GENMIDI_FLAG_2VOICE 0x0004

#pragma pack(push, 1)
typedef struct
{
    uint8_t tremolo;   // reg 0x20: AM/VIB/EG/KSR/MULT
    uint8_t attack;    // reg 0x60: Attack rate / Decay rate
    uint8_t sustain;   // reg 0x80: Sustain level / Release rate
    uint8_t waveform;  // reg 0xE0: Waveform select
    uint8_t scale;     // reg 0x40 bits 7-6: KSL
    uint8_t level;     // reg 0x40 bits 5-0: Total Level attenuation
} genmidi_op_t;

typedef struct
{
    genmidi_op_t modulator;         // 6 bytes
    uint8_t      feedback;          // 1 byte: reg 0xC0
    genmidi_op_t carrier;           // 6 bytes
    uint8_t      unused;            // 1 byte
    int16_t      base_note_offset;  // 2 bytes
} genmidi_voice_t;

typedef struct
{
    uint16_t        flags;          // 2 bytes: bit 0 = fixed pitch, bit 2 = double voice
    uint8_t         finetune;       // 1 byte
    uint8_t         fixed_note;     // 1 byte
    genmidi_voice_t voices[2];      // 32 bytes (16 bytes per voice)
} genmidi_instr_t;

typedef struct
{
    char     id[4];        // "MUS\x1a"
    uint16_t score_len;
    uint16_t score_start;
    uint16_t channels;
    uint16_t sec_channels;
    uint16_t instr_cnt;
    uint16_t dummy;
} mus_header_t;
#pragma pack(pop)

typedef struct
{
    const genmidi_instr_t* instrument;
    unsigned int volume;       // current effective volume (0..127)
    unsigned int volume_base;  // base volume (0..127)
    int          bend;         // pitch bend (-64..+63 in 1/32 semitones)
    unsigned int pan;          // pan register bits
} opl_channel_t;

typedef struct opl_voice_s
{
    int index;                      // 0..8 within bank
    int op1;                        // modulator operator offset
    int op2;                        // carrier operator offset
    int array;                      // 0x000 (bank 0) or 0x100 (bank 1)

    const genmidi_instr_t* current_instr;
    unsigned int current_instr_voice; // 0 or 1

    opl_channel_t* channel;
    unsigned int key;
    unsigned int note;
    unsigned int freq;
    unsigned int note_volume;
    unsigned int car_volume;
    unsigned int mod_volume;
    unsigned int reg_pan;
    unsigned int priority;
} opl_voice_t;

#if defined(__DJGPP__)
static boolean opl_detected = false;
static boolean opl_opl3mode = false;
static int     num_opl_voices = 9;

static boolean opl_playing = false;
static boolean opl_paused = false;
static boolean song_looping = false;

static const genmidi_instr_t* main_instrs = NULL;
static const genmidi_instr_t* percussion_instrs = NULL;

static int current_music_volume = 127; // Master volume (0..127)

// Song handle storage
static const uint8_t* song_slots[MAX_SONG_SLOTS];

// Sequencer state
static const uint8_t* mus_song_data = NULL;
static uint32_t       mus_song_len = 0;
static uint32_t       mus_score_start = 0;
static uint32_t       mus_pos = 0;
static int            mus_delay_tics = 0;

static opl_channel_t channels[MUS_NUM_CHANS];

static opl_voice_t  voices[OPL_MAX_TOTAL_VOICES];
static opl_voice_t* voice_free_list[OPL_MAX_TOTAL_VOICES];
static opl_voice_t* voice_alloced_list[OPL_MAX_TOTAL_VOICES];
static int          voice_free_num = 0;
static int          voice_alloced_num = 0;

// Operator offsets for voices 0..8
static const int voice_operators[2][OPL_NUM_VOICES_BANK] = {
    { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 },
    { 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15 }
};

// DMX volume mapping table (translates MIDI volume to OPL level)
static const unsigned int volume_mapping_table[128] = {
    0, 1, 3, 5, 6, 8, 10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127
};

static inline void OPL_WriteRegister(int reg, int val)
{
    int port_addr = (reg & 0x100) ? OPL_PORT_OPL3_ADDR : OPL_PORT_BASE_ADDR;
    int port_data = (reg & 0x100) ? OPL_PORT_OPL3_DATA : OPL_PORT_BASE_DATA;

    outportb(port_addr, (uint8_t)(reg & 0xFF));
    for (int i = 0; i < 6; i++) inportb(OPL_PORT_BASE_ADDR); // ~3.3us delay
    outportb(port_data, (uint8_t)val);
    for (int i = 0; i < 24; i++) inportb(OPL_PORT_BASE_ADDR); // ~15us delay
}

static void VoiceKeyOff(opl_voice_t* voice)
{
    // Clear key-on bit while leaving octave and frequency intact
    OPL_WriteRegister((0xB0 + voice->index) | voice->array, voice->freq >> 8);
}

static unsigned int FrequencyForVoice(opl_voice_t* voice)
{
    if (!voice->current_instr)
        return 0;

    const genmidi_voice_t* gm_voice = &voice->current_instr->voices[voice->current_instr_voice];
    signed int note = voice->note;

    if ((voice->current_instr->flags & GENMIDI_FLAG_FIXED) == 0)
    {
        note += gm_voice->base_note_offset;
    }

    while (note < 0) note += 12;
    while (note > 95) note -= 12;

    signed int freq_index = 64 + 32 * note + voice->channel->bend;

    if (voice->current_instr_voice != 0)
    {
        freq_index += ((int)voice->current_instr->finetune / 2) - 64;
    }

    if (freq_index < 0) freq_index = 0;

    if (freq_index < 284)
    {
        return frequency_curve[freq_index];
    }

    unsigned int sub_index = (freq_index - 284) % (12 * 32);
    unsigned int octave = (freq_index - 284) / (12 * 32);

    if (octave >= 7) octave = 7;

    return frequency_curve[sub_index + 284] | (octave << 10);
}

static void UpdateVoiceFrequency(opl_voice_t* voice)
{
    unsigned int freq = FrequencyForVoice(voice);

    if (voice->freq != freq)
    {
        OPL_WriteRegister((0xA0 + voice->index) | voice->array, freq & 0xFF);
        OPL_WriteRegister((0xB0 + voice->index) | voice->array, (freq >> 8) | 0x20);
        voice->freq = freq;
    }
}

static void SetVoiceVolume(opl_voice_t* voice, unsigned int volume)
{
    voice->note_volume = volume;

    if (!voice->current_instr || !voice->channel)
        return;

    const genmidi_voice_t* data = &voice->current_instr->voices[voice->current_instr_voice];

    unsigned int note_vol = (volume > 127) ? 127 : volume;
    unsigned int chan_vol = (voice->channel->volume > 127) ? 127 : voice->channel->volume;

    unsigned int midi_volume = 2 * (volume_mapping_table[chan_vol] + 1);
    unsigned int full_volume = (volume_mapping_table[note_vol] * midi_volume) >> 9;

    unsigned int car_volume = 0x3F - full_volume;

    if (car_volume != (voice->car_volume & 0x3F))
    {
        voice->car_volume = car_volume | (voice->car_volume & 0xC0);
        OPL_WriteRegister((0x40 + voice->op2) | voice->array, voice->car_volume);

        // If non-modulated (additive) feedback mode, also set modulator volume
        if ((data->feedback & 0x01) != 0 && data->modulator.level != 0x3F)
        {
            unsigned int mod_volume = data->modulator.level;
            if (mod_volume < car_volume)
                mod_volume = car_volume;

            mod_volume |= (voice->mod_volume & 0xC0);
            if (mod_volume != voice->mod_volume)
            {
                voice->mod_volume = mod_volume;
                OPL_WriteRegister((0x40 + voice->op1) | voice->array,
                                  mod_volume | (data->modulator.scale & 0xC0));
            }
        }
    }
}

static void LoadOperatorData(int op_reg, const genmidi_op_t* data, boolean max_level, unsigned int* vol_out)
{
    int level = data->scale;
    if (max_level)
        level |= 0x3F;
    else
        level |= (data->level & 0x3F);

    *vol_out = (unsigned int)level;

    OPL_WriteRegister(0x40 + op_reg, level);
    OPL_WriteRegister(0x20 + op_reg, data->tremolo);
    OPL_WriteRegister(0x60 + op_reg, data->attack);
    OPL_WriteRegister(0x80 + op_reg, data->sustain);
    OPL_WriteRegister(0xE0 + op_reg, data->waveform & 0x03);
}

static void SetVoiceInstrument(opl_voice_t* voice, const genmidi_instr_t* instr, unsigned int instr_voice)
{
    if (voice->current_instr == instr && voice->current_instr_voice == instr_voice)
        return;

    voice->current_instr = instr;
    voice->current_instr_voice = instr_voice;

    const genmidi_voice_t* data = &instr->voices[instr_voice];
    boolean modulating = ((data->feedback & 0x01) == 0);

    // Doom DMX loads carrier (op2) first with maximum attenuation (0x3F), then modulator (op1)
    LoadOperatorData(voice->op2 | voice->array, &data->carrier, true, &voice->car_volume);
    LoadOperatorData(voice->op1 | voice->array, &data->modulator, !modulating, &voice->mod_volume);

    // Feedback and panning (0x30 = stereo centered for OPL3)
    OPL_WriteRegister((0xC0 + voice->index) | voice->array, data->feedback | voice->reg_pan);

    voice->priority = 0x0F - (data->carrier.attack >> 4) + 0x0F - (data->carrier.sustain & 0x0F);
}

static void SetChannelVolume(opl_channel_t* channel, unsigned int volume)
{
    channel->volume_base = volume;

    // Proportionally scale channel volume by master music volume (0..127)
    channel->volume = (volume * (unsigned int)current_music_volume) / 127;

    for (int i = 0; i < num_opl_voices; i++)
    {
        if (voices[i].channel == channel)
        {
            SetVoiceVolume(&voices[i], voices[i].note_volume);
        }
    }
}

static void ReleaseVoice(int index)
{
    if (index >= voice_alloced_num)
        return;

    opl_voice_t* voice = voice_alloced_list[index];
    VoiceKeyOff(voice);

    voice->channel = NULL;
    voice->note = 0;
    voice->key = 0;
    voice->current_instr = NULL;

    voice_alloced_num--;
    for (int i = index; i < voice_alloced_num; i++)
    {
        voice_alloced_list[i] = voice_alloced_list[i + 1];
    }

    voice_free_list[voice_free_num++] = voice;
}

static void ReplaceExistingVoice(void)
{
    if (voice_alloced_num == 0)
        return;

    // Priority 1: Steal the oldest secondary voice (chorus/doubling voice)
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->current_instr_voice != 0)
        {
            ReleaseVoice(i);
            return;
        }
    }

    // Priority 2: Steal the oldest primary voice (index 0 has played longest)
    ReleaseVoice(0);
}

static opl_voice_t* GetFreeVoice(void)
{
    if (voice_free_num == 0)
        return NULL;

    opl_voice_t* result = voice_free_list[0];
    voice_free_num--;

    for (int i = 0; i < voice_free_num; i++)
    {
        voice_free_list[i] = voice_free_list[i + 1];
    }

    voice_alloced_list[voice_alloced_num++] = result;
    return result;
}

static void VoiceKeyOn(opl_channel_t* channel,
                       const genmidi_instr_t* instrument,
                       unsigned int instrument_voice,
                       unsigned int note,
                       unsigned int key,
                       unsigned int volume)
{
    opl_voice_t* voice = GetFreeVoice();
    if (!voice) return;

    voice->channel = channel;
    voice->key = key;

    if (instrument->flags & GENMIDI_FLAG_FIXED)
        voice->note = instrument->fixed_note;
    else
        voice->note = note;

    voice->reg_pan = channel->pan;

    SetVoiceInstrument(voice, instrument, instrument_voice);
    SetVoiceVolume(voice, volume);

    voice->freq = 0;
    UpdateVoiceFrequency(voice);
}

static void KeyOffEvent(opl_channel_t* channel, unsigned int key)
{
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->channel == channel && voice_alloced_list[i]->key == key)
        {
            ReleaseVoice(i);
            --i;
        }
    }
}

static void KeyOnEvent(opl_channel_t* channel, unsigned int chan_num, unsigned int key, unsigned int volume)
{
    if (volume == 0)
    {
        KeyOffEvent(channel, key);
        return;
    }

    const genmidi_instr_t* instrument;
    unsigned int note;

    if (chan_num == 15) // Percussion channel
    {
        if (key < 35 || key > 81)
            return;

        instrument = &percussion_instrs[key - 35];
        note = 60;
    }
    else
    {
        instrument = channel->instrument;
        note = key;
    }

    if (!instrument)
        return;

    boolean double_voice = (instrument->flags & GENMIDI_FLAG_2VOICE) != 0;

    // Allocate primary voice (steal oldest voice if necessary)
    if (voice_free_num == 0)
    {
        ReplaceExistingVoice();
    }

    VoiceKeyOn(channel, instrument, 0, note, key, volume);

    // If double-voice instrument: only allocate voice 1 if a voice is actually free.
    // Never steal an active voice just for secondary voice doubling/chorus.
    if (double_voice && voice_free_num > 0)
    {
        VoiceKeyOn(channel, instrument, 1, note, key, volume);
    }
}

static void PitchBendEvent(opl_channel_t* channel, uint8_t bend_byte)
{
    channel->bend = ((int)bend_byte / 2) - 64;

    for (int i = 0; i < num_opl_voices; i++)
    {
        if (voices[i].channel == channel)
        {
            UpdateVoiceFrequency(&voices[i]);
        }
    }
}

static void SilenceAllVoices(void)
{
    while (voice_alloced_num > 0)
    {
        ReleaseVoice(0);
    }

    for (int i = 0; i < num_opl_voices; i++)
    {
        VoiceKeyOff(&voices[i]);
    }
}

static void InitVoiceStructures(void)
{
    voice_free_num = num_opl_voices;
    voice_alloced_num = 0;

    for (int i = 0; i < num_opl_voices; ++i)
    {
        voices[i].index = i % OPL_NUM_VOICES_BANK;
        voices[i].op1 = voice_operators[0][i % OPL_NUM_VOICES_BANK];
        voices[i].op2 = voice_operators[1][i % OPL_NUM_VOICES_BANK];
        voices[i].array = (i / OPL_NUM_VOICES_BANK) << 8;
        voices[i].current_instr = NULL;
        voices[i].current_instr_voice = 0;
        voices[i].channel = NULL;
        voices[i].note = 0;
        voices[i].key = 0;
        voices[i].freq = 0;
        voices[i].note_volume = 0;
        voices[i].car_volume = 0x3F;
        voices[i].mod_volume = 0x3F;
        voices[i].reg_pan = 0x30; // Stereo center (both left and right outputs enabled)
        voices[i].priority = 0;

        voice_free_list[i] = &voices[i];
    }
}

static void InitOPLRegisters(boolean is_opl3)
{
    int r;

    // Reset timers
    OPL_WriteRegister(0x04, 0x60);
    OPL_WriteRegister(0x04, 0x80);

    // Mute carrier & modulator level registers (0x40..0x55)
    for (r = 0x40; r <= 0x55; ++r)
    {
        OPL_WriteRegister(r, 0x3F);
    }

    // Clear attack, sustain, waveform registers
    for (r = 0x60; r <= 0xF5; ++r)
    {
        OPL_WriteRegister(r, 0x00);
    }

    for (r = 1; r < 0x40; ++r)
    {
        OPL_WriteRegister(r, 0x00);
    }

    OPL_WriteRegister(0x01, 0x20); // Waveform select enable
    OPL_WriteRegister(0x08, 0x40); // Keyboard split on (standard DMX initialization)

    if (is_opl3)
    {
        OPL_WriteRegister(0x105, 0x01); // Enable OPL3 mode
        OPL_WriteRegister(0x104, 0x00); // Disable 4-op mode: use 18 2-op voices

        for (r = 0x40; r <= 0x55; ++r)
        {
            OPL_WriteRegister(r | 0x100, 0x3F);
        }
        for (r = 0x60; r <= 0xF5; ++r)
        {
            OPL_WriteRegister(r | 0x100, 0x00);
        }
        for (r = 1; r < 0x40; ++r)
        {
            OPL_WriteRegister(r | 0x100, 0x00);
        }
    }
}

static boolean DetectOPLChip(void)
{
    OPL_WriteRegister(0x04, 0x60); // Reset timer 1 and 2
    OPL_WriteRegister(0x04, 0x80); // Reset IRQ

    uint8_t stat1 = inportb(OPL_PORT_BASE_ADDR);

    OPL_WriteRegister(0x02, 0xFF); // Set timer 1 count
    OPL_WriteRegister(0x04, 0x21); // Start timer 1

    delay(2);

    uint8_t stat2 = inportb(OPL_PORT_BASE_ADDR);

    OPL_WriteRegister(0x04, 0x60);
    OPL_WriteRegister(0x04, 0x80);

    if ((stat1 & 0xE0) != 0x00 || (stat2 & 0xE0) != 0xC0)
    {
        return false;
    }

    // Yamaha YMF262 (OPL3) detection:
    // Status bits 1 and 2 at port base+0 (0x388) are 0 on OPL3 after timer reset.
    uint8_t stat = inportb(OPL_PORT_BASE_ADDR);
    boolean is_opl3 = ((stat & 0x06) == 0x00);

    // Command line override support (-opl3 or -opl2)
    if (M_CheckParm("-opl3"))
        is_opl3 = true;
    if (M_CheckParm("-opl2"))
        is_opl3 = false;

    // Check BLASTER environment variable: T4 (SB Pro 2) or T6 (SB16/AWE) has OPL3
    char* blaster = getenv("BLASTER");
    if (blaster)
    {
        char* t = strstr(blaster, "T");
        if (t && (t[1] == '4' || t[1] == '6'))
        {
            is_opl3 = true;
        }
    }

    if (is_opl3)
    {
        opl_opl3mode = true;
        num_opl_voices = OPL_MAX_TOTAL_VOICES;
    }
    else
    {
        opl_opl3mode = false;
        num_opl_voices = OPL_NUM_VOICES_BANK;
    }

    return true;
}
#endif

boolean I_OPL_Init(void)
{
#if defined(__DJGPP__)
    if (opl_detected)
        return true;

    if (!DetectOPLChip())
    {
        printf("I_OPL_Init: No AdLib / OPL chip detected at 0x%X.\n", OPL_PORT_BASE_ADDR);
        return false;
    }

    InitOPLRegisters(opl_opl3mode);

    // Cache GENMIDI instruments lump
    int genmidi_lump = W_CheckNumForName("GENMIDI");
    if (genmidi_lump < 0)
    {
        printf("I_OPL_Init: Error - GENMIDI lump not found in WAD!\n");
        return false;
    }

    const uint8_t* raw = (const uint8_t*)W_CacheLumpNum(genmidi_lump, PU_STATIC);
    if (memcmp(raw, "#OPL_II#", 8) != 0)
    {
        printf("I_OPL_Init: Error - Invalid GENMIDI header!\n");
        return false;
    }

    main_instrs = (const genmidi_instr_t*)(raw + 8);
    percussion_instrs = main_instrs + GENMIDI_NUM_INSTRS;

    InitVoiceStructures();

    for (int i = 0; i < MUS_NUM_CHANS; i++)
    {
        channels[i].instrument = &main_instrs[0];
        channels[i].volume = 100;
        channels[i].volume_base = 100;
        channels[i].bend = 0;
        channels[i].pan = 0x30;
    }

    memset(song_slots, 0, sizeof(song_slots));

    if (opl_opl3mode)
    {
        printf("I_OPL_Init: Yamaha OPL3 (YMF262) ready! 18-voice stereo FM synthesis enabled.\n");
    }
    else
    {
        printf("I_OPL_Init: Yamaha OPL2 (YM3812 / AdLib) ready! 9-voice FM synthesis enabled.\n");
    }

    opl_detected = true;
    return true;
#else
    return false;
#endif
}

void I_OPL_Shutdown(void)
{
#if defined(__DJGPP__)
    if (!opl_detected)
        return;

    I_OPL_StopSong(1);
    SilenceAllVoices();
    InitOPLRegisters(false);
    opl_detected = false;
#endif
}

int I_OPL_RegisterSong(void* data, int len)
{
    (void)len;
    if (!data) return 0;

#if defined(__DJGPP__)
    for (int i = 1; i < MAX_SONG_SLOTS; i++)
    {
        if (song_slots[i] == (const uint8_t*)data)
            return i;
    }
    for (int i = 1; i < MAX_SONG_SLOTS; i++)
    {
        if (!song_slots[i])
        {
            song_slots[i] = (const uint8_t*)data;
            return i;
        }
    }
    return 1;
#else
    return 0;
#endif
}

void I_OPL_UnRegisterSong(int handle)
{
#if defined(__DJGPP__)
    if (handle >= 1 && handle < MAX_SONG_SLOTS)
    {
        if (mus_song_data == song_slots[handle])
        {
            I_OPL_StopSong(handle);
        }
        song_slots[handle] = NULL;
    }
#else
    (void)handle;
#endif
}

void I_OPL_PlaySong(int handle, int looping)
{
#if defined(__DJGPP__)
    if (!opl_detected || handle < 1 || handle >= MAX_SONG_SLOTS)
        return;

    const uint8_t* data = song_slots[handle];
    if (!data) return;

    const mus_header_t* hdr = (const mus_header_t*)data;
    if (memcmp(hdr->id, "MUS\x1a", 4) != 0)
    {
        mus_song_data = NULL;
        return;
    }

    SilenceAllVoices();

    mus_song_data = data;
    mus_song_len = hdr->score_len;
    mus_score_start = hdr->score_start;
    mus_pos = mus_score_start;
    mus_delay_tics = 0;
    song_looping = (boolean)looping;
    opl_playing = true;
    opl_paused = false;

    for (int i = 0; i < MUS_NUM_CHANS; i++)
    {
        channels[i].instrument = &main_instrs[0];
        channels[i].volume_base = 100;
        channels[i].volume = (100 * (unsigned int)current_music_volume) / 127;
        channels[i].bend = 0;
        channels[i].pan = 0x30;
    }
#endif
}

void I_OPL_StopSong(int handle)
{
    (void)handle;
#if defined(__DJGPP__)
    opl_playing = false;
    opl_paused = false;
    mus_song_data = NULL;
    SilenceAllVoices();
#endif
}

void I_OPL_PauseSong(int handle)
{
    (void)handle;
#if defined(__DJGPP__)
    if (opl_playing)
    {
        opl_paused = true;
        SilenceAllVoices();
    }
#endif
}

void I_OPL_ResumeSong(int handle)
{
    (void)handle;
#if defined(__DJGPP__)
    if (opl_playing)
    {
        opl_paused = false;
    }
#endif
}

void I_OPL_SetMusicVolume(int volume)
{
#if defined(__DJGPP__)
    if (volume < 0) volume = 0;
    if (volume <= 15)
        volume = (volume * 127) / 15;
    if (volume > 127)
        volume = 127;

    current_music_volume = volume;

    if (!opl_detected)
        return;

    for (int i = 0; i < MUS_NUM_CHANS; i++)
    {
        SetChannelVolume(&channels[i], channels[i].volume_base);
    }
#endif
}

void I_OPL_Update(int tics)
{
#if defined(__DJGPP__)
    if (!opl_detected || !opl_playing || opl_paused || !mus_song_data)
        return;

    while (tics > 0 && opl_playing)
    {
        if (mus_delay_tics > 0)
        {
            if (mus_delay_tics > tics)
            {
                mus_delay_tics -= tics;
                return;
            }
            else
            {
                tics -= mus_delay_tics;
                mus_delay_tics = 0;
            }
        }

        while (mus_delay_tics == 0 && opl_playing)
        {
            if (mus_pos >= mus_song_len + mus_score_start)
            {
                if (song_looping)
                {
                    SilenceAllVoices();
                    for (int i = 0; i < MUS_NUM_CHANS; i++)
                    {
                        channels[i].instrument = &main_instrs[0];
                        channels[i].volume_base = 100;
                        channels[i].volume = (100 * (unsigned int)current_music_volume) / 127;
                        channels[i].bend = 0;
                        channels[i].pan = 0x30;
                    }
                    mus_pos = mus_score_start;
                    mus_delay_tics = 0;
                    return;
                }
                else
                {
                    I_OPL_StopSong(1);
                    return;
                }
            }

            uint8_t event_desc = mus_song_data[mus_pos++];
            uint8_t event_type = (event_desc >> 4) & 0x07;
            uint8_t chan_idx   = event_desc & 0x0F;
            boolean last_event = (event_desc & 0x80) != 0;

            opl_channel_t* chan = &channels[chan_idx];

            switch (event_type)
            {
                case 0: // Release Note
                {
                    uint8_t key = mus_song_data[mus_pos++] & 0x7F;
                    KeyOffEvent(chan, key);
                    break;
                }
                case 1: // Play Note
                {
                    uint8_t b = mus_song_data[mus_pos++];
                    uint8_t key = b & 0x7F;
                    uint8_t vol = 64;
                    if (b & 0x80)
                        vol = mus_song_data[mus_pos++] & 0x7F;
                    KeyOnEvent(chan, chan_idx, key, vol);
                    break;
                }
                case 2: // Pitch Bend
                {
                    uint8_t bend = mus_song_data[mus_pos++];
                    PitchBendEvent(chan, bend);
                    break;
                }
                case 3: // System Event
                {
                    uint8_t sys = mus_song_data[mus_pos++];
                    if (sys == 10 || sys == 11) // All notes off
                    {
                        for (int i = 0; i < voice_alloced_num; i++)
                        {
                            if (voice_alloced_list[i]->channel == chan)
                            {
                                ReleaseVoice(i);
                                --i;
                            }
                        }
                    }
                    else if (sys == 14) // Reset all controllers
                    {
                        chan->bend = 0;
                        chan->pan = 0x30;
                        SetChannelVolume(chan, 100);
                    }
                    break;
                }
                case 4: // Change Controller
                {
                    uint8_t ctrl = mus_song_data[mus_pos++];
                    uint8_t val  = mus_song_data[mus_pos++];
                    if (ctrl == 0) // Instrument change
                    {
                        if (chan_idx != 15) // Not percussion
                        {
                            chan->instrument = &main_instrs[val < 128 ? val : 0];
                        }
                    }
                    else if (ctrl == 3) // Channel volume
                    {
                        SetChannelVolume(chan, val);
                    }
                    else if (ctrl == 4) // Pan
                    {
                        if (val < 48)
                            chan->pan = 0x10; // Left
                        else if (val > 80)
                            chan->pan = 0x20; // Right
                        else
                            chan->pan = 0x30; // Center

                        for (int i = 0; i < num_opl_voices; i++)
                        {
                            if (voices[i].channel == chan && voices[i].current_instr)
                            {
                                voices[i].reg_pan = chan->pan;
                                const genmidi_voice_t* d = &voices[i].current_instr->voices[voices[i].current_instr_voice];
                                OPL_WriteRegister((0xC0 + voices[i].index) | voices[i].array, d->feedback | voices[i].reg_pan);
                            }
                        }
                    }
                    else if (ctrl == 10 || ctrl == 11) // All notes off
                    {
                        for (int i = 0; i < voice_alloced_num; i++)
                        {
                            if (voice_alloced_list[i]->channel == chan)
                            {
                                ReleaseVoice(i);
                                --i;
                            }
                        }
                    }
                    else if (ctrl == 14) // Reset all controllers
                    {
                        chan->bend = 0;
                        chan->pan = 0x30;
                        SetChannelVolume(chan, 100);
                    }
                    break;
                }
                case 5: // Score End / loop
                case 6:
                {
                    if (song_looping)
                    {
                        SilenceAllVoices();
                        for (int i = 0; i < MUS_NUM_CHANS; i++)
                        {
                            channels[i].instrument = &main_instrs[0];
                            channels[i].volume_base = 100;
                            channels[i].volume = (100 * (unsigned int)current_music_volume) / 127;
                            channels[i].bend = 0;
                            channels[i].pan = 0x30;
                        }
                        mus_pos = mus_score_start;
                        mus_delay_tics = 0;
                        return; // Return immediately to start next tick cleanly from score start
                    }
                    else
                    {
                        I_OPL_StopSong(1);
                        return;
                    }
                }
                default:
                    break;
            }

            if (last_event)
            {
                // Read variable-length delay ticks
                int delay = 0;
                while (mus_pos < mus_song_len + mus_score_start)
                {
                    uint8_t b = mus_song_data[mus_pos++];
                    delay = (delay * 128) + (b & 0x7F);
                    if ((b & 0x80) == 0) break;
                }
                mus_delay_tics = delay;
                break;
            }
        }
    }
#endif
}
