# Author: Josh Rawlinson, University of Warwick
# Date: 17.08.26
# Description: Makefile for linting and testing SSM Coprocessor

# Hide Verilator Info by default - `make <target> V=1` to see all commands for debugging
ifndef V
MAKEFLAGS += --silent
endif

# --- RTL Sources ---
# Input Parameters
GPARAMS ?= DATA_WIDTH=$(WIDTH)
G_FLAGS ?= $(addprefix -G,$(GPARAMS)) $(addprefix -CFLAGS -DTB_,$(GPARAMS))

# Top Level Test
TOP_MODULE 	:= top_mac_cvxif
RTL_DIR	   	:= hw/rtl
TB_DIR     	:= hw/tb/cpp
PKG_SOURCES := hw/pkg/dimc_pkg.sv
SV_SOURCES 	:= $(PKG_SOURCES) $(shell find $(RTL_DIR) -name '*.sv')
CPP_TB	   	:= $(TB_DIR)/tb_mac.cpp

# Unit Level Test
UNIT			?= bs_mult
WIDTH			?= 16
TRACE			?= 1
UNIT_DIR	 	:= obj_dir/$(UNIT)_w$(WIDTH)
UNIT_LINT_FLAGS = $(LINT_FLAGS) --quiet --top-module $(UNIT)	\
				  $(addprefix -G,$(GPARAMS)) $(SV_SOURCES)
UNIT_SIM_FLAGS 	= -Wall --trace-fst --quiet --cc --exe --build -j 0 --top-module $(UNIT) 		\
					$(G_FLAGS) -CFLAGS -std=c++20 -CFLAGS -DTB_WIDTH=$(WIDTH) --Mdir $(UNIT_DIR) \
					$(SV_SOURCES) $(TB_DIR)/tb_$(UNIT).cpp

# --- Toolchain ---
VERILATOR   := verilator
LINT_FLAGS  := --lint-only -Wall
SIM_FLAGS   := -Wall --trace-fst --cc --exe --build -j 0 --top-module $(TOP_MODULE)
V_EXE	    := obj_dir/V$(TOP_MODULE)
WAVE_FILE   := waveform.fst
REPORT_FILE := reports/mamba_cvxif_area.txt


# --- Commands ---
.PHONY: all lint model sim unit ref_check sim_unit wave synth clean

all: lint		# Default target

lint:			# Linting target
	@echo "[INFO] Running Verilator Linter..."
	$(VERILATOR) $(LINT_FLAGS) $(SV_SOURCES) --top-module $(TOP_MODULE)
	@echo "[PASS] RTL is clean"

model:			# Runs the Golden Model test cases
	@echo "[INFO] Running Python model of SSM"
	python3 -m pytest model/ssm_model/s4_tests.py -v

sim: lint		# Compile and simulate (Auto run lint first)
	@echo "[INFO] Compiling C++ Testbench and SystemVerilog RTL"
	$(VERILATOR) $(SIM_FLAGS) $(SV_SOURCES) $(CPP_TB)
	@echo "[INFO] Executing Simulation..."
	./$(V_EXE) +trace
unit:			# Unit level tests to validate module functions
	@mkdir -p $(UNIT_DIR)
	$(VERILATOR) $(UNIT_LINT_FLAGS)
	$(VERILATOR) $(UNIT_SIM_FLAGS)
	./$(UNIT_DIR)/V$(UNIT) $(if $(filter 1,$(TRACE)),+trace,)
ref_check:		# Validate the C++ reference model against the Python golden model
	@mkdir -p obj_dir
	g++ -std=c++20 -O2 -Wall -Wextra -Werror -o obj_dir/ref_check $(TB_DIR)/tb_ref_check.cpp
	./obj_dir/ref_check
sim_unit: 		# Test a number of units across various Datawidth
	@echo "[INFO] Running Simulation of individual Units..."
	$(MAKE) ref_check
	$(MAKE) unit UNIT=bs_mult WIDTH=4  TRACE=0
	$(MAKE) unit UNIT=bs_mult WIDTH=8  TRACE=0
	$(MAKE) unit UNIT=bs_mult WIDTH=16 TRACE=0
	$(MAKE) unit UNIT=rnd_sat GPARAMS="IN_W=12 SHIFT=4 OUT_W=6" WIDTH=small TRACE=0
	$(MAKE) unit UNIT=rnd_sat GPARAMS="IN_W=33" WIDTH=lane  TRACE=0
	$(MAKE) unit UNIT=rnd_sat GPARAMS="IN_W=36" WIDTH=array TRACE=0
	@echo "[INFO] Unit-level Simulation completed."
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
