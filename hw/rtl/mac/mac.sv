// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-08-25
// Design Name:     Multiple-Accumulate  (MAC)
// Description:
//   Basic Multiple-Accumulate (MAC) module
//   This is a placeholder for future development
//   later development will replace this dummy MAC with an
//   Analog In-Memory Compute crossbar
/////////////////////////////////////////////////////////////////////////////////////

module mac #(
  parameter int unsigned DATA_WIDTH = 32
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Control Interface
  input  logic valid_i,       // Driven by CV-X-IF interface
  input  logic clear_accum_i, // Set HIGH to reste the sum to 0
  input  logic ready_i,       // Tells MAC if Host is ready for results
  output logic ready_o,       // Tells FIFO if it wants to keep adding data
  output logic valid_o,       // HIGH when accumulator has valid output
  // Data Interface
  input  logic [DATA_WIDTH-1:0]       rs1_i,
  input  logic [DATA_WIDTH-1:0]       rs2_i,
  output logic [(DATA_WIDTH * 2)-1:0] result_o
);

assign ready_o = ready_i

// --- 1. MULTIPLIER
logic [(DATA_WIDTH * 2)-1:0] mult_product_d, mult_product_q;
logic                        mult_valid_q;

assign mult_product_d = rs1_i * rs2_i;

always_ff @(posedge clk_i or negedge rst_ni) begin : read_memory
  if (!rst_ni) begin
    mult_product_q <= '0;
    mult_valid_q   <= 1'b0;
  end else if (ready_i) begin
    mult_product_q <= mult_product_d;
    mult_valid_q   <= valid_i;
  end
end


// --- 2. ACCUMULATE
logic [(DATA_WIDTH * 2)-1:0] accum_sum_d, accum_sum_q;
logic                        accum_valid_q;
logic clear_accum_d, clear_accum_q;

assign clear_accum_d = clear_accum_i;

always_comb begin : accumulate_update
  // Default: Hold current State
  accum_sum_d = accum_sum_q;

  // Update if multipler holds valid data
  if (mult_valid_q) begin
    if (clear_accum_q) begin
      accum_sum_d = mult_product_q;
    end else begin
      accum_sum_d = accum_sum_q + mult_valid_q;
    end
  end
end

always_ff @( posedge clk_i or negedge rst_ni ) begin : accumulate
  if (!rst_ni) begin
    accum_sum_q   <= '0;
    accum_valid_q <= 1'b0;
    clear_accum_q <= '0;
  end else if (ready_i) begin
    accum_sum_q   <= accum_sum_d;
    accum_valid_q <= mult_valid_q;
    clear_accum_q <= clear_accum_d;
  end
end

// --- Output Assignment
assign result_o = accum_sum_q;
assign valid_o  = accum_valid_q;

endmodule
