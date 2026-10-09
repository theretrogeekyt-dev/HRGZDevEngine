//-----------------------------------------------------------------------------
//
// LibXenon Weak Hardware Stubs
// Resolves symbols for standalone cross-compilation without external SDK archives
// Overridden automatically when linked against real libxenon.a
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "include/xenos/xenos.h"
#include "include/xenos/xe.h"
#include "include/input/input.h"
#include "include/usb/usb.h"
#include "include/xenon_sound/sound.h"
#include "include/xenon_soc/xenon_power.h"
#include "include/console/console.h"
#include "include/diskio/ata.h"

static uint32_t stub_fb[1280 * 720];

__attribute__((weak)) void xenos_init(int mode)
{
    (void)mode;
}

__attribute__((weak)) void xe_init(void)
{
}

__attribute__((weak)) void* xe_get_buffer(void)
{
    return stub_fb;
}

__attribute__((weak)) void xe_sync(void)
{
}

__attribute__((weak)) int get_controller_data(struct controller_data_s *ctrl, int port)
{
    (void)ctrl;
    (void)port;
    return 0;
}

__attribute__((weak)) int usb_init(void)
{
    return 0;
}

__attribute__((weak)) void usb_do_poll(void)
{
}

__attribute__((weak)) void xenon_sound_init(void)
{
}

__attribute__((weak)) void xenon_sound_submit(void* pcm, int bytes)
{
    (void)pcm;
    (void)bytes;
}

__attribute__((weak)) void xenon_return_to_xell(void)
{
    exit(0);
}

__attribute__((weak)) void console_init(void)
{
}

__attribute__((weak)) int xenon_ata_init(void)
{
    return 0;
}
