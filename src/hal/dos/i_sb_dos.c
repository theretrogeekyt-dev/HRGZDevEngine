// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Sound Blaster DSP & 8-bit Auto-Init DMA audio driver for MS-DOS (DJGPP).
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__DJGPP__)
#include <pc.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/movedata.h>
#endif

#include "doomdef.h"
#include "i_system.h"
#include "i_sb_dos.h"
#include "../common/i_sound_mixer.h"

#define DMA_BUF_SIZE    2048
#define HALF_BUF_SIZE   1024

#if defined(__DJGPP__)
static uint16_t sb_base = 0x220;
static uint8_t  sb_irq  = 7;
static uint8_t  sb_dma  = 1;
static boolean  sb_active = false;

static _go32_dpmi_seginfo dma_mem_info;
static uint32_t dma_phys_addr = 0;
static int last_played_half = -1;
static uint8_t mix_chunk[HALF_BUF_SIZE];

static void dsp_write(uint8_t val)
{
    for (int i = 0; i < 20000; i++)
    {
        if ((inportb(sb_base + 0x0C) & 0x80) == 0)
        {
            outportb(sb_base + 0x0C, val);
            return;
        }
    }
}

static uint8_t dsp_read(void)
{
    for (int i = 0; i < 20000; i++)
    {
        if (inportb(sb_base + 0x0E) & 0x80)
        {
            return inportb(sb_base + 0x0A);
        }
    }
    return 0;
}

static boolean dsp_reset(uint16_t port)
{
    outportb(port + 0x06, 1);
    delay(1);
    outportb(port + 0x06, 0);

    for (int i = 0; i < 2000; i++)
    {
        if (inportb(port + 0x0E) & 0x80)
        {
            if (inportb(port + 0x0A) == 0xAA)
                return true;
        }
        delay(1);
    }
    return false;
}

static void parse_blaster_env(void)
{
    char* b = getenv("BLASTER");
    if (!b) return;

    char* p = b;
    while (*p)
    {
        while (*p == ' ') p++;
        if (!*p) break;

        char tag = *p++;
        if (tag == 'A' || tag == 'a')
        {
            sb_base = (uint16_t)strtoul(p, &p, 16);
        }
        else if (tag == 'I' || tag == 'i')
        {
            sb_irq = (uint8_t)strtoul(p, &p, 10);
        }
        else if (tag == 'D' || tag == 'd')
        {
            sb_dma = (uint8_t)strtoul(p, &p, 10);
        }
        else
        {
            while (*p && *p != ' ') p++;
        }
    }
}
#endif

