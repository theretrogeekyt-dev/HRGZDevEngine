#ifndef __XENON_SOUND_SOUND_H__
#define __XENON_SOUND_SOUND_H__

#include <stdint.h>

void xenon_sound_init(void);
void xenon_sound_submit(void* pcm, int bytes);

#endif
