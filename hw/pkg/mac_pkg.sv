// Copyright 2026 Josh Rawlinson
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
/////////////////////////////////////////////////////////////////////////////////////
// Engineer:        Josh Rawlinson - joshrawlinson05@outlook.com
// Date:            2026-08-20
// Design Name:     Coprocessor Package
// Description:
//    Contains package description, enum and structs used within
//    the coprocessor.
//    Current v0.1 Coprocessor is a dummy MAC but will evolve to
//    a complete State-Space Model Accelerator
/////////////////////////////////////////////////////////////////////////////////////

package mac_pkg;

  // Data path sizing
  localparam int DATA_WIDTH = 32;

  // Coprocessor internal opcodes
  typedef enum logic [1:0] {
    OP_NOP = 2'b00, // No Operation
    OP_MAC = 2'b01  // Dummy Multiply-Accumulate
    // Future expansion: OP_SILU, OP_EXP, etc...
  } mac_opcode_e;

  // Internal Instruction payload
  typdef struct packed {
    mac_opcode_e                    opcode;
    logic         [DATA_WIDTH-1:0]  operand_a;
    logic         [DATA_WIDTH-1:0]  operand_b;
  } mac_internal_req_t;

endpackage
