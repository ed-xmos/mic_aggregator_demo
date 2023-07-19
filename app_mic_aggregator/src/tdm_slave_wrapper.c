// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>

#include "tdm_slave_wrapper.h"

I2S_CALLBACK_ATTR
void i2s_init(void *app_data, i2s_config_t *i2s_config)
{
    printf("i2s_init\n");

    (void) app_data;
    (void) i2s_config;

}

I2S_CALLBACK_ATTR
void i2s_send(void *app_data, size_t n, int32_t *send_data)
{
    audio_frame_t **read_buffer_ptr = (audio_frame_t **)app_data;
    audio_frame_t *read_buffer = *read_buffer_ptr; // Make copy just in case mics move on buffer halfway through frame

    if(read_buffer != NULL){
        for(int ch = 0; ch < 16; ch++){
            send_data[ch] = read_buffer->data[ch][0];
        }
    } else {
        for(int ch = 0; ch < 16; ch++){
            send_data[ch] = 0;
        }
    }
}

I2S_CALLBACK_ATTR
i2s_restart_t i2s_restart_check(void *app_data)
{
    return I2S_NO_RESTART;
}


void tdm16_slave(audio_frame_t **read_buffer_ptr) {
    printf("tdm16_slave\n");

    i2s_tdm_ctx_t ctx;
    i2s_callback_group_t i_i2s = {
            .init = (i2s_init_t) i2s_init,
            .restart_check = (i2s_restart_check_t) i2s_restart_check,
            .receive = NULL,
            .send = (i2s_send_t) i2s_send,
            .app_data = (void*)read_buffer_ptr,
    };

    port_t p_bclk = TDM_SLAVEPORT_BCLK;
    port_t p_fsync = TDM_SLAVEPORT_FSYNCH;
    port_t p_dout = TDM_SLAVEPORT_OUT;

    xclock_t bclk = TDM_SLAVEPORT_CLK_BLK;

    i2s_tdm_slave_tx_16_init(
        &ctx,
        &i_i2s,
        p_dout,
        p_fsync,
        p_bclk,
        bclk,
        TDM_SLAVETX_OFFSET,
        I2S_SLAVE_SAMPLE_ON_BCLK_RISING);

    i2s_tdm_slave_tx_16_thread(&ctx);
}