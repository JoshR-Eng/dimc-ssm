#include <iostream>
#include <memory>
#include <cassert>
#include "Vcvxif_issue_stage.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

void half_clock_cycle(Vcvxif_issue_stage* top, VerilatorContext* contextp, VerilatedVcdC* tfp) {
    top->clk_i ^= 1;                // Flip clock state
    top->eval();                    // Update hardware
    tfp->dump(contextp->time());    // Dump data to waveform
    contextp->timeInc(1);           // Advance time 1 tick
}

int main(int argc, char* argv[]) {

    // --- Initialise Verilator
    // Initialise Verilator context & trace
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->traceEverOn(true);
    // Instantiate the SystemVerilog Module
    const std::unique_ptr<Vcvxif_issue_stage> top{new Vcvxif_issue_stage{contextp.get()}};

    // Setup Waveform Dumping
    VerilatedVcdC* tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("issue_stage_waveform.vcd");

    // Initialise all input signals
    top->clk_i          = 0;
    top->rst_ni         = 0;
    top->issue_valid_i  = 0;
    top->mac_done_i     = 0;


    // --- Simulation Loop
    int cycle_count = 0;
    int cycle_count_max = 20;
    half_clock_cycle(top.get(), contextp.get(), tfp);

    while (!contextp->gotFinished() && cycle_count < cycle_count_max) {
        half_clock_cycle(top.get(), contextp.get(), tfp);   // Clock LOW

        // Stimulation
        if (cycle_count == 1) {         // Cycle 1-2: Assert reset
            top->rst_ni = 0;
        } else if (cycle_count == 2) {
            top->rst_ni = 1;
        } else if (cycle_count == 4) {  // Cycle 4: Host sends valid data
            top->issue_valid_i = 1;
        } else if (cycle_count == 5) {  // Cycle 5: Host drops valid
            top->issue_valid_i = 0;
        } else if (cycle_count == 8) {  // Cycle 8: MAC finishes
            top->mac_done_i = 1;
        } else if (cycle_count == 9) {  // Cycle 9: MAC drops done
            top->mac_done_i = 0;
        }

        half_clock_cycle(top.get(), contextp.get(), tfp);   // Clock HIGH

        // Evaluation
        if (cycle_count == 5) {
            if (top->issue_ready_o != 0) {
                std::cerr << "[FAIL] Coprocessor failed to deassert ready signal" << std::endl;
                exit(1);
            }
        }

        cycle_count++;
    }

    // --- Tidy Up
    top->final();
    tfp->close();
    delete tfp;
    return 0;
}
