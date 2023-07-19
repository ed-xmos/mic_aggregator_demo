// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <xcore/port.h>
#include <xscope.h>
#include <xclib.h>
#include <stdio.h>

#include "app_config.h"


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

    int32_t rx_data[16] = {0};

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