// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-09-06
// Design Name:     Asynchronous FIFO
// Description:
//   Asynchronous FIFO module
//   Utilised to enable writes from a high speed transmitter
//   and reads from a lower speed receiver.
/////////////////////////////////////////////////////////////////////////////////////

module async_fifo#(
  parameter int unsigned DEPTH       = 8,
  parameter int unsigned DATA_WIDTH  = 32
) (
  // Host (Write)
  input                   wr_clk_i,
  input                   wr_rst_ni,
  input                   push_i,
  input  [DATA_WIDTH-1:0] data_i,
  output                  full_o,
  // Coprocessor (Read)
  input                   rd_clk_i,
  input                   rd_rst_ni,
  input                   pop_i,
  output [DATA_WIDTH-1:0] data_o,
  output                  empty_o
);

// --- Local Variable declaration
localparam int PTR_WIDTH = $clog2(DEPTH) + 1;

logic [DATA_WIDTH-1:0] mem_q [0:DEPTH-1]; // FIFO Memory Array


// --- WRITE Domain
logic [PTR_WIDTH-1:0] wr_ptr_bin_q, wr_ptr_bin_d;
logic [PTR_WIDTH-1:0] wr_ptr_gray_q, wr_ptr_gray_d;
logic [PTR_WIDTH-2:0] wr_addr;

always_comb begin
  wr_ptr_bin_d = wr_ptr_bin_q;
  if (push_i && !full_o) begin
    wr_ptr_bin_d = wr_ptr_bin_q + 1'b1; // Increment Binary
  end
  wr_ptr_gray_d = wr_ptr_bin_d ^ (wr_ptr_bin_d >> 1); // Converty binary to gray code
end

assign wr_addr = wr_ptr_bin_q[PTR_WIDTH-2:0];

always_ff @(posedge wr_clk_i or negedge wr_rst_ni) begin
  if (!wr_rst_ni) begin
    wr_ptr_bin_q  <= '0;
    wr_ptr_gray_q <= '0;
  end else begin
    wr_ptr_bin_q  <= wr_ptr_bin_d;
    wr_ptr_gray_q <= wr_ptr_gray_d;

    if (push_i && !full_o) begin
      mem_q[wr_addr] <= data_i;
    end
  end
end



// --- READ Domain
logic [PTR_WIDTH-1:0] rd_ptr_bin_q, rd_ptr_bin_d;
logic [PTR_WIDTH-1:0] rd_ptr_gray_q, rd_ptr_gray_d;
logic [PTR_WIDTH-2:0] rd_addr;

always_comb begin
  rd_ptr_bin_d = rd_ptr_bin_q;
  if (pop_i && !empty_o) begin
    rd_ptr_bin_d = rd_ptr_bin_q + 1'b1;
  end
  rd_ptr_gray_d = rd_ptr_bin_d ^ (rd_ptr_bin_d >> 1);
end

assign rd_addr = rd_ptr_bin_q[PTR_WIDTH-2:0];
assign data_o  = mem_q[rd_addr]; // First-Word Fall-Through

always_ff @(posedge rd_clk_i or negedge rd_rst_ni ) begin
  if (!rd_rst_ni) begin
    rd_ptr_bin_q  <= '0;
    rd_ptr_gray_q <= '0;
  end else begin
    rd_ptr_bin_q  <= rd_ptr_bin_d;
    rd_ptr_gray_q <= rd_ptr_gray_d;
  end
end



// --- CLOCK DOMAIN CROSSING: 2-stage synchroniser
logic [PTR_WIDTH-1:0] wr_ptr_gray_sync1_q, wr_ptr_gray_sync2_q;
logic [PTR_WIDTH-1:0] rd_ptr_gray_sync1_q, rd_ptr_gray_sync2_q;

// Sync Write Pointer into Read Domain
always_ff @(posedge rd_clk_i or negedge rd_rst_ni) begin
  if (!rd_rst_ni) begin
    wr_ptr_gray_sync1_q <= '0;
    wr_ptr_gray_sync2_q <= '0;
  end else begin
    wr_ptr_gray_sync1_q <= wr_ptr_gray_q;
    wr_ptr_gray_sync2_q <= wr_ptr_gray_sync1_q;
  end
end

// Sync Read Pointer into Write Domain
always_ff @(posedge wr_clk_i or negedge wr_rst_ni) begin
  if (!wr_rst_ni) begin
    rd_ptr_gray_sync1_q <= '0;
    rd_ptr_gray_sync2_q <= '0;
  end else begin
    rd_ptr_gray_sync1_q <= rd_ptr_gray_q;
    rd_ptr_gray_sync2_q <= rd_ptr_gray_sync1_q;
  end
end



// --- EMPTY & FULL Flag Assignment
// EMPTY: Evaluated in read domain using synced write pointer
assign empty_o = (rd_ptr_gray_q == wr_ptr_gray_sync2_q);

// FULL: Evaluted in write domain using synced read pointer
//       It works since in grey code once the write pointer
//       has looped back around the MSB & MSB-1 are the inverse
//       of the read pointers yet MSB-2:0 are the exact same
assign full_o = (wr_ptr_gray_q == {~rd_ptr_gray_sync2_q[PTR_WIDTH-1:PTR_WIDTH-2],
                                    rd_ptr_gray_sync2_q[PTR_WIDTH-3:0]        });

endmodule
