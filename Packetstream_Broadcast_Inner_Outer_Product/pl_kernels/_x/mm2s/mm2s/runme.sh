#!/bin/sh

# 
# v++(TM)
# runme.sh: a v++-generated Runs Script for UNIX
# Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
# 

if [ -z "$PATH" ]; then
  PATH=/home/Xilinx/Vitis_2022_2/Vitis_HLS/2022.2/bin:/home/Xilinx/Vitis_2022_2/Vitis/2022.2/bin:/home/Xilinx/Vitis_2022_2/Vitis/2022.2/bin
else
  PATH=/home/Xilinx/Vitis_2022_2/Vitis_HLS/2022.2/bin:/home/Xilinx/Vitis_2022_2/Vitis/2022.2/bin:/home/Xilinx/Vitis_2022_2/Vitis/2022.2/bin:$PATH
fi
export PATH

if [ -z "$LD_LIBRARY_PATH" ]; then
  LD_LIBRARY_PATH=
else
  LD_LIBRARY_PATH=:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

HD_PWD='/home/sai.divya/pkt_clone/AI_Engine_Development/Feature_Tutorials/04-packet-switching/pktstream_aie/pl_kernels/_x/mm2s/mm2s'
cd "$HD_PWD"

HD_LOG=runme.log
/bin/touch $HD_LOG

ISEStep="./ISEWrap.sh"
EAStep()
{
     $ISEStep $HD_LOG "$@" >> $HD_LOG 2>&1
     if [ $? -ne 0 ]
     then
         exit
     fi
}

EAStep vitis_hls -f mm2s.tcl -messageDb vitis_hls.pb
