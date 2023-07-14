// Copyright 2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#pragma once

#define ENABLE_BURN_MIPS                0           // Specifies whether to include burn() tasks on remaining cores for testing purposes.


#define MIC_ARRAY_CONFIG_MCLK_FREQ      24576000
#define MIC_ARRAY_CONFIG_PDM_FREQ       3072000
#define MIC_ARRAY_CONFIG_USE_DDR        1
#define MIC_ARRAY_CONFIG_PORT_MCLK      XS1_PORT_1D // X0D11, J14 - Pin 15
#define MIC_ARRAY_CONFIG_PORT_PDM_CLK   XS1_PORT_1A // X0D00, J14 - Pin 2
#define MIC_ARRAY_CONFIG_PORT_PDM_DATA  XS1_PORT_8B // X0D14..X0D21 | J14 - Pin 3,5,12,14 and Pin 6,7,10,11
#define MIC_ARRAY_CONFIG_MIC_COUNT      16
#define MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME 32
#define MIC_ARRAY_TILE                  0           // NOTE: Tile 1 might still have issues with channels other than the first.
#define NUM_DECIMATOR_SUBTASKS          2           // Indicates the number of subtasks to perform the decimation process on.


#define MIC_ARRAY_CLK1                  XS1_CLKBLK_1
#define MIC_ARRAY_CLK2                  XS1_CLKBLK_2
                


// Configuration checks
#if MIC_ARRAY_CONFIG_MIC_COUNT > 8 && NUM_DECIMATOR_SUBTASKS < 2
#error "NUM_DECIMATOR_SUBTASKS: Unsupported value"
#endif

#if !(MIC_ARRAY_CONFIG_MIC_COUNT == 8 || MIC_ARRAY_CONFIG_MIC_COUNT == 16)
#error "MIC_ARRAY_CONFIG_MIC_COUNT: Unsupported value"
#endif

#if NUM_DECIMATOR_SUBTASKS > MIC_ARRAY_CONFIG_MIC_COUNT
#error "NUM_DECIMATOR_SUBTASKS must be less than or equal to MIC_ARRAY_CONFIG_MIC_COUNT"
#endif

#if MIC_ARRAY_CONFIG_USE_DDR != 1
#error "MIC_ARRAY_CONFIG_USE_DDR: This application only supports DDR"
#endif
