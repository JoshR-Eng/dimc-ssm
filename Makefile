# Author: Josh Rawlinson, University of Warwick
# Date: 17.08.26
# Description: Makefile for linting and testing SSM Coprocessor

# --- Toolchain ---
VERILATOR = verilator
LINT_FLAGS = --lint-only -Wall

# --- RTL Sources ---
RTL_SRC = hw/rtl/cvxif/cvxif_issue_stage.sv

# --- Commands ---
.PHONY: all lint clean

all: lint		# Default target
lint:			# Linting target
	$(VERILATOR) $(LINT_FLAGS) $(RTL_SRC)
clean:			# clean up
	rm -rf obj_dir
