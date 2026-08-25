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
  // Coprocessor MAC Interface (dummy MAC for this v0.1 phase)
  input   logic                   mac_done_i,       // Flag from dummy MAC indicating completion
  output  logic [DATA_WIDTH-1:0]  mac_rs1_o,
  output  logic [DATA_WIDTH-1:0]  mac_rs2_o,
  output  logic                   mac_valid_o
);

  logic fifo_full;
  logic fifo_empty;

  // --- Handshake Logic
  // Coprocessor ready to accept new instruction as long as FIFO isn't full
  assign issue_ready_o = ~fifo_full;

  // --- MAC Control
  //  MAC has valid data to compute if FIFO not empty
  assign mac_valid_o = ~fifo_empty;


  // --- FIFO Instantiation
  sync_fifo #(
    .DEPTH(8),
    .DATA_WIDTH(DATA_WIDTH)
  ) i_instruction_fifo (
    .clk_i    (clk_i),
    .rst_ni   (rst_ni),
    .r1_i     (issue_rs1_i),
    .r2_i     (issue_rs2_i),
    .push_i   (issue_valid_i & issue_ready_o),
    .pop_i    (mac_done_i),
    .r1_o     (mac_rs1_o),
    .r2_o     (mac_rs2_o),
    .full_o   (fifo_full),
    .empty_o  (fifo_empty)
  );

endmodule
