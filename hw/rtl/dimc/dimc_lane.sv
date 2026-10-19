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

typedef enum logic [1:0] {
  IDLE,
  START,
  WAIT,
  COMMIT
} state_e;


// --- Registers
  // State Control
state_e state_q, state_d;         // FSM State Registers
logic [1:0] pass_q, pass_d;           // Which multiply is in progress (0=Ah, 1=Bx, 2=Ch)
  // Internal Variables
logic [DATA_W-1:0] a_bar_q, a_bar_d;  // Coefficient \bar{A}
logic [DATA_W-1:0] b_bar_q, b_bar_d;  // Coefficient \bar{B}
logic [DATA_W-1:0] c_q, c_d;          // Coefficient c
logic [DATA_W-1:0] x_q, x_d;          // Input Sample x
logic [DATA_W-1:0] h_q, h_d;          // State h
  // Output
logic [LANE_ACC_W-1:0] acc_q, acc_d;
logic [(DATA_W*2)-1:0] yp_q, yp_d;
logic                  out_valid_q, out_valid_d;


// --- Combinational Datapath
  // combinational variables
logic                  mult_start;
logic [DATA_W-1:0]     mult_a;
logic [DATA_W-1:0]     mult_b;
logic [DATA_W-1:0]     h_next;
logic [LANE_ACC_W-1:0] prod_ext;
logic [LANE_ACC_W-1:0] acc_sum;
logic                  mult_done;
logic                  mult_busy;
logic [(DATA_W*2)-1:0] mult_product;

  // Flag that asserts when machine is in the START state
assign mult_start = (state_q == START);

  // Mux that selects which operands to pass into bit-serial multiplier
always_comb begin : operand_mux
  unique case (pass_q)
    2'd0: begin
      mult_a = a_bar_q;
      mult_b = h_q;
    end

    2'd1: begin
      mult_a = b_bar_q;
      mult_b = x_q;
    end

    2'd2: begin
      mult_a = c_q;
      mult_b = h_q;
    end

    default: begin
      mult_a = '0;
      mult_b = '0;
    end
  endcase
end // operand_mux

  // Performs signed bit extension
assign prod_ext = {mult_product[(2*DATA_W)-1], mult_product};
  // Sum the accumulator output and sign-extended product
assign acc_sum = acc_q + prod_ext;

// --- FSM Logic
always_comb begin : FSM
  // Default - every register holds current state
  c_d     = c_q;
  h_d     = h_q;
  x_d     = x_q;
  yp_d    = yp_q;
  acc_d   = acc_q;
  pass_d  = pass_q;
  a_bar_d = a_bar_q;
  b_bar_d = b_bar_q;
  state_d = state_q;
  out_valid_d = 1'b0;

  unique case (state_q)
      IDLE: begin
        if (wr_en_i) begin
          // Load the coeff or state selected
          // by `wr_sel_i` - only in IDLE
          // values cna't change when running
          case (wr_sel_i)
            2'd0: a_bar_d = wr_data_i;
            2'd1: b_bar_d = wr_data_i;
            2'd2: c_d     = wr_data_i;
            2'd3: h_d     = wr_data_i;
          endcase
        end
        if (in_valid_i) begin
          x_d     = x_i;
          pass_d  = '0;
          state_d = START;
        end
      end // IDLE

      START: begin
        // Single Cycle State
        //  giving each pass its own START
        //  means operand muxes are already settled
        state_d = WAIT;
      end // START

      WAIT: begin
        if (mult_done) begin
          unique case (pass_q)
            2'd0: begin
              acc_d   = prod_ext;
              pass_d  = 2'd1;
              state_d = START;
            end
            2'd1: begin
              acc_d   = acc_sum;
              state_d = COMMIT;
            end
            2'd2: begin
              yp_d = mult_product;
              out_valid_d = 1'b1;
              state_d = IDLE;
            end
            default: begin
              state_d = IDLE;
            end
          endcase
        end
      end // WAIT

      COMMIT: begin
        h_d     = h_next;
        pass_d  = 2'd2;
        state_d = START;
      end // COMMIT
  endcase
end

// --- Sequential Register Updates
always_ff @(posedge clk_i or negedge rst_ni ) begin
  if (!rst_ni) begin
    state_q     <= IDLE;
    pass_q      <= '0;
    a_bar_q     <= '0;
    b_bar_q     <= '0;
    c_q         <= '0;
    x_q         <= '0;
    h_q         <= '0;
    acc_q       <= '0;
    yp_q        <= '0;
    out_valid_q <= 1'b0;
  end else begin
    state_q     <= state_d;
    pass_q      <= pass_d;
    a_bar_q     <= a_bar_d;
    b_bar_q     <= b_bar_d;
    c_q         <= c_d;
    x_q         <= x_d;
    h_q         <= h_d;
    acc_q       <= acc_d;
    yp_q        <= yp_d;
    out_valid_q <= out_valid_d;
  end
end


// --- Module Instantiation
  // Bit-serial multiplier: one W x W product per pass (W+1 cycles)
bs_mult #(
  .DATA_WIDTH (DATA_W)
) u_bs_mult (
  .clk_i      (clk_i),
  .rst_ni     (rst_ni),
  .start_i    (mult_start),
  .busy_o     (mult_busy),
  .done_o     (mult_done),
  .a_i        (mult_a),        // stationary coefficient (A_bar, B_bar, C)
  .b_i        (mult_b),        // streamed operand (h or x), captured on start
  .product_o  (mult_product)
);

  // Requantiser: acc_q (Q.26, 33 b) -> h_next (Q3.13, 16 b), round-half-up + saturate
rnd_sat #(
  .IN_W   (LANE_ACC_W),
  .SHIFT  (FRAC),
  .OUT_W  (DATA_W)
) u_rnd_sat (
  .in_i   (acc_q),
  .out_o  (h_next)
);

// --- Output assignment
  // Assign internal variables to the outputs
assign in_ready_o  = (state_q == IDLE);
assign out_valid_o = out_valid_q;
assign h_o         = h_q;
assign  yp_o        = yp_q;

endmodule
