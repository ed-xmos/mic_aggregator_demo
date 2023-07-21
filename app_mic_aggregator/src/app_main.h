// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#pragma once

#include <stdint.h>
#include "app_config.h"

#define NUM_AUDIO_BUFFERS   3

typedef struct audio_frame_t{
    int32_t data[MIC_ARRAY_CONFIG_MIC_COUNT][MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME];
} audio_frame_t;

// Macro to adjust input pad timing for the round trip delay. Supports 0 (default) to 5 core clock cycles
#define PORT_DELAY  0x7007
#define DELAY_SHIFT 0x3
#define set_pad_delay(port, delay)  {__asm__ __volatile__ ("setc res[%0], %1": : "r" (port) , "r" ((delay << 0x3) | PORT_DELAY));}

/* Pad control defines */
#define PAD_CONTROL 0x00000006
#define DRIVE_2MA   0x0
#define DRIVE_4MA   0x1
#define DRIVE_8MA   0x2
#define DRIVE_12MA  0x3
#define DRIVE_SHIFT 20

#define set_pad_drive_strength(port, strength)  {__asm__ __volatile__ ("setc res[%0], %1": : "r" (port) , "r" ((strength << DRIVE_SHIFT) | PAD_CONTROL));}
