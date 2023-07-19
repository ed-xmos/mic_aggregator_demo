// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <stdlib.h>

#include <xcore/channel.h>
#include <xcore/parallel.h>
#include <xcore/hwtimer.h>
#include <xscope.h>
#include <xclib.h>
#include <print.h>

#include "app_config.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"
#include "mic_array_wrapper.h"
#include "i2s_tdm_slave.h"
#include "i2c.h"

#define NUM_AUDIO_BUFFERS   3

typedef struct audio_frame_t{
    int32_t data[MIC_ARRAY_CONFIG_MIC_COUNT][MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME];
} audio_frame_t;

volatile audio_frame_t *read_buffer = NULL;
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

extern volatile int32_t t_dec_exec;
extern volatile int32_t t_dec_per;

DECLARE_JOB(monitor_tile0, (void));
void monitor_tile0(void) {
    printf("monitor_tile0\n");

    hwtimer_t tmr = hwtimer_alloc();

    while(1){
        hwtimer_delay(tmr, XS1_TIMER_KHZ * 5000);
        printf("dec period: %ld exec: %ld\n", t_dec_per, t_dec_exec);
    }
}

int32_t rx_data[16] = {0};

DECLARE_JOB(monitor_tile1, (void));
void monitor_tile1(void) {
    printf("monitor_tile1\n");

    hwtimer_t tmr = hwtimer_alloc();

    while(1){
        hwtimer_delay(tmr, XS1_TIMER_KHZ * 10000);
        printf("tdm_rx: %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
          rx_data[0], rx_data[1], rx_data[2], rx_data[3],
          rx_data[4], rx_data[5], rx_data[6], rx_data[7],
          rx_data[8], rx_data[9], rx_data[10], rx_data[11],
          rx_data[12], rx_data[13], rx_data[14], rx_data[15]
          );
    }
}


