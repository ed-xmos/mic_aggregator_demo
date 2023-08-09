// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <xcore/channel.h>
#include <xcore/port.h>
#include <xcore/parallel.h>
#include <xcore/hwtimer.h>

#include "app_config.h"

#include "xud.h"
#include "xua.h"
#include "xua_commands.h"

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

// TODO unglobalise
const size_t num_ep_out = 2;
XUD_EpType epTypeTableOut[num_ep_out] = {XUD_EPTYPE_CTL | XUD_STATUS_ENABLE, XUD_EPTYPE_ISO};

channel_t c_ep_out[num_ep_out];
chanend_t chanend_ep_out[num_ep_out];

const size_t num_ep_in = 3;
XUD_EpType epTypeTableIn[num_ep_in] = {XUD_EPTYPE_CTL | XUD_STATUS_ENABLE, XUD_EPTYPE_ISO, XUD_EPTYPE_ISO};

channel_t c_ep_in[num_ep_in];
chanend_t chanend_ep_in[num_ep_in];

channel_t c_sof;
chanend_t c_aud_g;
channel_t c_aud_ctl;

port_t p_for_mclk_count;


DECLARE_JOB(xud_wrapper, (void));
void xud_wrapper(void){
    hwtimer_realloc_xc_timer();
    XUD_Main(chanend_ep_out, num_ep_out, chanend_ep_in, num_ep_in,
             c_sof.end_a, epTypeTableOut, epTypeTableIn, 
             XUD_SPEED_HS, XUD_PWR_SELF);
    hwtimer_free_xc_timer();
}

DECLARE_JOB(ep0_wrapper, (void));
void ep0_wrapper(void){
    XUA_Endpoint0(c_ep_out[0].end_b, c_ep_in[0].end_b, c_aud_ctl.end_a, 0, 0, 0, 0);
}

DECLARE_JOB(buffer_wrapper, (void));
void buffer_wrapper(void){
    XUA_Buffer(c_ep_out[1].end_b, c_ep_in[2].end_b, c_ep_in[1].end_b, c_sof.end_b, c_aud_ctl.end_b, p_for_mclk_count, c_aud_g);
}


void xua_wrapper(chanend_t c_aud) {
    printf("xua_wrapper\n");

    for(int i = 0; i < num_ep_out; i++){
        c_ep_out[i] = chan_alloc();
        chanend_ep_out[i] = c_ep_out[i].end_a;
    }
 
    for(int i = 0; i < num_ep_in; i++){
        c_ep_in[i] = chan_alloc();
        chanend_ep_in[i] = c_ep_in[i].end_a;
    }
 
    c_sof = chan_alloc();
    c_aud_ctl = chan_alloc();
    c_aud_g = c_aud;

    /* Declare and enable internal MCLK counting port */
    p_for_mclk_count = PORT_MCLK_COUNT;
    port_enable(p_for_mclk_count);

    /* Connect mclk_count clock-block to mclk_in pin */
    port_t p_for_mclk_in = USB_MCLK_IN;
    port_enable(p_for_mclk_in);
    xclock_t usb_mclk_in_clk = USB_MCLK_COUNT_CLK_BLK;
    clock_enable(usb_mclk_in_clk);
    clock_set_source_port(usb_mclk_in_clk, p_for_mclk_in);
    port_set_clock(p_for_mclk_count, usb_mclk_in_clk);
    clock_start(usb_mclk_in_clk);

    PAR_JOBS(
        PJOB(xud_wrapper, ()),
        PJOB(ep0_wrapper, ()),
        PJOB(buffer_wrapper, ())
    );
}

void xua_exchange(chanend_t c_aud, int32_t samples[NUM_USB_CHAN_IN]){
    chanend_out_word(c_aud, 0);
    int isct = chanend_test_control_token_next_byte(c_aud);
    if(isct){
        char ct = chanend_in_control_token(c_aud);
        printf("ct: %d\n", ct);
        if(ct == SET_SAMPLE_FREQ)
        {
            chanend_in_word(c_aud);
        }
        return;
    }

    const unsigned loops = (NUM_USB_CHAN_OUT > 0) ? NUM_USB_CHAN_OUT : 1; 
    for(int i = 0; i < loops; i++)
    {
        chanend_in_word(c_aud);
    }

    for(int i = 0; i < NUM_USB_CHAN_IN; i++)
    {
        chanend_out_word(c_aud, samples[i]);
        // chanend_out_word(c_aud, 0x20000000);
    }
}
