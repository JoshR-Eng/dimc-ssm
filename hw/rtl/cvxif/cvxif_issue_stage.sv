// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-08-17
// Design Name:     CV-X-IF Issue Stage
// Description:
//   CV-X-IF Issue Stage Handshake.
//   Manages the asynchronous valid/ready boundary between the host core
//   and the internal TCDM streaming queue. Ensures zero combinational
//   feedback loops on the issue_ready_o path.
/////////////////////////////////////////////////////////////////////////////////////


module cvxif_issue_stage #(
  parameter int DATA_WIDTH = 32           // Operand bus width
) (
  input   logic         clk_i,            // System Clock
  input   logic         rst_ni,           // Active-low asynchronous reset

  // Host Interface
  input   logic                   issue_valid_i,    // Host offlead request valid
  output  logic                   issue_ready_o,    // Coprocessor accept ready
  input   logic [DATA_WIDTH-1:0]  issue_rs1_i,      // Instruction Operand 1
  input   logic [DATA_WIDTH-1:0]  issue_rs2_i,      // Instruction Operand 2

  // Internal Coprocessor Flags (dummy MAC for this v0.1 phase)
  input   logic         mac_done_i        // Flag from dummy MAC indicating completion
);

  logic issue_accept_s;
  logic busy_q;
  logic busy_d;

  logic [31:0] latched_rs1_q;
  logic [31:0] latched_rs2_q;

  // --- Handshake Logic
  // Coprocessor ready to accept new instruction if not currently busy
  assign issue_ready_o  = ~busy_q;
  assign issue_accept_s = issue_valid_i & issue_ready_o;

  // --- Next-State Logic for the Depth-1 FIFO
  always_comb begin : busy_flag_logic
    busy_d = busy_q; // Default assignment

    if (issue_accept_s) begin
      // A new instuction is accepted - Coprocessor now busy.
      busy_d = 1'b1;
    end else if (mac_done_i) begin
      // The MAC finished and no new instruction is waiting - no longer busy
      busy_d = 1'b0;
    end
  end


  // --- Sequential Logic (to Capture operands)
  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      busy_q        <= 1'b0;
      latched_rs1_q <= 'b0;
      latched_rs2_q <= 'b0;
    end else begin
      // Update the busy state every clock cycle
      busy_q <= busy_d;
      // Capture incoming operands into internal registers
      if (issue_accept_s) begin
        latched_rs1_q <= issue_rs1_i;
        latched_rs2_q <= issue_rs2_i;
      end
    end
  end

endmodule