DECLARE_JOB(hub, (chanend_t));
void hub(chanend_t c_mic_array) {
    printf("hub\n");

    unsigned write_buffer_idx = 0;
    audio_frame_t audio_frames[NUM_AUDIO_BUFFERS] = {{{{0}}}};

    printf("ptrs: %p %p %p\n", audio_frames[0].data, audio_frames[1].data, audio_frames[2].data);

    int32_t old_t = 0;
    while(1){
        int32_t t0 = get_reference_time();
        timing = t0 - old_t;
        old_t = t0;        
        ma_frame_rx((int32_t*)&audio_frames[write_buffer_idx], c_mic_array, MIC_ARRAY_CONFIG_MIC_COUNT, MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME);

        for(int i = 0; i < 16; i++){
            audio_frames[write_buffer_idx].data[i][0] <<= 9; // Simple gain
        }

        read_buffer = &audio_frames[write_buffer_idx];

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

#define NUM_REGISTERS 10
uint8_t registers[NUM_REGISTERS];

// This variable is set to -1 if no current register has been selected.
// If the I2C master does a write transaction to select the register then
// the variable will be updated to the register the master wants to
// read/update.
int current_regnum = -1;
int changed_regnum = -1;

/*
// Handle application requests to get/set register values.
case app.set_register(int regnum, uint8_t data):
  if (regnum >= 0 && regnum < NUM_REGISTERS) {
    registers[regnum] = data;
  }
  break;
case app.get_register(int regnum) -> uint8_t data:
  if (regnum >= 0 && regnum < NUM_REGISTERS) {
    data = registers[regnum];
  } else {
    data = 0;
  }
  break;
case app.get_changed_regnum() -> unsigned regnum:
  regnum = changed_regnum;
  break;
  */


I2C_CALLBACK_ATTR
i2c_slave_ack_t i2c_ack_read_req(void *app_data) {
    printstr("i2c_ack_read_req\n");

    i2c_slave_ack_t response = I2C_SLAVE_NACK;

    // If no register has been selected using a previous write
    // transaction the NACK, otherwise ACK
    if (current_regnum == -1) {
      response = I2C_SLAVE_NACK;
    } else {
      response = I2C_SLAVE_ACK;
    }

    return response;
}

I2C_CALLBACK_ATTR
i2c_slave_ack_t i2c_ack_write_req(void *app_data) {
    printstr("i2c_ack_write_req\n");

    // Write requests are always accepted

    return I2C_SLAVE_ACK;
}

I2C_CALLBACK_ATTR
uint8_t i2c_master_req_data(void *app_data) {
    // The master is trying to read, if a register is selected then
    // return the value (other return 0).

    uint8_t data = 0;

    if (current_regnum != -1) {
        data = registers[current_regnum];
        printf("REGFILE: reg[%d] -> %x\n", current_regnum, data);
    } else {
        data = 0;
    }

    return data;
}

I2C_CALLBACK_ATTR
i2c_slave_ack_t i2c_master_sent_data(void *app_data, uint8_t data) {
    printf("xCORE i2c_master_sent_data: %u\n", data);

    i2c_slave_ack_t response = I2C_SLAVE_NACK;

    // The master is trying to write, which will either select a register
          // or write to a previously selected register
          if (current_regnum != -1) {
            registers[current_regnum] = data;
            printf("REGFILE: reg[%d] <- %x\n", current_regnum, data);

            // Inform the user application that the register has changed
            changed_regnum = current_regnum;
            // app.register_changed();

            response = I2C_SLAVE_ACK;
          }
          else {
            if (data < NUM_REGISTERS) {
              current_regnum = data;
              printf("REGFILE: select reg[%d]\n", current_regnum);
              response = I2C_SLAVE_ACK;
            } else {
              response = I2C_SLAVE_NACK;
            }
          }

    return response;
}

I2C_CALLBACK_ATTR
void i2c_stop_bit(void *app_data) {
    // The stop_bit function is timing critical. Needs to use printstr to meet
    // timing and detect the start bit
    printstr("i2c_stop_bit\n");

    // The I2C transaction has completed, clear the regnum
    printf("REGFILE: stop_bit\n");
    current_regnum = -1;
}

I2C_CALLBACK_ATTR
int i2c_shutdown(void *app_data) {
    return 0;
}

#define DEVICE_ADDR  0x3c


DECLARE_JOB(i2c_control, (void));
void i2c_control(void) {

    port_t p_scl = XS1_PORT_1A;
    port_t p_sda = XS1_PORT_1B;

    i2c_callback_group_t i_i2c = {
        .ack_read_request = (ack_read_request_t) i2c_ack_read_req,
        .ack_write_request = (ack_write_request_t) i2c_ack_write_req,
        .master_requires_data = (master_requires_data_t) i2c_master_req_data,
        .master_sent_data = (master_sent_data_t) i2c_master_sent_data,
        .stop_bit = (stop_bit_t) i2c_stop_bit,
        .shutdown = (shutdown_t) i2c_shutdown,
        .app_data = NULL,
    };

    i2c_slave(&i_i2c, p_scl, p_sda, DEVICE_ADDR);
}


DECLARE_JOB(tdm16_master_simple, (void));
void tdm16_master_simple(void) {
    printf("tdm16_master_simple\n");

    port_t p_fsynch_master = TDM_SIMPLE_MASTER_FSYNCH;
    port_t p_data_in_master = TDM_SIMPLE_MASTER_DATA;
    xclock_t tdm_master_clk = TDM_SIMPLE_MASTER_CLK_BLK;

    const int offset = 1;

    clock_enable(tdm_master_clk);
    clock_set_source_port(tdm_master_clk, TDM_SLAVEPORT_BCLK);
    clock_set_divide(tdm_master_clk, 0);

    // Buffered Output ports:
    // Outputs pin from the LSb of the shift register and shifts right
    // Writes to the transfer register to shift register as soon as there is space 

    const int32_t fsynch_bit_pattern = 0x00000001; // Bit stream of up to 32 BCLK periods, 0x3.. 0x7 etc..
    port_enable(p_fsynch_master);
    port_start_buffered(p_fsynch_master, 32);
    port_clear_buffer(p_fsynch_master);
    port_set_clock(p_fsynch_master, tdm_master_clk);

    port_set_trigger_time(p_fsynch_master, 1);
    port_out(p_fsynch_master, fsynch_bit_pattern);

    // Buffered Input ports:
    // Pin inputs to the MSb and then shifts right
    // Copies to the transfer register when fully shifted

    // Macro to adjust pad timing for the round trip delay
    #define set_pad_delay(port, delay)  {__asm__ __volatile__ ("setc res[%0], %1": : "r" (port) , "r" ((delay << 3) | 0x7007));}

    port_enable(p_data_in_master);
    port_start_buffered(p_data_in_master, 32);
    port_set_clock(p_data_in_master, tdm_master_clk);
    port_clear_buffer(p_data_in_master);
    port_set_trigger_time(p_data_in_master, 32 + 1 + offset);
    set_pad_delay(p_data_in_master, 4); // 2,3,4,5 work. 6 not settable. 1 Does not work. So choose 4 as midpoint

    clock_start(tdm_master_clk);

    while(1){
        for(int i = 0; i < 15; i++){

            port_out(p_fsynch_master, 0x00000000);
            if(i && i < 3){ // Output first two channels only due to performance limit of xscope
                xscope_int(i - 1, rx_data[i - 1]);
            }
            rx_data[i] = bitrev(port_in(p_data_in_master));
        }

        port_out(p_fsynch_master, fsynch_bit_pattern);
        rx_data[15] = bitrev(port_in(p_data_in_master));
    }
}




///////// Tile main functions where we par off the threads ///////////

void main_tile_0(chanend_t c_cross_tile){
    printf("Hello world tile[0]\n");

    PAR_JOBS(
        PJOB(pdm_mic_16, (c_cross_tile)),
        PJOB(pdm_mic_16_front_end, ()),
        PJOB(monitor_tile0, ())
    );
}

void main_tile_1(chanend_t c_cross_tile){
    printf("Hello world tile[1]\n");

    device_pll_init();

    PAR_JOBS(
        PJOB(hub, (c_cross_tile)),
        PJOB(tdm16, ()),
        PJOB(tdm16_master_simple, ()),
        PJOB(i2c_control, ()),
        PJOB(monitor_tile1, ())
    );
}