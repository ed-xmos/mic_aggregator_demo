// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <stdlib.h>
#include <xcore/channel.h>
#include <xcore/parallel.h>
#include <xcore/hwtimer.h>

#include "app_config.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"
#include "mic_array_wrapper.h"
#include "i2s_tdm_slave.h"

#define NUM_AUDIO_BUFFERS   3
typedef struct audio_frame_t{
    int32_t data[MIC_ARRAY_CONFIG_MIC_COUNT][MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME];
    } audio_frame_t;
volatile audio_frame_t *read_buffer = NULL;

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

DECLARE_JOB(monitor, (void));
void monitor(void) {
    printf("monitor\n");

    hwtimer_t tmr = hwtimer_alloc();


    while(1){
        hwtimer_delay(tmr, XS1_TIMER_KHZ * 10);
        audio_frame_t *audio_frame = (audio_frame_t *)read_buffer;
        printf("ma_frame_rx: 0x%p %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n", audio_frame,
          audio_frame->data[0][0], audio_frame->data[1][0], audio_frame->data[2][0], audio_frame->data[3][0],
          audio_frame->data[4][0], audio_frame->data[5][0], audio_frame->data[6][0], audio_frame->data[7][0],
          audio_frame->data[8][0], audio_frame->data[9][0], audio_frame->data[10][0], audio_frame->data[11][0],
          audio_frame->data[12][0], audio_frame->data[13][0], audio_frame->data[14][0], audio_frame->data[15][0]
          );
    }
}


DECLARE_JOB(hub, (chanend_t));
void hub(chanend_t c_mic_array) {
    printf("hub\n");

    unsigned write_buffer_idx = 0;
    audio_frame_t audio_frames[NUM_AUDIO_BUFFERS] = {{{{0}}}};

    printf("ptrs: %p %p %p\n", audio_frames[0].data, audio_frames[1].data, audio_frames[2].data);

    while(1){

        read_buffer = &audio_frames[write_buffer_idx];
        ma_frame_rx((int32_t*)read_buffer, c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);

        write_buffer_idx++;
        if(write_buffer_idx == NUM_AUDIO_BUFFERS){
            write_buffer_idx = 0;
        }
    }
}


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
    audio_frame_t *curr_read_buffer = (audio_frame_t *)read_buffer; // Make copy just in case mics move on buffer halfway through frame

    // printf("i2s_send %p\n", curr_read_buffer);

    if(curr_read_buffer != NULL)
    for(int ch = 0; ch < 16; ch++){
        send_data[ch] = curr_read_buffer->data[ch][0];
    }
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
        I2S_SLAVE_SAMPLE_ON_BCLK_RISING,
        NULL);

    i2s_tdm_slave_tx_16_thread(&ctx);
}

DECLARE_JOB(tdm_master_emulator, (void));
void tdm_master_emulator(void) {
    printf("tdm_master_emulator\n");

    port_t p_fsynch_master = TDM_MASTER_EMULATOR_FSYNCH;
    port_t p_data_in_master = TDM_MASTER_EMULATOR_DATA;
    xclock_t tdm_master_clk = TDM_MASTER_CLK_BLK;

    clock_enable(tdm_master_clk);
    clock_set_source_port(tdm_master_clk, TDM_SLAVEPORT_BCLK);
    clock_set_divide(tdm_master_clk, 0);

    port_enable(p_fsynch_master);
    port_start_buffered(p_fsynch_master, 32);
    port_clear_buffer(p_fsynch_master);
    port_set_clock(p_fsynch_master, tdm_master_clk);
    // port_out(p_fsynch_master, 0x00000000);

    port_enable(p_data_in_master);
    port_start_buffered(p_data_in_master, 32);
    // port_in(p_data_in_master); // Dummy read
    port_set_clock(p_data_in_master, tdm_master_clk);
    port_clear_buffer(p_data_in_master);

    int32_t rx_data[16] = {0};

    clock_start(tdm_master_clk);

    while(1){
        port_out(p_fsynch_master, 0x00000001);

        // rx_data[0] = port_in(p_data_in_master);
        for(int i = 1; i < 16; i++){
            port_out(p_fsynch_master, 0x00000000);
            // rx_data[i] = port_in(p_data_in_master);
        }
    }
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
        PJOB(tdm16, ()),
        PJOB(tdm_master_emulator, ()),
        PJOB(monitor, ())
    );
}