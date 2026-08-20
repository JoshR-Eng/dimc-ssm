#include <iostream>
#include <memory>
#include <cassert>
#include "Vcvxif_issue_stage.h"
#include "verilated.h"
#include "verilated_vcd_c.h"


// --- Helper Fuctions & Test Cases
namespace {
    constexpr int ksimTimeoutCycles = 200;  // Immutable Timeout Limit
    int g_fail_count = 0;                   // Failed Test Counter

    // --- Helper Functions
    void check(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[FAIL] " << message << std::endl;
            ++g_fail_count;
        }
    }

    // Drives one half period of clk-I and advances sim trace by one tick
    void half_clock_cycle(Vcvxif_issue_stage* top, VerilatorContext* contextp,
                          VerilatedVcdC* tfp) {
        top->clk_i ^= 1;                // Flip clock state
        top->eval();                    // Update hardware
        tfp->dump(contextp->time());    // Dump data to waveform
        contextp->timeInc(1);           // Advance time 1 tick
    }

    // Drives full clock cycle & can apply test stimulus before rising edge
    void tick(Vcvxif_issue_stage* top, VerilatedContext* contextp,
              VerilatedVcdC* tfp, const std::function<void()>& stimulus = nullptr) {
        half_clock_cycle(top, contextp, tfp); // clk_i >> LOW
        if (stimulus) stimulus();
        half_clock_cycle(top, contextp, tfp); // clk_i >> HIGH
    }

    // Reset Variables and tick one clock cycle
    void apply_reset(Vcvxif_issue_stage* top, VerilatedContext* contextp,
                     VerilatedVcdC* tfp) {
        top->rst_ni         = 0;
        top->issue_valid_i  = 0;
        top->issue_rs1_i    = 0;
        top->issue_rs2_i    = 0;
        top->mac_done_i     = 0;
        for (int i = 0; i < 3; ++i) tick(top, contextp, tfp);
        top->rst_ni = 1;
        tick(top, contextp, tfp);

        check(top->issue_ready_o == 1,
            "Reset: `issue_ready_o` not high right after reset");
    }


    // --- Test Cases
    // Scenario 1: Complete handshake
    //      - Accept
    //      - Ready deasserts while busy
    //      - Relase on `mac_done_i`
    void test_handshake(Vcvxif_issue_stage* top, VerilatedContext* contextp,
                        VerilatedVcdC* tfp) {
        apply_reset(top, contextp, tfp);
        tick(top, contextp, tfp, [&] { top->issue_valid_i = 1; });
        tick(top, contextp, tfp, [&] { top->issue_valid_i = 1; });
        check(top->issue_ready_o == 0,
                "'test_handshake': Coprocessor did not deassert ready signal");

        for (int i = 0; i < 3; ++i) {
            tick(top, contextp, tfp);
            check(top->issue_ready_o == 0,
                    "'test_handshake': ready remained high while still busy");
        }

        tick(top, contextp, tfp, [&] { top->mac_done_i = 1; });
        tick(top, contextp, tfp, [&] { top->mac_done_i = 0; });
        check(top->issue_ready_o == 1,
                "'test_handshake': ready did not reassert after `mac_done_i`");
    }
}



// --- Main Test Loop
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
