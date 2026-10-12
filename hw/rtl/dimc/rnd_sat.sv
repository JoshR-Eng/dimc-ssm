// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-09-25
// Design Name:     Bit-Serial Multiplier
// Description:
//
//    Algorithm:
//
//    Interface
//
/////////////////////////////////////////////////////////////////////////////////////

module rnd_sat #(
  parameter int unsigned IN_W   = dimc_pkg::LANE_ACC_W,
  parameter int unsigned SHIFT  = dimc_pkg::FRAC,
  parameter int unsigned OUT_W  = dimc_pkg::DATA_W,
) (
  input  logic [IN_W-1:0]   in_i, // two's complement, wide accumulator
  output logic [OUT_W-1:0]  out_o // round-half-up then saturate
);

// --- Error handle potential misconfiguration
if (SHIFT < 1) begin : g_bad_shift
  $error("[ERROR: rnd_sat - SHIFT must be >= 1");
end
if (OUT_W > IN_W + 1 - SHIFT) begin : g_bad_out_w
  $error("[ERROR]: rnd_sat - OUT_W too wide for IN_W/SHIFT")

  // 1. Biased sum: sign-extend the input, add 2^(SHIFT-1).
  // 2. Rounded value: discard the low SHIFT bits.             Bit-select, or >>> on a SIGNED operand
  // 3. In range? Which bits must all equal each other?        (Friday's question d)
  // 4. Output: rounded low OUT_W bits if in range, else the rail chosen by the TRUE MSB
