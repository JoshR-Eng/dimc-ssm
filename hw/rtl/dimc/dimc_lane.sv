// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-10-18
// Design Name:     Digital In-memory Compute Lane
// Description:
//    [[TODO: Complete this description]]
/////////////////////////////////////////////////////////////////////////////////////

module dimc_lane #(
  parameter int unsigned DATA_W     = dimc_pkg::DATA_W,
  parameter int unsigned FRAC       = dimc_pkg::FRAC,
  parameter int unsigned LANE_ACC_W = dimc_pkg::LANE_ACC_W
) (
  input logic clk_i,
  input logic rst_ni,

  input  logic in_valid_i,
  output logic in_ready_o,
  output logic out_valid_o,

  input  logic [DATA_W-1:0]     x_i,
  output logic [DATA_W-1:0]     h_o,
  output logic [(DATA_W*2)-1:0] yp_o,

  input logic              wr_en_i,
  input logic [1:0]        wr_sel_i,
  input logic [DATA_W-1:0] wr_data_i
);

typedef enum logic {
  IDLE,
  START,
  WAIT,
  COMMIT
} state_e;


// --- Registers
  // State Control
logic [1:0] state_q, state_d;         // FSM State Registers
logic [1:0] coef_sel_q, coef_sel_d;   // Coefficient Selection Registers
  // Internal Variables
logic [DATA_W-1:0] a_bar_q, a_bar_d;  // Coefficient \bar{A}
logic [DATA_W-1:0] b_bar_q, b_bar_d;  // Coefficient \bar{B}
logic [DATA_W-1:0] c_q, c_d;          // Coefficient c
logic [DATA_W-1:0] x_q, x_d;          // Coefficient x
logic [DATA_W-1:0] h_q, h_d;          // Coefficient h
  // Output
logic [LANE_ACC_W-1:0] acc_q, acc_d;
logic [(DATA_W*2)-1:0] yp_q, yp_d;
logic                  out_valid_q, out_valid_d;


// --- Combinational Datapath
logic                  mult_start;
logic [DATA_W-1:0]     mult_a;
logic [DATA_W-1:0]     mult_a;
logic [DATA_W-1:0]     h_next;
logic [LANE_ACC_W-1:0] prod_ext;
logic [LANE_ACC_W-1:0] acc_sum;

// --- FSM Logic
  // TODO: Use unique case to define each state
always_comb begin : FSM
  // TODO: Add default - each variable holds it original state
  unique case (state_q)
      IDLE: begin

      end

      START: begin

      end

      WAIT: begin

      end

      COMMIT: begin

      end
  endcase
end

// --- Sequential Register Updates
always_ff @(posedge clk_i or negedge rst_ni ) begin
  if (!rst_ni) begin
    // TODO: Set Default Values
  end else begin

  end
end


// --- Module Instantiation
// TODO: Add `bs_mult.sv` instant...

// --- Output assignment
  // Assign internal variables to the outputs

endmodule
