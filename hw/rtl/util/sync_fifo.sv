// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-08-22
// Design Name:     FIFO
// Description:
//   Simple FIFO module
//   Primary implementation is to store instruction operands
//   recieved by coprocessor from host
/////////////////////////////////////////////////////////////////////////////////////

module sync_fifo#(
  parameter int unsigned DEPTH       = 8,
  parameter int unsigned DATA_WIDTH  = 32
) (
  input                   clk_i,
  input                   rst_ni,
  input                   push_i,
  input                   pop_i,
  input  [DATA_WIDTH-1:0] data_i,
  output [DATA_WIDTH-1:0] data_o,
  output                  full_o,
  output                  empty_o
);

// --- Local Variable declaration
localparam int PTR_WIDTH = $clog2(DEPTH) + 1;

logic [PTR_WIDTH-1:0] wr_ptr_d, wr_ptr_q;
logic [PTR_WIDTH-1:0] rd_ptr_d, rd_ptr_q;
logic [PTR_WIDTH-2:0] wr_addr;
logic [PTR_WIDTH-2:0] rd_addr;

logic [DATA_WIDTH-1:0] mem_q [0:DEPTH-1]; // FIFO Memory Array


// --- Pointer Slicing (Seperate Address & Empty/Full Flags)
assign wr_addr = wr_ptr_q[PTR_WIDTH-2:0];
assign rd_addr = rd_ptr_q[PTR_WIDTH-2:0];
// EMPTY: read/write pointers are the same
assign empty_o = (wr_ptr_q == rd_ptr_q);
// FULL: Pointer MSBs are diff. but lower address bits are identical
assign full_o  = (wr_ptr_q[PTR_WIDTH-1]   != rd_ptr_q[PTR_WIDTH-1]) &&
                 (wr_ptr_q[PTR_WIDTH-2:0] == rd_ptr_q[PTR_WIDTH-2:0]);


// --- Continous Output Read (FWFT FIFO)
assign data_o = mem_q[rd_addr];


// --- Increment Pointer LOgic
always_comb begin : pointer_increment_logic
  wr_ptr_d = wr_ptr_q;
  rd_ptr_d = rd_ptr_q;

  if (push_i && !full_o) begin
    wr_ptr_d = wr_ptr_q + 1'b1;
  end
  if (pop_i && !empty_o) begin
    rd_ptr_d = rd_ptr_q + 1'b1;
  end
end

// --- Sequential State Updates
always_ff @(posedge clk_i or negedge rst_ni) begin: ptr_registers
  if (!rst_ni) begin
    wr_ptr_q <= '0;
    rd_ptr_q <= '0;
  end else begin
    // Update Pointers
    wr_ptr_q <= wr_ptr_d;
    rd_ptr_q <= rd_ptr_d;

    // Push memory into FIFO
    if (push_i && !full_o) begin
      mem_q[wr_addr] <= data_i;
    end
  end
end
endmodule
