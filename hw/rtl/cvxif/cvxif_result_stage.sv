// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-08-25
// Design Name:     CV-X-IF Results Stage
// Description:
//   CV-X-IF Results Stage Handshake.
//   Manages the asynchronous valid/ready boundary between the host core
//   and the internal TCDM streaming queue. Ensures zero combinational
/////////////////////////////////////////////////////////////////////////////////////

module cvxif_result_stage #(
  parameter int unsigned DATA_WIDTH = 32
) (
  input logic clk_i,
  input logic rst_ni,
  // Host Inteface
  output logic [(DATA_WIDTH * 2)-1:0] results_data_o,
  output logic                        results_valid_o, // Coprocessor ready to offload data
  input  logic                        results_ready_i, // Host ready for data
  // MAC
  input  logic                        mac_valid_i,
  input  logic [(DATA_WIDTH * 2)-1:0] mac_results_i,
  output logic                        mac_ready_o
);

// Skid Buffer Registers
logic                        valid_d, valid_q;
logic [(DATA_WIDTH * 2)-1:0] data_d, data_q;

// --- Handshake & Backpressure
// Accept new data from the MAC if buffer is currently empty
// OR if the host is draining the buffer on this exact clock cycle.
assign mac_ready_o = ~valid_q | result_ready_i;


// --- Combinational Next-State Logic ---
always_comb begin : skid_logic
  // Default: Hold current state
  valid_d = valid_q;
  data_d  = data_q;

  // 1. Host drains the buffer
  if (valid_q && result_ready_i) begin
    valid_d = 1'b0;
  end
  // 2. MAC writes to the buffer
  // Note: 2. can safely overrides 1., allowing continous streaming
  if (mac_valid_i && mac_ready_o) begin
    valid_d = 1'b1;
    data_d  = mac_result_i;
  end
end

// --- Sequential Logic
always_ff @(posedge clk_i or negedge rst_ni) begin : skid_registers
  if (!rst_ni) begin
    valid_q <= 1'b0;
  end else begin
    valid_q <= valid_d;
    data_q  <= data_d;
  end
end

// --- Output assignment
assign result_valid_o = valid_q;
assign result_data_o  = data_q;

endmodule
