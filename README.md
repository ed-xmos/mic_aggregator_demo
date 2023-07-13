# mic_aggregator_demo
16 PDM mics to TDM 16 demo running on the explorer board

****************
Building the app
****************

First install and source the XTC version: 15.2.1 tools.

To build for the first time:

.. code-block:: console

    $ mkdir build
    $ cd build
    $ cmake --toolchain ../fwk_io/xmos_cmake_toolchain/xs3a.cmake  ..
    $ make mic_aggregator -j

Following inital cmake build, you may just type:

. code-block:: console
    $ make mic_aggregator -j

**************
Running the app
**************

. code-block:: console

    $ xrun app_mic_aggregator/mic_aggregator.xe 