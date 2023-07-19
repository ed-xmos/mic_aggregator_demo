// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#pragma once

#define ENABLE_BURN_MIPS                    0           // Specifies whether to include burn() tasks on remaining cores for testing purposes.


#define MIC_ARRAY_CONFIG_MCLK_FREQ          24576000
#define MIC_ARRAY_CONFIG_PDM_FREQ           3072000
#define MIC_ARRAY_CONFIG_USE_DDR            1
#define MIC_ARRAY_CONFIG_PORT_MCLK          XS1_PORT_1D // X0D11, J14 - Pin 15, '11'
#define MIC_ARRAY_CONFIG_PORT_PDM_CLK       XS1_PORT_1A // X0D00, J14 - Pin 2, '00'
#define MIC_ARRAY_CONFIG_PORT_PDM_DATA      XS1_PORT_8B // X0D14..X0D21 | J14 - Pin 3,5,12,14 and Pin 6,7,10,11
#define MIC_ARRAY_CONFIG_MIC_COUNT          16
#define MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME  1
#define MIC_ARRAY_TILE                      0           // NOTE: Tile 1 might still have issues with channels other than the first.
#define MIC_ARRAY_NUM_DECIMATOR_TASKS       3           // Defines the number of subtasks to perform the decimation process on.
#define MIC_ARRAY_PDM_RX_OWN_THREAD         1           // Use dedicated thread for PDM Rx task
#define MIC_ARRAY_CLK1                      XS1_CLKBLK_1
#define MIC_ARRAY_CLK2                      XS1_CLKBLK_2

#define TDM_SLAVEPORT_OUT                   XS1_PORT_1A // X1D00, I2S DAC OUT
#define TDM_SLAVEPORT_FSYNCH                XS1_PORT_1B // X1D01, I2S LRCLK
#define TDM_SLAVEPORT_BCLK                  XS1_PORT_1C // X1D10, I2S BCLK
#define TDM_SLAVEPORT_CLK_BLK               XS1_CLKBLK_1
#define TDM_SLAVETX_OFFSET                  1

#define TDM_SIMPLE_MASTER_FSYNCH            XS1_PORT_1M // X1D36, J10 - pin 2, '36'
#define TDM_SIMPLE_MASTER_DATA              XS1_PORT_1O // X1D38, J10 - pin 15, '38'
#define TDM_SIMPLE_MASTER_CLK_BLK           XS1_CLKBLK_2

#define I2C_CONTROL_SLAVE_ADDRESS           0x3c    
#define I2C_CONTROL_NUM_REGISTERS           (16 * (4 + 1))  // Number of 8b registers
#define I2C_CONTROL_SLAVE_SCL               XS1_PORT_1N     //X0D37, SCL
#define I2C_CONTROL_SLAVE_SDA               XS1_PORT_1O     //X0D38, SDA


// Configuration checks
#if MIC_ARRAY_CONFIG_MIC_COUNT > 8 && MIC_ARRAY_NUM_DECIMATOR_TASKS < 2
#error "MIC_ARRAY_NUM_DECIMATOR_TASKS: Unsupported value"
#endif

#if !(MIC_ARRAY_CONFIG_MIC_COUNT == 8 || MIC_ARRAY_CONFIG_MIC_COUNT == 16)
#error "MIC_ARRAY_CONFIG_MIC_COUNT: Unsupported value"
#endif

#if MIC_ARRAY_NUM_DECIMATOR_TASKS > MIC_ARRAY_CONFIG_MIC_COUNT
#error "MIC_ARRAY_NUM_DECIMATOR_TASKS must be less than or equal to MIC_ARRAY_CONFIG_MIC_COUNT"
#endif

#if MIC_ARRAY_CONFIG_USE_DDR != 1
#error "MIC_ARRAY_CONFIG_USE_DDR: This application only supports DDR"
#endif
