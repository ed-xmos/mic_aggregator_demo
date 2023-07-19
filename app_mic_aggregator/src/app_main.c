// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>

#include <xcore/channel.h>
#include <xcore/channel_streaming.h>
#include <xcore/parallel.h>
#include <xcore/select.h>
#include <xcore/hwtimer.h>

#include "app_main.h"
#include "mic_array.h"
#include "device_pll_ctrl.h"
#include "mic_array_wrapper.h"
#include "tdm_slave_wrapper.h"
#include "tdm_master_simple.h"
#include "fixed_gain.h"
#include "i2c.h"

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


uint8_t i2c_slave_registers[I2C_CONTROL_NUM_REGISTERS];

// This variable is set to -1 if no current register has been selected.
// If the I2C master does a write transaction to select the register then
// the variable will be updated to the register the master wants to
// read/update.
int current_regnum = -1;
int changed_regnum = -1;


I2C_CALLBACK_ATTR
i2c_slave_ack_t i2c_ack_read_req(void *app_data) {
    i2c_slave_ack_t response = I2C_SLAVE_NACK;

    // If no register has been selected using a previous write
    // transaction the NACK, otherwise ACK
    if (current_regnum != -1) {
        response = I2C_SLAVE_ACK;
    }

    return response;
}

I2C_CALLBACK_ATTR
i2c_slave_ack_t i2c_ack_write_req(void *app_data) {
    // Write requests are always accepted

    return I2C_SLAVE_ACK;
}

I2C_CALLBACK_ATTR
uint8_t i2c_master_req_data(void *app_data) {
    // The master is trying to read, if a register is selected then
    // return the value (other return 0).

    uint8_t data = 0;

    if (current_regnum != -1) {
        data = i2c_slave_registers[current_regnum];
        printf("REGFILE: reg[%d] -> %x\n", current_regnum, data);
    } else {
        data = 0;
    }

    return data;
}

I2C_CALLBACK_ATTR
i2c_slave_ack_t i2c_master_sent_data(void *app_data, uint8_t data) {
    i2c_slave_ack_t response = I2C_SLAVE_NACK;

    printf("i2c_master_sent_data\n");


    // The master is trying to write, which will either select a register
    // or write to a previously selected register
    if (current_regnum != -1) {
        i2c_slave_registers[current_regnum] = data;
        printf("REGFILE: reg[%d] <- %x\n", current_regnum, data);

        // Inform the user application that the register has changed
        changed_regnum = current_regnum;

        // Note that, even on the same tile, we have 8 bytes of channel buffering
        // so this will never block if mic_array is looping
        chanend_t c_i2c_reg = *(chanend_t*)app_data;
        s_chan_out_byte(c_i2c_reg, changed_regnum);
        s_chan_out_byte(c_i2c_reg, data);

        response = I2C_SLAVE_ACK;
    }
    else {
        if (data < I2C_CONTROL_NUM_REGISTERS) {
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
    // The stop_bit function is timing critical. Exit quickly
    // The I2C transaction has completed, clear the regnum

    current_regnum = -1;
}

I2C_CALLBACK_ATTR
int i2c_shutdown(void *app_data) {
    return 0;
}


DECLARE_JOB(i2c_control, (chanend_t));
void i2c_control(chanend_t c_i2c_reg) {
    printf("i2c_control\n");

    port_t p_scl = I2C_CONTROL_SLAVE_SCL;
    port_t p_sda = I2C_CONTROL_SLAVE_SDA;

    i2c_callback_group_t i_i2c = {
        .ack_read_request = (ack_read_request_t) i2c_ack_read_req,
        .ack_write_request = (ack_write_request_t) i2c_ack_write_req,
        .master_requires_data = (master_requires_data_t) i2c_master_req_data,
        .master_sent_data = (master_sent_data_t) i2c_master_sent_data,
        .stop_bit = (stop_bit_t) i2c_stop_bit,
        .shutdown = (shutdown_t) i2c_shutdown,
        .app_data = &c_i2c_reg,
    };

    i2c_slave(&i_i2c, p_scl, p_sda, I2C_CONTROL_SLAVE_ADDRESS);
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