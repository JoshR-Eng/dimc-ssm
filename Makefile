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
VERILATOR  := verilator
LINT_FLAGS := --lint-only -Wall
SIM_FLAGS  := -Wall --trace-fst --cc --exe --build -j 0 --top-module $(TOP_MODULE)
V_EXE	   := obj_dir/V$(TOP_MODULE)
WAVE_FILE  := waveform.fst


# --- Commands ---
.PHONY: all lint sim wave clean

all: lint		# Default target

lint:			# Linting target
	@echo "[INFO] Running Verilator Linter..."
	$(VERILATOR) $(LINT_FLAGS) $(SV_SOURCES) --top-module $(TOP_MODULE)
	@echo "[PASS] RTL is clean"

sim: lint		# Compile and simulate (Auto run lint first)
	@echo "[INFO] Compiling C++ Testbench and SystemVerilog RTL"
	$(VERILATOR) $(SIM_FLAGS) $(SV_SOURCES) $(CPP_TB)
	@echo "[INFO] Executing Simulation..."
	./$(V_EXE) +trace

wave:			# Open Waveform viewer
	@echo "[INFO] Opening Fast Signal Trace in GTKWave..."
	gtkwave $(WAVE_FILE) &

clean: 			# Clean up generated objects
	@echo "[INFO] Cleaning build directories and waveforms..."
	rm -rf obj_dir
	rm -f $(WAVE_FILE)
