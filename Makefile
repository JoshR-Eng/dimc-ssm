# Author: Josh Rawlinson, University of Warwick
# Date: 17.08.26
# Description: Makefile for linting and testing SSM Coprocessor

# --- Toolchain ---
VERILATOR = verilator
LINT_FLAGS = --lint-only -Wall
SIM_FLAGS = -cc --exe --build --trace --Wall

# --- RTL Sources ---
RTL_SRC = hw/rtl/cvxif/cvxif_issue_stage.sv
TB_SRC = hw/tb/cpp/issue_driver.cpp

# --- Commands ---
.PHONY: all lint sim clean

all: lint		# Default target
lint:			# Linting target
	$(VERILATOR) $(LINT_FLAGS) $(RTL_SRC)
clean:			# clean up
	rm -rf obj_dir
sim:
	$(VERILATOR) $(SIM_FLAGS) $(RTL_SRC) $(TB_SRC)
	./obj_dir/Vcvxif_issue_stage
