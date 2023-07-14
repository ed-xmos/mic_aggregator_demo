// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <xcore/channel.h>
#include <xcore/parallel.h>

#include "app_config.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"

void app_mic_array_init( void );
void app_mic_array_task( chanend_t c_frames_out );

DECLARE_JOB(pdm_mic_16, (chanend_t));
void pdm_mic_16(chanend_t c_mic_array) {
    printf("pdm_mic_16\n");

    app_mic_array_init();
    app_mic_array_task(c_mic_array);
}

DECLARE_JOB(hub, (chanend_t));
void hub(chanend_t c_mic_array) {
    printf("hub\n");

    int32_t audio_frame[MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME * MIC_ARRAY_CONFIG_MIC_COUNT];

    while(1){

      ma_frame_rx(audio_frame, c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);
      printf("ma_frame_rx\n");
  }
}

DECLARE_JOB(tdm16, (void));
void tdm16(void) {
    printf("tdm16\n");
}

DECLARE_JOB(tdm_master_emulator, (void));
void tdm_master_emulator(void) {
    printf("tdm_master_emulator\n");
}




///////// Tile main functions ///////////

void main_tile_0(chanend_t c_cross_tile){
    printf("Hello world tile[0]\n");

    PAR_JOBS(
        PJOB(pdm_mic_16, (c_cross_tile))
    );
}

void main_tile_1(chanend_t c_cross_tile){
    printf("Hello world tile[1]\n");

    device_pll_init();

    PAR_JOBS(
        PJOB(hub, (c_cross_tile)),
        PJOB(tdm16, ()),
        PJOB(tdm_master_emulator, ())
    );
}