boolean I_SB_Init(void)
{
#if defined(__DJGPP__)
    if (sb_active)
        return true;

    parse_blaster_env();

    // Verify DSP reset at target or alternative ports
    if (!dsp_reset(sb_base))
    {
        if (dsp_reset(0x220))
            sb_base = 0x220;
        else if (dsp_reset(0x240))
            sb_base = 0x240;
        else
        {
            printf("I_SB_Init: No Sound Blaster DSP detected.\n");
            return false;
        }
    }

    // Read DSP version
    dsp_write(0xE1);
    uint8_t major = dsp_read();
    uint8_t minor = dsp_read();

    // Allocate 4096 bytes of DOS conventional memory for DMA
    memset(&dma_mem_info, 0, sizeof(dma_mem_info));
    dma_mem_info.size = 4096 / 16;
    if (_go32_dpmi_allocate_dos_memory(&dma_mem_info) != 0)
    {
        printf("I_SB_Init: Failed to allocate conventional memory for DMA.\n");
        return false;
    }

    dma_phys_addr = (uint32_t)dma_mem_info.rm_segment << 4;
    // Align to 64KB physical page boundary
    if ((dma_phys_addr & 0xFFFF) + DMA_BUF_SIZE > 0x10000)
    {
        dma_phys_addr = (dma_phys_addr + 0xFFFF) & 0xFFFF0000;
    }

    // Fill DMA buffer with silence (128)
    uint8_t silence[HALF_BUF_SIZE];
    memset(silence, 128, sizeof(silence));
    dosmemput(silence, HALF_BUF_SIZE, dma_phys_addr);
    dosmemput(silence, HALF_BUF_SIZE, dma_phys_addr + HALF_BUF_SIZE);

    // Setup 8237 DMA Channel 1 for 8-bit Auto-Init playback
    outportb(0x0A, 0x04 | sb_dma); // Mask DMA channel
    outportb(0x0C, 0);             // Clear flip-flop
    outportb(0x0B, 0x48 | 0x10 | sb_dma); // Single-cycle, read transfer, auto-init

    outportb(0x02, (uint8_t)(dma_phys_addr & 0xFF));
    outportb(0x02, (uint8_t)((dma_phys_addr >> 8) & 0xFF));
    outportb(0x83, (uint8_t)((dma_phys_addr >> 16) & 0xFF));

    uint16_t count = DMA_BUF_SIZE - 1;
    outportb(0x03, (uint8_t)(count & 0xFF));
    outportb(0x03, (uint8_t)((count >> 8) & 0xFF));

    outportb(0x0A, sb_dma); // Unmask DMA channel

    // Turn Sound Blaster speaker ON
    dsp_write(0xD1);

    // Set sample rate time constant (11025 Hz)
    dsp_write(0x40);
    dsp_write((uint8_t)(256 - (1000000 / MIXER_SAMPLE_RATE)));

    // Set block transfer size
    dsp_write(0x48);
    dsp_write((uint8_t)((HALF_BUF_SIZE - 1) & 0xFF));
    dsp_write((uint8_t)(((HALF_BUF_SIZE - 1) >> 8) & 0xFF));

    // Start 8-bit Auto-Init DMA playback
    dsp_write(0x1C);

    last_played_half = -1;
    sb_active = true;

    printf("I_SB_Init: Sound Blaster (DSP v%d.%02d) at 0x%X, DMA %d, IRQ %d initialized.\n",
           (int)major, (int)minor, (int)sb_base, (int)sb_dma, (int)sb_irq);
    return true;
#else
    return false;
#endif
}

void I_SB_Shutdown(void)
{
#if defined(__DJGPP__)
    if (!sb_active)
        return;

    // Pause DSP DMA transfer
    dsp_write(0xD0);

    // Turn speaker OFF
    dsp_write(0xD3);

    // Mask DMA channel
    outportb(0x0A, 0x04 | sb_dma);

    if (dma_mem_info.rm_segment)
    {
        _go32_dpmi_free_dos_memory(&dma_mem_info);
        memset(&dma_mem_info, 0, sizeof(dma_mem_info));
    }

    sb_active = false;
#endif
}

void I_SB_Update(void)
{
#if defined(__DJGPP__)
    if (!sb_active)
        return;

    // Read remaining DMA transfer count
    outportb(0x0C, 0);
    uint16_t remain = inportb(0x03);
    remain |= (inportb(0x03) << 8);

    uint32_t current_pos = (DMA_BUF_SIZE - 1) - remain;
    int current_half = (current_pos >= HALF_BUF_SIZE) ? 1 : 0;

    if (current_half != last_played_half)
    {
        // When hardware is playing the second half (half 1), refill the first half (half 0)
        // When hardware is playing the first half (half 0), refill the second half (half 1)
        int fill_half = (current_half == 1) ? 0 : 1;
        uint32_t dest_addr = dma_phys_addr + (fill_half * HALF_BUF_SIZE);

        I_Mixer_Mix8(mix_chunk, HALF_BUF_SIZE);
        dosmemput(mix_chunk, HALF_BUF_SIZE, dest_addr);

        last_played_half = current_half;
    }

    // Acknowledge DSP interrupt if pending
    if (inportb(sb_base + 0x0E) & 0x80)
    {
        inportb(sb_base + 0x0A);
    }
#endif
}

