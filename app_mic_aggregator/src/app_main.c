// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <xcore/channel.h>
#include <xcore/parallel.h>

#include "app_config.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"

MA_C_API void app_mic_array_init(void);
MA_C_API void app_mic_array_task(chanend_t c_frames_out);
MA_C_API void app_mic_array_assertion_disable(void);

DECLARE_JOB(pdm_mic_16, (chanend_t));
void pdm_mic_16(chanend_t c_mic_array) {
    printf("pdm_mic_16\n");

    app_mic_array_init();
    app_mic_array_assertion_disable();
    app_mic_array_task(c_mic_array);
}

DECLARE_JOB(hub, (chanend_t));
void hub(chanend_t c_mic_array) {
    printf("hub\n");

    int32_t audio_frame[MIC_ARRAY_CONFIG_MIC_COUNT][MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME];

    while(1){

      ma_frame_rx(audio_frame, c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);
      printf("ma_frame_rx: %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", 
        audio_frame[0][0], audio_frame[1][0], audio_frame[2][0], audio_frame[3][0],
        audio_frame[4][0], audio_frame[5][0], audio_frame[6][0], audio_frame[7][0],
        audio_frame[8][0], audio_frame[9][0], audio_frame[10][0], audio_frame[11][0],
        audio_frame[12][0], audio_frame[13][0], audio_frame[14][0], audio_frame[15][0]
        );
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
        PJOB(hub, (c_cross_tile))
        // PJOB(tdm16, ()),
        // PJOB(tdm_master_emulator, ())
    );
}