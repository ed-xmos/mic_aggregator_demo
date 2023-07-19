// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>

#include <xcore/channel.h>
#include <xcore/channel_streaming.h>
#include <xcore/parallel.h>
#include <xcore/select.h>
#include <xcore/hwtimer.h>
#include <print.h>

#include "app_main.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"
#include "mic_array_wrapper.h"
#include "tdm_slave_wrapper.h"
#include "tdm_master_simple.h"
#include "fixed_gain.h"
#include "i2c_control.h"

volatile int32_t timing = 0;

DECLARE_JOB(pdm_mic_16, (chanend_t));
void pdm_mic_16(chanend_t c_mic_array) {
    printf("pdm_mic_16 running: %d threads total\n", MIC_ARRAY_PDM_RX_OWN_THREAD + MIC_ARRAY_NUM_DECIMATOR_TASKS);

    app_mic_array_init();
    // app_mic_array_assertion_disable();
    app_mic_array_assertion_enable();
    app_mic_array_task(c_mic_array);
}

DECLARE_JOB(pdm_mic_16_front_end, (void));
void pdm_mic_16_front_end(void) {
    printf("pdm_mic_16_front_end\n");

    app_pdm_rx_task();
}


DECLARE_JOB(hub, (chanend_t, chanend_t, audio_frame_t **));
void hub(chanend_t c_mic_array, chanend_t c_i2c_reg, audio_frame_t **read_buffer_ptr) {
    printf("hub\n");

    unsigned write_buffer_idx = 0;
    audio_frame_t audio_frames[NUM_AUDIO_BUFFERS] = {{{{0}}}};

    fixed_gain_t fg;
    fixed_gain_set_multiplier(&fg, 1, VECTOR_SIZE);

    int32_t old_t = 0;
    while(1){
        int32_t t0 = get_reference_time();
        timing = t0 - old_t;
        old_t = t0;        
        ma_frame_rx((int32_t*)&audio_frames[write_buffer_idx], c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);

        for(int i = 0; i < 16; i += 8){
            fixed_gain_apply(&fg, &audio_frames[write_buffer_idx].data[i][0]);
        }

        *read_buffer_ptr = &audio_frames[write_buffer_idx];  // update read buffer for TDM

        write_buffer_idx++;
        if(write_buffer_idx == NUM_AUDIO_BUFFERS){
            write_buffer_idx = 0;
        }

        // Handle any control updates from the host
        // Non-blocking channel read on c_i2c_reg
        SELECT_RES(
            CASE_THEN(c_i2c_reg, i2c_register_write),
            DEFAULT_THEN(drop_through)
        )
        {
            i2c_register_write:
            {
                uint8_t reg_num = s_chan_in_byte(c_i2c_reg);
                uint8_t reg_data = s_chan_in_byte(c_i2c_reg);

                (void) reg_num;
                (void) reg_data;
                printchar('*');
            }
            break;

            drop_through:
            {
                // Do nothing & fall-through
            }
            break;
        }
    }
}


// Debug task only
extern volatile int32_t t_dec_exec;
extern volatile int32_t t_dec_per;
volatile int32_t samp;

DECLARE_JOB(monitor_tile0, (void));
void monitor_tile0(void) {
    printf("monitor_tile0\n");

    hwtimer_t tmr = hwtimer_alloc();

    while(1){
        hwtimer_delay(tmr, XS1_TIMER_KHZ * 1000);
        // printf("dec period: %ld exec: %ld\n", t_dec_per, t_dec_exec);
    }
}



///////// Tile main functions where we par off the threads ///////////

void main_tile_0(chanend_t c_cross_tile[2]){
    PAR_JOBS(
        PJOB(pdm_mic_16, (c_cross_tile[0])), // Note spawns MIC_ARRAY_NUM_DECIMATOR_TASKS threads
        PJOB(pdm_mic_16_front_end, ()),
        PJOB(i2c_control, (c_cross_tile[1])),
        PJOB(monitor_tile0, ())
    );
}

void main_tile_1(chanend_t c_cross_tile[2]){
    // Pointer to pointer for sharing the mic_array read_buffer last write
    audio_frame_t *read_buffer = NULL;
    audio_frame_t **read_buffer_ptr = &read_buffer;

    device_pll_init();

    PAR_JOBS(
        PJOB(hub, (c_cross_tile[0], c_cross_tile[1], read_buffer_ptr)),
        PJOB(tdm16_slave, (read_buffer_ptr)),
        PJOB(tdm16_master_simple, ()),
        PJOB(tdm_master_monitor, ()) // Temp monitor for checking reception of TDM frames. Separate task so non-intrusive
    );
}