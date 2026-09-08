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
  /* verilator lint_off UNUSEDSIGNAL */
  input   logic [31:0]            issue_instr_i,    // 32-bit RISC-V Instruction
  /* verilator lint_on UNUSEDSIGNAL */
  input   logic [DATA_WIDTH-1:0]  issue_rs1_i,      // Instruction Operand 1
  input   logic [DATA_WIDTH-1:0]  issue_rs2_i,      // Instruction Operand 2
  // Coprocessor MAC Interface (dummy MAC for this v0.1 phase)
  input   logic                   mac_ready_i,
  output  logic [DATA_WIDTH-1:0]  mac_rs1_o,
  output  logic [DATA_WIDTH-1:0]  mac_rs2_o,
  output  logic                   mac_clear_o,
  output  logic                   mac_valid_o
);

  // --- Payload Definition
  localparam int unsigned PAYLOAD_WIDTH = 1 + (DATA_WIDTH * 2);
  typedef struct packed {
    logic [DATA_WIDTH-1:0] rs1_data;
    logic [DATA_WIDTH-1:0] rs2_data;
    logic                  clear_accum;
  } issue_payload_t;

  issue_payload_t fifo_data_in, fifo_data_out;

  // --- Instruction Decoder
  logic is_custom_opcode;
  logic is_clear_funct3;

  always_comb begin : instruction_decode
    // Use RISC-V custom instruction CUSTOM-O (0x0B) and Funct3 = 1
    is_custom_opcode = (issue_instr_i[6:0]    == 7'h0B);
    is_clear_funct3  = (issue_instr_i[14:12]  == 3'h1);
    // Pack the struct for the FIFO
    fifo_data_in.rs1_data    = issue_rs1_i;
    fifo_data_in.rs2_data    = issue_rs2_i;
    fifo_data_in.clear_accum = (is_custom_opcode & is_clear_funct3);
  end


  // --- Handshake Logic
  logic fifo_full;
  logic fifo_empty;

  // Coprocessor ready to accept new instruction as long as FIFO isn't full
  assign issue_ready_o = ~fifo_full;

  //  MAC has valid data to compute if FIFO not empty
  assign mac_valid_o = ~fifo_empty;


  // --- Payload Extraction
  // Unpack struct for MAC engine
  assign mac_rs1_o   = fifo_data_out.rs1_data;
  assign mac_rs2_o   = fifo_data_out.rs2_data;
  assign mac_clear_o = fifo_data_out.clear_accum;

// --- FIFO Instantiation
  async_fifo #(
    .DEPTH(8),
    .DATA_WIDTH(PAYLOAD_WIDTH) // Passes the calculated struct width
  ) i_instruction_fifo (
    // Host (Write) Domain
    .wr_clk_i   (clk_i),       // Temporarily tied to system clock
    .wr_rst_ni  (rst_ni),      // Temporarily tied to system reset
    .push_i     (issue_valid_i & issue_ready_o),
    .data_i     (fifo_data_in),
    .full_o     (fifo_full),

    // Coprocessor (Read) Domain
    .rd_clk_i   (clk_i),       // Temporarily tied to system clock
    .rd_rst_ni  (rst_ni),      // Temporarily tied to system reset
    .pop_i      (mac_valid_o & mac_ready_i),
    .data_o     (fifo_data_out),
    .empty_o    (fifo_empty)
  );

endmodule
