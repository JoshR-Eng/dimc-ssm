# Author: Josh Rawlinson, University of Warwick
# Date: 17.08.26
# Description: Makefile for linting and testing SSM Coprocessor

# --- RTL Sources ---
TOP_MODULE := top_mac_cvxif
RTL_DIR	   := hw/rtl
TB_DIR     := hw/tb/cpp

SV_SOURCES := $(shell find $(RTL_DIR) -name '*.sv')
CPP_TB	   := $(TB_DIR)/tb_mac.cpp


# --- Toolchain ---
VERILATOR   := verilator
LINT_FLAGS  := --lint-only -Wall
SIM_FLAGS   := -Wall --trace-fst --cc --exe --build -j 0 --top-module $(TOP_MODULE)
V_EXE	    := obj_dir/V$(TOP_MODULE)
WAVE_FILE   := waveform.fst
REPORT_FILE := reports/mamba_cvxif_area.txt


# --- Commands ---
.PHONY: all lint model sim wave synth clean

all: lint		# Default target

lint:			# Linting target
	@echo "[INFO] Running Verilator Linter..."
	$(VERILATOR) $(LINT_FLAGS) $(SV_SOURCES) --top-module $(TOP_MODULE)
	@echo "[PASS] RTL is clean"

model:
	@echo "[INFO] Running Python model of SSM"
	python3 -m pytest model/ssm_model/s4_tests.py -v

sim: lint		# Compile and simulate (Auto run lint first)
	@echo "[INFO] Compiling C++ Testbench and SystemVerilog RTL"
	$(VERILATOR) $(SIM_FLAGS) $(SV_SOURCES) $(CPP_TB)
	@echo "[INFO] Executing Simulation..."
	./$(V_EXE) +trace

wave:			# Open Waveform viewer
	@echo "[INFO] Opening Fast Signal Trace in Surfer..."
	surfer $(WAVE_FILE) &

synth: 			# Use Yosys to map the SystemVerilog to generic gates
	@echo "[INFO] Running Yosys Synthesis for Area Estimation"
	@mkdir -p ./reports
	yosys -p "read_verilog -sv $(SV_SOURCES); synth -top $(TOP_MODULE); stat" | tee $(REPORT_FILE)
	@echo "[INFO] Synthesis Complete. Report saved to $(REPORT_FILE)"

clean: 			# Clean up generated objects
	@echo "[INFO] Cleaning build directories and waveforms..."
	rm -rf obj_dir
	rm -f $(WAVE_FILE)
