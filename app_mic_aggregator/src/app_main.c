// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <xcore/channel.h>
#include <xcore/parallel.h>

#include "app_config.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"
#include "mic_array_wrapper.h"
#include "i2s_tdm_slave.h"



DECLARE_JOB(pdm_mic_16, (chanend_t));
void pdm_mic_16(chanend_t c_mic_array) {
    printf("pdm_mic_16\n");

    app_mic_array_init();
    app_mic_array_assertion_disable();
    app_mic_array_task(c_mic_array);
}

DECLARE_JOB(pdm_mic_16_front_end, (void));
void pdm_mic_16_front_end(void) {
    printf("pdm_mic_16_front_end\n");

    app_pdm_rx_task();
}


DECLARE_JOB(hub, (chanend_t));
void hub(chanend_t c_mic_array) {
    printf("hub\n");

    int32_t audio_frame[MIC_ARRAY_CONFIG_MIC_COUNT][MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME];

    while(1){

      ma_frame_rx((int32_t*)audio_frame, c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);
      printf("ma_frame_rx: %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", 
        audio_frame[0][0], audio_frame[1][0], audio_frame[2][0], audio_frame[3][0],
        audio_frame[4][0], audio_frame[5][0], audio_frame[6][0], audio_frame[7][0],
        audio_frame[8][0], audio_frame[9][0], audio_frame[10][0], audio_frame[11][0],
        audio_frame[12][0], audio_frame[13][0], audio_frame[14][0], audio_frame[15][0]
        );
  }
}


I2S_CALLBACK_ATTR
void i2s_init(void *app_data, i2s_config_t *i2s_config)
{
    (void) app_data;
    (void) i2s_config;

}

I2S_CALLBACK_ATTR
void i2s_send(void *app_data, size_t n, int32_t *send_data)
{
    static int32_t cnt = 0;

    if (cnt == 500) {
        printf("TDM iters reached..\n");
        exit(0);
    }

    cnt++;
}

I2S_CALLBACK_ATTR
i2s_restart_t i2s_restart_check(void *app_data)
{
    return I2S_NO_RESTART;
}


DECLARE_JOB(tdm16, (void));
void tdm16(void) {
    printf("tdm16\n");

    i2s_tdm_ctx_t ctx;
    i2s_callback_group_t i_i2s = {
            .init = (i2s_init_t) i2s_init,
            .restart_check = (i2s_restart_check_t) i2s_restart_check,
            .receive = NULL,
            .send = (i2s_send_t) i2s_send,
            .app_data = NULL,
    };

    port_t p_bclk = TDM_PORT_BCLK;
    port_t p_fsync = TDM_PORT_FSYNCH;
    port_t p_dout = TDM_PORT_OUT;

    xclock_t bclk = TDM_PORT_CLK_BLK;

    i2s_tdm_slave_tx_16_init(
        &ctx,
        &i_i2s,
        p_dout,
        p_fsync,
        p_bclk,
        bclk,
        TDM_TX_OFFSET,
        I2S_SLAVE_SAMPLE_ON_BCLK_RISING,
        NULL);
        
}

DECLARE_JOB(tdm_master_emulator, (void));
void tdm_master_emulator(void) {
    printf("tdm_master_emulator\n");
}




///////// Tile main functions where we par off the threads ///////////

void main_tile_0(chanend_t c_cross_tile){
    printf("Hello world tile[0]\n");

    PAR_JOBS(
        PJOB(pdm_mic_16, (c_cross_tile)),
        PJOB(pdm_mic_16_front_end, ())
    );
}

void main_tile_1(chanend_t c_cross_tile){
    printf("Hello world tile[1]\n");

    device_pll_init();

    PAR_JOBS(
        PJOB(hub, (c_cross_tile)),
        PJOB(tdm16, ())
        // PJOB(tdm_master_emulator, ())
    );
}