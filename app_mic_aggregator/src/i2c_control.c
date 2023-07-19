// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <xcore/parallel.h>
#include <xcore/channel.h>

#include "app_config.h"
#include "i2c.h"


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
        // printf("REGFILE: reg[%d] -> %x\n", current_regnum, data);
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
        // printf("REGFILE: reg[%d] <- %x\n", current_regnum, data);

        // Inform the user application that the register has changed
        changed_regnum = current_regnum;

        // Forward command to hub on the other tile
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
            // printf("REGFILE: select reg[%d]\n", current_regnum);
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
