#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i_mus2midi.h"

// MUS format header
#pragma pack(push, 1)
typedef struct
{
    char     id[4];        // "MUS\x1A"
    uint16_t score_len;
    uint16_t score_start;
    uint16_t channels;
    uint16_t sec_channels;
    uint16_t instr_cnt;
    uint16_t dummy;
} mus_header_t;
#pragma pack(pop)

// Channel mapping table (MUS channel 15 -> MIDI channel 9 percussion)
static const uint8_t channel_map[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 9
};

// MUS controller to MIDI controller mapping
static const uint8_t ctrl_map[15] = {
    0,   // 0: program change (handled separately)
    0,   // 1: bank select
    1,   // 2: modulation
    7,   // 3: volume
    10,  // 4: pan
    11,  // 5: expression
    91,  // 6: reverb
    93,  // 7: chorus
    64,  // 8: sustain pedal
    67,  // 9: soft pedal
    120, // 10: all sounds off
    123, // 11: all notes off
    126, // 12: mono
    127, // 13: poly
    121  // 14: reset all controllers
};

static void WriteVarLen(uint8_t** ptr, uint32_t value)
{
    uint32_t buffer = value & 0x7F;
    while ((value >>= 7) > 0)
    {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    for (;;)
    {
        *(*ptr)++ = (uint8_t)(buffer & 0xFF);
        if (buffer & 0x80)
            buffer >>= 8;
        else
            break;
    }
}

static void Write16BE(uint8_t** ptr, uint16_t val)
{
    *(*ptr)++ = (uint8_t)((val >> 8) & 0xFF);
    *(*ptr)++ = (uint8_t)(val & 0xFF);
}

static void Write32BE(uint8_t** ptr, uint32_t val)
{
    *(*ptr)++ = (uint8_t)((val >> 24) & 0xFF);
    *(*ptr)++ = (uint8_t)((val >> 16) & 0xFF);
    *(*ptr)++ = (uint8_t)((val >> 8) & 0xFF);
    *(*ptr)++ = (uint8_t)(val & 0xFF);
}

uint8_t* I_MusToMidi(const uint8_t* mus_data, size_t mus_len, size_t* midi_len)
{
    if (!mus_data || mus_len < sizeof(mus_header_t) || !midi_len)
        return NULL;

    const mus_header_t* hdr = (const mus_header_t*)mus_data;
    if (memcmp(hdr->id, "MUS\x1A", 4) != 0)
    {
        // Might already be standard MIDI (MThd)
        if (mus_len >= 4 && memcmp(mus_data, "MThd", 4) == 0)
        {
            uint8_t* copy = (uint8_t*)malloc(mus_len);
            if (!copy) return NULL;
            memcpy(copy, mus_data, mus_len);
            *midi_len = mus_len;
            return copy;
        }
        return NULL;
    }

    uint16_t score_start = hdr->score_start;
    if (score_start >= mus_len)
        return NULL;

    size_t actual_mus_len = mus_len;
    if (hdr->score_len > 0 && (size_t)(score_start + hdr->score_len) <= mus_len)
    {
        actual_mus_len = score_start + hdr->score_len;
    }

    // Allocate an output buffer generously (accounting for channel reset blocks)
    size_t max_track_size = actual_mus_len * 4 + 4096;
    uint8_t* track_buf = (uint8_t*)malloc(max_track_size);
    if (!track_buf)
        return NULL;

    uint8_t* trk = track_buf;
    const uint8_t* mus = mus_data + score_start;
    const uint8_t* mus_end = mus_data + actual_mus_len;

    // Emit initial controller states for all 16 channels at delta time 0
    // so any loop back to timestamp 0 starts in a clean, calibrated state.
    for (int ch = 0; ch < 16; ch++)
    {
        // Controller 121: Reset All Controllers
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 121;
        *trk++ = 0;

        // Controller 120: All Sound Off
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 120;
        *trk++ = 0;

        // Controller 123: All Notes Off
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 123;
        *trk++ = 0;

        // Pitch Bend to center (8192 = 0x2000 => LSB 0x00, MSB 0x40)
        WriteVarLen(&trk, 0);
        *trk++ = 0xE0 | ch;
        *trk++ = 0x00;
        *trk++ = 0x40;

        // Controller 1: Modulation Wheel = 0
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 1;
        *trk++ = 0;

        // Controller 64: Sustain / Damper Pedal = 0 (off)
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 64;
        *trk++ = 0;

        // Controller 11: Expression = 127 (full)
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 11;
        *trk++ = 127;

        // Standard GM Pitch Bend Sensitivity (+/- 2 semitones):
        // RPN MSB (101) = 0, RPN LSB (100) = 0
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 101;
        *trk++ = 0;

        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 100;
        *trk++ = 0;

        // Data Entry MSB (6) = 2 (semitones), Data Entry LSB (38) = 0 (cents)
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 6;
        *trk++ = 2;

        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 38;
        *trk++ = 0;

        // Deselect RPN: RPN MSB (101) = 127, RPN LSB (100) = 127
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 101;
        *trk++ = 127;

        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 100;
        *trk++ = 127;
    }

    uint8_t last_velocity[16] = {64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64};
    uint32_t queued_delta = 0;

    while (mus < mus_end)
    {
        uint8_t event = *mus++;
        uint8_t chan = channel_map[event & 0x0F];
        uint8_t type = (event >> 4) & 0x07;
        boolean last = (event & 0x80) != 0;

        switch (type)
        {
            case 0: // Release key
            {
                if (mus >= mus_end) break;
                uint8_t note = *mus++;
                WriteVarLen(&trk, queued_delta);
                queued_delta = 0;
                *trk++ = 0x80 | chan;
                *trk++ = note & 0x7F;
                *trk++ = 0;
                break;
            }
            case 1: // Press key
            {
                if (mus >= mus_end) break;
                uint8_t note = *mus++;
                if (note & 0x80)
                {
                    if (mus < mus_end)
                        last_velocity[chan] = (*mus++) & 0x7F;
                }
                WriteVarLen(&trk, queued_delta);
                queued_delta = 0;
                *trk++ = 0x90 | chan;
                *trk++ = note & 0x7F;
                *trk++ = last_velocity[chan];
                break;
            }
            case 2: // Pitch wheel
            {
                if (mus >= mus_end) break;
                uint8_t wheel = *mus++;
                // Convert 8-bit MUS pitch (0..255) to 14-bit MIDI pitch (0..16383)
                // Center 128 maps exactly to 8192 (0x2000 => LSB 0x00, MSB 0x40)
                uint16_t bend;
                if (wheel == 128)
                {
                    bend = 8192;
                }
                else if (wheel < 128)
                {
                    bend = (uint16_t)wheel * 64; // 0..127 -> 0..8128
                }
                else
                {
                    bend = 8192 + (uint16_t)(wheel - 128) * 8191 / 127; // 129..255 -> 8256..16383
                }
                WriteVarLen(&trk, queued_delta);
                queued_delta = 0;
                *trk++ = 0xE0 | chan;
                *trk++ = bend & 0x7F;
                *trk++ = (bend >> 7) & 0x7F;
                break;
            }
            case 3: // System event
            {
                if (mus >= mus_end) break;
                uint8_t ctrl = *mus++;
                if (ctrl >= 10 && ctrl <= 14)
                {
                    WriteVarLen(&trk, queued_delta);
                    queued_delta = 0;
                    *trk++ = 0xB0 | chan;
                    *trk++ = ctrl_map[ctrl];
                    *trk++ = 0;
                }
                break;
            }
            case 4: // Change controller
            {
                if (mus + 1 >= mus_end) break;
                uint8_t ctrl = *mus++;
                uint8_t val  = *mus++;
                WriteVarLen(&trk, queued_delta);
                queued_delta = 0;
                if (ctrl == 0) // Program change
                {
                    *trk++ = 0xC0 | chan;
                    *trk++ = val & 0x7F;
                }
                else if (ctrl < sizeof(ctrl_map))
                {
                    *trk++ = 0xB0 | chan;
                    *trk++ = ctrl_map[ctrl];
                    *trk++ = val & 0x7F;
                }
                break;
            }
            case 5: // End of measure / score
                goto score_end;
            default:
                break;
        }

        if (last)
        {
            // Read variable length delta time in 140Hz MUS tics
            uint32_t delta = 0;
            uint8_t byte;
            do
            {
                if (mus >= mus_end) break;
                byte = *mus++;
                delta = (delta << 7) | (byte & 0x7F);
            } while (byte & 0x80);

            queued_delta += delta;
        }
    }

score_end:
    // Write track-end cleanup events (silence notes and reset pitch/modulation before loop)
    for (int ch = 0; ch < 16; ch++)
    {
        // First event absorbs any queued_delta accumulated at the end of the song
        WriteVarLen(&trk, queued_delta);
        queued_delta = 0;

        // All Sound Off & All Notes Off
        *trk++ = 0xB0 | ch;
        *trk++ = 120;
        *trk++ = 0;

        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 123;
        *trk++ = 0;

        // Pitch Bend to center (8192 = 0x2000 => LSB 0x00, MSB 0x40)
        WriteVarLen(&trk, 0);
        *trk++ = 0xE0 | ch;
        *trk++ = 0x00;
        *trk++ = 0x40;

        // Modulation Wheel = 0
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 1;
        *trk++ = 0;

        // Sustain / Damper Pedal = 0
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 64;
        *trk++ = 0;

        // Controller 121: Reset All Controllers
        WriteVarLen(&trk, 0);
        *trk++ = 0xB0 | ch;
        *trk++ = 121;
        *trk++ = 0;
    }

    // Add pad delta time (4 tics = ~28ms at 140Hz MUS rate) before End of Track
    // so synthesizers process cleanup before loop boundary wrap
    WriteVarLen(&trk, 4);
    *trk++ = 0xFF;
    *trk++ = 0x2F;
    *trk++ = 0x00;

    size_t track_len = trk - track_buf;
    size_t total_midi_len = 14 + 8 + track_len; // Header chunk (14) + Track chunk header (8) + track data

    uint8_t* out = (uint8_t*)malloc(total_midi_len);
    if (!out)
    {
        free(track_buf);
        return NULL;
    }

    uint8_t* p = out;
    // MThd chunk
    memcpy(p, "MThd", 4); p += 4;
    Write32BE(&p, 6);        // Length: 6
    Write16BE(&p, 0);        // Format 0 (single track)
    Write16BE(&p, 1);        // 1 track
    Write16BE(&p, 70);       // Division: 70 tics per quarter note (at 140Hz MUS rate)

    // MTrk chunk
    memcpy(p, "MTrk", 4); p += 4;
    Write32BE(&p, (uint32_t)track_len);
    memcpy(p, track_buf, track_len);

    free(track_buf);
    *midi_len = total_midi_len;
    return out;
}

