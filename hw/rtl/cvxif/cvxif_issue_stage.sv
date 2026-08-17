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


module cvxif_issue_stage (
  input   logic clk_i,            // System Clock
  input   logic rst_ni,           // Active-low asynchronous reset
  input   logic issue_valid_i,    // Host offload request valid
  input   logic [31:0] issue_rs1_i
  input   logic [31:0] issue_rs2_i
  output  logic issue_ready_o,    // Coprocessor accept ready
  output  logic issue_accept_o    // Internal successful transaction flag
  );

  assign issue_ready_o  = 1'b1;
  assign issue_accept_o = issue_valid_i & issue_ready_o;

endmodule
