// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <limits.h>

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
#include "i2c_control.h"

#include "xud.h"
#include "xua.h"


DECLARE_JOB(pdm_mic_16, (chanend_t));
void pdm_mic_16(chanend_t c_mic_array) {
    printf("pdm_mic_16 running: %d threads total\n", MIC_ARRAY_PDM_RX_OWN_THREAD + MIC_ARRAY_NUM_DECIMATOR_TASKS);

    app_mic_array_init();
    // app_mic_array_assertion_disable();
    app_mic_array_assertion_enable();   // Inform if timing is not met
    app_mic_array_task(c_mic_array);
}

DECLARE_JOB(pdm_mic_16_front_end, (void));
void pdm_mic_16_front_end(void) {
    printf("pdm_mic_16_front_end\n");

    app_pdm_rx_task();
}

static inline int32_t scalar_gain(int32_t samp, int32_t gain){
    int64_t accum = (int64_t)samp * (int32_t)gain;
    accum = accum > INT_MAX ? INT_MAX : accum;
    accum = accum < INT_MIN ? INT_MIN : accum;

    return (int32_t)accum;
}


DECLARE_JOB(hub, (chanend_t, chanend_t, audio_frame_t **));
void hub(chanend_t c_mic_array, chanend_t c_i2c_reg, audio_frame_t **read_buffer_ptr) {
    printf("hub\n");

    unsigned write_buffer_idx = 0;
    audio_frame_t audio_frames[NUM_AUDIO_BUFFERS] = {{{{0}}}};

    uint16_t gains[MIC_ARRAY_CONFIG_MIC_COUNT] = {MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT,
                                                  MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT,
                                                  MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT,
                                                  MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT, MIC_GAIN_INIT};  
    while(1){
        ma_frame_rx((int32_t*)&audio_frames[write_buffer_idx], c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);

        // Apply gain
        for(int ch = 0; ch < MIC_ARRAY_CONFIG_MIC_COUNT; ch++){
            audio_frames[write_buffer_idx].data[ch][0] = scalar_gain(audio_frames[write_buffer_idx].data[ch][0], gains[ch]);
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
                uint8_t channel = s_chan_in_byte(c_i2c_reg);
                uint8_t data_h = s_chan_in_byte(c_i2c_reg);
                uint8_t data_l = s_chan_in_byte(c_i2c_reg);

                int32_t gain = U16_FROM_BYTES(data_h, data_l);
                gains[channel] = gain;
            }
            break;

            drop_through:
            {
                // Do nothing & fall-through
            }
            break;
        }
        // There are currently around 1600 ticks (16us) of slack at the end of this loop
    }
}

extern void XUA_Endpoint0(  chanend_t c_ep0_out,
                            chanend_t c_ep0_in,
                            chanend_t c_audioCtrl,
                            unsigned int c_mix_ctl,
                            unsigned int c_clk_ctl,
                            unsigned int c_EANativeTransport_ctrl,
                            unsigned int dfuInterface
);


extern void XUA_Buffer(
            chanend_t c_aud_out,
            chanend_t c_aud_in,
            chanend_t c_aud_fb,
            chanend_t c_sof,
            chanend_t c_aud_ctl,
            port_t p_off_mclk,
            chanend_t c_aud
);

DECLARE_JOB(xua_wrapper, (void));
void xua_wrapper(void) {
    printf("xua_wrapper\n");

    /* Endpoint type tables - informs XUD what the transfer types for each Endpoint in use and also
     * if the endpoint wishes to be informed of USB bus resets */
    const size_t num_ep_out = 2;
    XUD_EpType epTypeTableOut[num_ep_out] = {XUD_EPTYPE_CTL | XUD_STATUS_ENABLE, XUD_EPTYPE_ISO};

    channel_t c_ep_out[num_ep_out];
    chanend_t chanend_ep_out[num_ep_out];
    for(int i = 0; i < num_ep_out; i++){
        c_ep_out[i] = chan_alloc();
        chanend_ep_out[i] = c_ep_out[i].end_a;
    }
 
    const size_t num_ep_in = 3;
    XUD_EpType epTypeTableIn[num_ep_in] = {XUD_EPTYPE_CTL | XUD_STATUS_ENABLE, XUD_EPTYPE_ISO, XUD_EPTYPE_ISO};

    channel_t c_ep_in[num_ep_in];
    chanend_t chanend_ep_in[num_ep_in];
    for(int i = 0; i < num_ep_in; i++){
        c_ep_in[i] = chan_alloc();
        chanend_ep_in[i] = c_ep_in[i].end_a;
    }
 
    /* Channel for communicating SOF notifications from XUD to the Buffering cores */
    channel_t c_sof = chan_alloc();

    /* Channel for audio data between buffering cores and AudioHub/IO core */
    channel_t c_aud = chan_alloc();
    
    /* Channel for communicating control messages from EP0 to the rest of the device (via the buffering cores) */
    channel_t c_aud_ctl = chan_alloc();

    XUD_Main(chanend_ep_out, num_ep_out, chanend_ep_in, num_ep_in,
             c_sof.end_a, epTypeTableOut, epTypeTableIn, 
             XUD_SPEED_HS, XUD_PWR_SELF);


    XUA_Endpoint0(c_ep_out[0].end_b, c_ep_in[0].end_b, c_aud_ctl.end_a, 0, 0, 0, 0);

    port_t p_for_mclk_count = XS1_PORT_32A;
    port_enable(p_for_mclk_count);
    /* Connect master-clock clock-block to clock-block pin */
    // set_clock_src(clk_audio_mclk_usb, p_mclk_in_usb);           /* Clock clock-block from mclk pin */
    // set_port_clock(p_for_mclk_count, clk_audio_mclk_usb);       /* Clock the "count" port from the clock block */
    // start_clock(clk_audio_mclk_usb);                            /* Set the clock off running */

    XUA_Buffer(c_ep_out[1].end_b, c_ep_in[2].end_b, c_ep_in[1].end_b, c_sof.end_b, c_aud_ctl.end_b, p_for_mclk_count, c_aud.end_a);

    
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

    // Enable and setup the 24.576MHz APP PLL which is used as BCLK and prescaled as PDM clock
    port_t p_app_pll_out = MIC_ARRAY_CONFIG_PORT_MCLK;
    port_enable(p_app_pll_out);
    set_pad_drive_strength(p_app_pll_out, DRIVE_12MA);
    device_pll_init();

    PAR_JOBS(
        PJOB(hub, (c_cross_tile[0], c_cross_tile[1], read_buffer_ptr)),
        PJOB(tdm16_slave, (read_buffer_ptr)),
        PJOB(tdm16_master_simple, ()),
        PJOB(xua_wrapper, ()),
        PJOB(tdm_master_monitor, ()) // Temp monitor for checking reception of TDM frames. Separate task so non-intrusive
    );
}