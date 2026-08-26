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

module top_mac_cvxif #(
  parameter int unsigned DATA_WIDTH = 32
) (
  input logic clk_i,
  input logic rst_ni,
  // Host Issue Interface (From CV32E40X)
  input  logic                  issue_valid_i,
  output logic                  issue_ready_o,
  input  logic [DATA_WIDTH-1:0] issue_rs1_i,
  input  logic [DATA_WIDTH-1:0] issue_rs2_i,
  // Host Result Interface (From Coprocessor)
  output logic                  results_valid_o,
  input  logic                  results_ready_i,
  output logic [DATA_WIDTH-1:0] results_data_o,
  // *TEMPORARY* Control Flags for Dummy MAC
  input logic issue_clear_i
);

///////////////////////////////
// Internal Interconnections //
///////////////////////////////

// Issue <-> MAC wires
logic                  issue_mac_valid;
logic                  issue_mac_ready;
logic [DATA_WIDTH-1:0] issue_mac_rs1;
logic [DATA_WIDTH-1:0] issue_mac_rs2;

// MAC <-> Results wires
logic                  mac_res_valid;
logic                  mac_res_ready;
logic [DATA_WIDTH-1:0] mac_res_data;


/////////////////////////
// Module Instantation //
/////////////////////////

// --- CV-X-IF Issue Stage (w/TCDM FIFO)
cvxif_issue_stage #(
  .DATA_WIDTH(DATA_WIDTH)
) i_issue_stage (
  .clk_i         (clk_i),
  .rst_ni        (rst_ni),
  // Host pins
  .issue_valid_i (issue_valid_i),
  .issue_ready_o (issue_ready_o),
  .issue_rs1_i   (issue_rs1_i),
  .issue_rs2_i   (issue_rs2_i),
  // Internal MAC pins
  .mac_valid_o   (issue_mac_valid),
  .mac_ready_i   (issue_mac_ready),
  .mac_rs1_o     (issue_mac_rs1),
  .mac_rs2_o     (issue_mac_rs2)
);

// --- Dummy MAC
mac #(
.DATA_WIDTH(DATA_WIDTH)
) i_mac_engine (
  .clk_i         (clk_i),
  .rst_ni        (rst_ni),
  // Control Handshake
  .valid_i       (issue_mac_valid),
  .ready_o       (issue_mac_ready),
  .clear_accum_i (issue_clear_i),
  .ready_i       (mac_res_ready),
  .valid_o       (mac_res_valid),
  // Data Payload
  .rs1_i         (issue_mac_rs1),
  .rs2_i         (issue_mac_rs2),
  .result_o      (mac_res_data)
);

// --- Result Stage (w/skid buffer)
cvxif_result_stage #(
.DATA_WIDTH(DATA_WIDTH)
) i_result_stage (
  .clk_i          (clk_i),
  .rst_ni         (rst_ni),
  // Internal MAC pins
  .mac_result_i   (mac_res_data),
  .mac_valid_i    (mac_res_valid),
  .mac_ready_o    (mac_res_ready),
  // Host pins
  .result_valid_o (result_valid_o),
  .result_ready_i (result_ready_i),
  .result_data_o  (result_data_o)
);
