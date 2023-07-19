# mic_aggregator_demo
16 PDM mics to TDM 16 demo running on the explorer board

Obtaining the app files
-----------------------

Download the main repo and submodules using:

    $ git clone --recurse git@github.com:ed-xmos/mic_aggregator_demo.git
    $ cd mic_aggregator_demo/


Building the app
----------------

First install and source the XTC version: 15.2.1 tools. You should be able to see:

    $ xcc --version
    xcc: Build 19-198606c, Oct-25-2022
    XTC version: 15.2.1
    Copyright (C) XMOS Limited 2008-2021. All Rights Reserved.

To build for the first time you will need to run cmake to create the make files:

    $ mkdir build
    $ cd build
    $ cmake --toolchain ../fwk_io/xmos_cmake_toolchain/xs3a.cmake  ..
    $ make mic_aggregator -j

Following inital cmake build, as long as you don't add new source files, you may just type:

    $ make mic_aggregator -j

If you add new source files you will need to run the `cmake` step again.

Running the app
---------------

Connect the explorer board to the host and type:

    $ xrun app_mic_aggregator/mic_aggregator.xe 

Required Hardware
-----------------

The demo runs on the XCORE-AI Explorer board version 2 (with integrated XTAG debug adapter). You will require in addition:

- The dual DDR microphone board that attaches via the flat flex connector
- Header pins soldered into
    - J14, J10, SCL/SDA IOT, the I2S expansion header, MIC data and MIC clock
- A handful of jumper wires connected as below

An oscilloscope will also be handy in case of debug needed.

*Note you will only be able to inject PDM data to two channels at a time due to a single pair of mics on the HW*


Jumper Connections
------------------

Make the following connections using flying leads:

- MIC CLK <-> J14 '00'. This is the mic clock which is to be sent to the PDM mics from J14.
- MIC DATA <-> J14 '14' initially. This is the data line for mics 0 and 8. See below..
- I2S LRCLK <-> J10 '36'. This is the FSYCNH input for TDM slave. J10 '36' is the TDM master FSYNCH output for the demo
- I2S MCLK <-> I2S BCLK. MCLK is the 24.576MHz clock which directly drives the BCLK input for the TDM slave
- I2S DAC <-> J10 '38'. I2S DAC is the TDM Slave Tx out which is read by the TDM Master Rx input on J10.

To access other mic inputs use the following:

| Mic pair | J14 pin |
| -------- | ------- |
| 0, 8 | 14 |
| 1, 9 | 15 |
| 2, 10 | 16 |
| 3, 11 | 17 |
| 4, 12 | 18 |
| 5, 13 | 19 |
| 6, 14 | 20 |
| 7, 15 | 21 |


For I2C control, make the following connections:

- SCL IOL <-> Your I2C host SCL
- SDA IOL <-> Your I2C host SDA
- GND <-> Your I2C host ground

There are XX registers which control the gain of each output channel. If using a raspberry Pi as the I2C host you may use the following commands:

    $ i2cset -y 1 0x3c 0 100 #Set the gain on mic channel 0 to 100
    $ i2cset -y 1 0x3c 15 50 #Set the gain on mic channel 15 to 50
    $ i2cget -y 1 0x3c 0 #Get the gain on mic channel 0





