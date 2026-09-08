// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-09-08
// Design Name:     SRAM Skid-Buffer
// Description:
//    A 2-Stage Shift Register meant to append to SRAM memory block
//    enabling First-Word Fall-Through that masks the SRAM read latency
//    while safely absorbing the 'skid' if the coprocessor stalls
/////////////////////////////////////////////////////////////////////////////////////

module sram_skid_buffer #(
  parameter int unsigned DATA_WIDTH = 32
) (
  input logic clk_i,
  input logic rst_ni,
  // SRAM Interface
  input  logic                  sram_empty_i,
  output logic                  sram_rd_en_o,
  input  logic [DATA_WIDTH-1:0] sram_data_i,
  // FWFT Output Interface
  input  logic                  pop_i,
  output logic [DATA_WIDTH-1:0] data_o,
  output logic                  empty_o
);

// --- Track SRAM Reads
logic sram_data_valid_q;

always_ff @(posedge clk_i or negedge rst_ni) begin
  if (!rst_ni) begin
    sram_data_valid_q <= 1'b0;
  end else begin
    sram_data_valid_q <= sram_rd_en_o;
  end
end


// --- 2-Stage Skid Registers
logic [1:0]            valid_d, valid_q;
logic [DATA_WIDTH-1:0] data_d [0:1], data_q [0:1];

assign empty_o = ~valid_q[0];
assign data_o  = data_q[0];


// --- Prefetch logic
//      Can only issue an SRAM read if physical space is gaurenteed
logic [1:0] occupied_slots;
assign occupied_slots = {1'b0, valid_q[0]} + {1'b0, valid_q[1]} + {1'b0, sram_data_valid_q};
//      Issue read if SRAM has data & less than 2 slots accounted for
assign sram_rd_en_o = ~sram_empty_i && (occupied_slots < 2'd2);



// --- Shift Register State Machine
always_comb begin
  valid_d = valid_q;
  data_d  = data_q;

  // Handle Coprocessor Pop (Shift Down)
  if (pop_i && valid_q[0]) begin
    valid_d[0] = valid_q[1];
    data_d[0]  = data_q[1];
    valid_d[1] = 1'b0;
  end

  // Handle SRAM Push
  if (sram_data_valid_q) begin
    if (valid_d[0] == 1'b0) begin
      valid_d[0] = 1'b1;
      data_d[0]  = sram_data_i;
    end else if (valid_d[1] == 1'b0) begin
      valid_d[1] = 1'b1;
      data_d[1]  = sram_data_i;
    end
  end
end


// --- Sequential Update
always_ff @(posedge clk_i or negedge rst_ni) begin
  if (!rst_ni) begin
    valid_q   <= '0;
    data_q[0] <= '0;
    data_q[1] <= '0;
  end else begin
    valid_q <= valid_d;
    data_q  <= data_d;
  end
end

endmodule
