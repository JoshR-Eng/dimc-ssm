// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-09-30
// Design Name:     Digital In-Memory Compute (DIMC) Package
// Description:
//    Contains package description, enum and structs used within
//    the DIMC.
//    Current v0.2 Coprocessor is a simple DIMC but will evolve to
//    a complete State-Space Model Accelerator
/////////////////////////////////////////////////////////////////////////////////////

package dimc_kpg;
  // Arithmetic Contract:
  localparam int unsigned DATA_W  = 16; // Q3.13
  localparam int unsigned FRAC    = 13;
  localparam int unsigned N_LANES = 8;

  // Accumulator Widths
  localparam int unsigned LANE_ACC_W  = ;
  localparam int unsigned ARRAY_ACC_W = ;
endpackage
