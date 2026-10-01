// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-08-25
// Design Name:     Bit-Serial Multiplier
// Description:
//  Bit-serial shift-add multiplier, the compute primitive of one DIMC row
//  the input operand (b) is multiplies the static coefficient (a) one bit
//  per cycle against the whole of 'a' in parallel, LSB first
/////////////////////////////////////////////////////////////////////////////////////

module bs_mult #(
  parameter int unsigned DATA_WIDTH = 16
) (
  input logic clk_i,
  input logic rst_ni,
    // Multiplier Flags
  input  logic start_i,
  output logic busy_o,
  output logic done_o,
    // Operands
  input [DATA_WIDTH-1:0] a_i,  // multiplicand - The no. being multipled (e.g. A_bar, B_bar, etc.)
  input [DATA_WIDTH-1:0] b_i,  // multiplier - e.g. h_k, x_k
  output [(2 * DATA_WIDTH)-1:0] product_o // Final result {prod_hi_q, prod_lo_q}
);

localparam int unsigned CNT_WIDTH = $clog2(DATA_WIDTH);

typedef enum logic {
  IDLE,
  SHIFT
} state_e;


// --- Registers
state_e                state_q, state_d;     // State registers
logic [CNT_WIDTH-1:0]  bit_cnt_q, bit_cnt_d; // Counts number of shift-add's
logic [DATA_WIDTH-1:0] prod_hi_q, prod_hi_d; // Upper half product register, running partial Sum
logic [DATA_WIDTH-1:0] prod_lo_q, prod_lo_d; // Lower half product register
logic                  done_q, done_d;



/// --- 1. Combinational Datapath: one shift-add step
logic [DATA_WIDTH-1:0] addend;  // multiplicand if multiplier bit is 1, else 0
logic [DATA_WIDTH:0]   sum;     // bit-serial sum +1 to hold the carry out
logic [DATA_WIDTH-1:0] step_hi; // upper half of single step shift
logic [DATA_WIDTH-1:0] step_lo; // lower half of single step shift

// Evaluate the inputs and output the sum
assign addend = prod_lo_q[0] ? a_i : '0;
assign sum    = $signed({prod_hi_q[DATA_WIDTH-1], prod_hi_q}
                      + {addend[DATA_WIDTH-1]   , addend}); // widen to protect carry out

// Shift Logic
assign step_hi = sum[DATA_WIDTH:1];
assign step_lo = {sum[0], prod_lo_q[DATA_WIDTH-1:1]};



// --- 2. Control Logic (FSM)
always_comb begin : control
  // Default: every register holds it value
  state_d   = state_q;
  bit_cnt_d = bit_cnt_q;
  prod_hi_d = prod_hi_q;
  prod_lo_d = prod_lo_q;
  done_d    = 1'b0;

  unique case (state_q)
    IDLE: begin
      if (start_i) begin
        prod_hi_d = '0;
        prod_lo_d = b_i;
        bit_cnt_d = '0;
        state_d   = SHIFT;
      end
    end

    SHIFT: begin
      prod_hi_d = step_hi;
      prod_lo_d = step_lo;
      bit_cnt_d = bit_cnt_q + 1'b1;
      if (bit_cnt_q == CNT_WIDTH'(DATA_WIDTH-1)) begin
        state_d = IDLE;
        done_d  = 1'b1;
      end
    end

    default: state_d = IDLE;
  endcase
end // control



// --- 3. Sequential Register Updates
always_ff @(posedge clk_i or negedge rst_ni ) begin : registers
 if (!rst_ni) begin
  state_q   <= IDLE;
  bit_cnt_q <= '0;
  prod_hi_q <= '0;
  prod_lo_q <= '0;
  done_q    <= 1'b0;
 end else begin
  state_q   <= state_d;
  bit_cnt_q <= bit_cnt_d;
  prod_hi_q <= prod_hi_d;
  prod_lo_q <= prod_lo_d;
  done_q    <= done_d;
 end
end // registers



// --- Output Assignment
assign busy_o    = state_q == IDLE ? 1'b0 : 1'b1;
assign done_o    = done_q;
assign product_o = {prod_hi_q, prod_lo_q};

endmodule
