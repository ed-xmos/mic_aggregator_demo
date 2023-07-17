# Copyright 2023 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

# Create the VCD file using:  xrun --xscope-file trace.vcd app_mic_aggregator/mic_aggregator.xe 
# Then run: python vcd2wav.py to pick it up and convert to wav

import re
import struct
from bitstring import BitArray
import scipy.io.wavfile
import numpy as np


def get_meta_data(file):
    timescale = None
    varnames = []
    counter = 0
    while True:
        line = file.readline()
        
        if "$timescale" in line:
            time_str = file.readline()
            m = re.match(r'\s*([0-1.]+)\s+([a-z]+)', line)
            if m:
                num = float(m.groups(0)[0])
                unit = m.groups(0)[1]
            file.readline() # consume "#end"

        if "$var" in line:
            m = re.match(r'\s*\$var\s+wire\s+(\d+)\s+(\d+)\s+(\S+)\s+\$end', line)
            if m:
                width = int(m.groups(0)[0])
                idx = int(m.groups(0)[1])
                name = m.groups(0)[2]
                if "Missing_Data" not in name:
                    assert len(varnames) == idx
                varnames.append(name)

        if "$enddefinitions" in line:
            return varnames

        counter += 1
        if counter == 100:
            assert 0, "Failed to extract metadata"

def get_data(file, varnames):
    data = [[] for i in range(len(varnames))] #init empty 2d array
    while True:
        line = file.readline() #get timestamp
        if line == "":
            return data
        m = re.match(r'#(\d+)', line)
        if m:
            time = int(m.groups(0)[0])
        else:
            assert 0, f"#tttttttttt not found in {line}"
        line = file.readline() # get data
        if line == "":
            return data

        m = re.match(r'b([0-1]+)\s+(\d+)', line)
        if m:
            bin = m.groups(0)[0]
            if len(bin) < 32:
                bin = "0" * (32 - len(bin)) + bin
            b = BitArray(bin=bin)
            val = b.int
            idx = int(m.groups(0)[1])
        else:
            assert 0, f"bNNNNNNNNNNNNNNNNNN M not found in {line}"
        data[idx].append(val)
    

def write_wav(data, varnames, samp_rate):
    num_channels = len(data) - 1 #remove Missing_Data data field
    wav_data = np.array(data[0:num_channels], dtype=np.int32)
    scipy.io.wavfile.write("capture.wav", samp_rate, wav_data.T)


def print_summary(data, varnames):
    num_channels = len(data) - 1 #remove Missing_Data data
    ch = 0
    for channame in varnames:
        samps = len(data[ch])
        print(f"Channel: {ch} {channame} samples: {samps}")
        ch += 1


def vcd2array(file_name, samp_rate):
    with open(file_name) as vcd:
        varnames = get_meta_data(vcd)
        data = get_data(vcd, varnames)
        print_summary(data, varnames)
        write_wav(data, varnames, samp_rate)

vcd2array("trace.vcd", 16000)