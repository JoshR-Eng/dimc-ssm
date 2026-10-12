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

// --- local Variables
//    Width of the rounded value: IN_W - SHIFT integer bits, plus 1
//    so round-up increment cannot overflow
localparam int unsigned RND_W = IN_W - SHIFT + 1;


// --- Error handle potential misconfiguration
if (SHIFT < 1) begin : g_bad_shift
  $error("[ERROR: rnd_sat - SHIFT must be >= 1");
end
if (OUT_W > IN_W + 1 - SHIFT) begin : g_bad_out_w
  $error("[ERROR]: rnd_sat - OUT_W too wide for IN_W/SHIFT")



// --- Combinational Logic

logic [RND_W-1:0] truncated;
logic [RND_W-1:0] rounded;
logic             in_range;

  // 1. Drop the fraction bits
assign truncated = {in_i[IN_W-1], in_i[IN_W-1:SHIFT]};

  // 2. Round half up
assign rounded = truncated + RND_W'(in_i[SHIFT-1]);

  // 3. Saturation detection
assign in_range = (&rounded[RND_W-1:OUT_W-1]) | (~|rounded[RND_W-1:OUT-1]);

  // 4. Output
assign out_o = in_range ? rounded[OUT_W-1:0]                                  // fits: low OUT_W bits
                        : {rounded[RND_w-1], {(OUT_W-1){~rounded[RND_W-1]}}}; // rail from the TRUE MSB
