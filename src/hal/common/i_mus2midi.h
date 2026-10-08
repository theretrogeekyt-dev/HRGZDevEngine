#ifndef __I_MUS2MIDI_H__
#define __I_MUS2MIDI_H__

#include <stdint.h>
#include <stddef.h>
#include "doomtype.h"

// Converts DOOM MUS format data to Standard MIDI File (SMF Format 0)
// Returns newly allocated buffer with MIDI data (caller frees with free())
// Sets *midi_len to the size of the returned buffer
uint8_t* I_MusToMidi(const uint8_t* mus_data, size_t mus_len, size_t* midi_len);

#endif // __I_MUS2MIDI_H__

