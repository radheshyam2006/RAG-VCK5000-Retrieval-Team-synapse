# Simple combined Makefile for AIE + PL hw_emu (vitis 2022.2, VCK5000)
PLATFORM ?= /opt/xilinx/platforms/xilinx_vck5000_gen4x8_qdma_2_202220_1/xilinx_vck5000_gen4x8_qdma_2_202220_1.xpfm
AIE_BUILD := build.aie
WORK_DIR := Work
PL_DIR := pl_kernels
PL_TARGET := hw_emu
PL_BUILD := build.pl.$(PL_TARGET)

AIE_GRAPH := aie/graph.cpp
AIE_INCLUDES := -I$(XILINX_VITIS)/aietools/include -I.

.PHONY: all aie aiesim pl_hw_emu run_aiesim run_pl_hw_emu clean

all: aie

aie: $(AIE_BUILD)/libadf.a

$(AIE_BUILD)/libadf.a: $(AIE_GRAPH)
	@mkdir -p $(AIE_BUILD)
	cd $(AIE_BUILD) && \
	aiecompiler -v --target=hw \
	  -include="$(XILINX_VITIS)/aietools/include" \
	  -include=".." \
	  ../$(AIE_GRAPH) \
	  -workdir=../$(WORK_DIR) 2>&1 | tee aiecompiler.log

run_aiesim: aie
	cd $(AIE_BUILD) && aiesimulator --pkg-dir=$(WORK_DIR) --i=.. --profile --dump-vcd=aie.vcd

# PL HLS -> .xo rules (one .xo per .cpp in pl_kernels)
PL_SRCS := $(wildcard $(PL_DIR)/*.cpp)
PL_XOS := $(patsubst $(PL_DIR)/%.cpp,$(PL_BUILD)/%.xo,$(PL_SRCS))

pl_hw_emu: $(PL_BUILD)/xclbin.hw_emu

$(PL_BUILD)/%.xo: $(PL_DIR)/%.cpp
	@mkdir -p $(PL_BUILD)
	v++ -c --platform $(PLATFORM) -k $(basename $(notdir $<)) --kernel_frequency 250 $< -o $@ --target=$(PL_TARGET) --save-temps

$(PL_BUILD)/xclbin.hw_emu: $(PL_XOS)
	@mkdir -p $(PL_BUILD)
	v++ -l --platform $(PLATFORM) -o $@ $(PL_XOS) --target=$(PL_TARGET) --temp_dir $(PL_BUILD)/temp

run_pl_hw_emu: pl_hw_emu
	@echo "Run: emconfigutil --platform $(PLATFORM) && export XCL_EMULATION_MODE=hw_emu && ./launch_hw_emu.sh $(PL_BUILD)/xclbin.hw_emu sw/host.exe"

clean:
	rm -rf $(AIE_BUILD) $(PL_BUILD) Work aiecompiler.log
	$(MAKE) -C $(PL_DIR) clean || true
