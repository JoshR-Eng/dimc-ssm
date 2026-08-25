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
    void half_clock_cycle(Vcvxif_issue_stage* top, VerilatedContext* contextp,
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


    // Scenario 1: Saturate FIFO
    //      - pass 8 consecutive instructions
    //      - Verify backpressure (ready_o == 0)
    //      - Drain the pipeline and check recovery
    void test_fifo_saturation(Vcvxif_issue_stage* top, VerilatedContext* contextp, VerilatedVcdC* tfp) {
        apply_reset(top, contextp, tfp);

        // The host CPU offloads 8 instructions in a single cycle stream
        for (int i = 0; i < 8; ++i) {
            check(top->issue_ready_o == 1, "Burst: Coprocessor stalled prematurely");

            tick(top, contextp, tfp, [&] {
                top->issue_valid_i = 1;
                top->issue_rs1_i = i + 1; // Inject identifiable operand data
            });
        }

        // The FIFO should now be perfectly full. coprocessor MUST apply backpressure.
        tick(top, contextp, tfp, [&] { top->issue_valid_i = 0; });
        check(top->issue_ready_o == 0, "Backpressure: ready_o remained high when FIFO was full");

        // The MAC engine finishes 8 consecutive operations
        for (int i = 0; i < 8; ++i) {
            tick(top, contextp, tfp, [&] { top->mac_done_i = 1; });
        }

        // The FIFO is empty again, should be ready to accept new host data
        tick(top, contextp, tfp, [&] { top->mac_done_i = 0; });
        check(top->issue_ready_o == 1, "Recovery: ready_o did not reassert after drain");
    }
}



// --- Main Test Loop
int main(int argc, char* argv[]) {

    // --- Initialise Verilator
    // Initialise Verilator context & trace
    const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
    contextp->commandArgs(argc, argv);
    contextp->traceEverOn(true);

    const std::unique_ptr<Vcvxif_issue_stage> top{
        new Vcvxif_issue_stage{contextp.get()}};

    // Setup Waveform Dumping
    VerilatedVcdC* tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("issue_stage_waveform.vcd");


    // --- Run Test cases
    test_fifo_saturation(top.get(), contextp.get(), tfp);

    // --- Handle timeout scenario
    if (contextp->time() > ksimTimeoutCycles * 2) {
        std::cerr << "[FAIL] Timeout Error" << std::endl;
        ++g_fail_count;
    }

    // --- Tidy Up
    top->final();
    tfp->close();
    delete tfp;

    if (g_fail_count == 0) {
        std::cout << "[PASS] All `cvxif_issue_stage` tests passed" << std::endl;
    } else {
        std::cerr << "[FAIL] " << g_fail_count << " check(s) failed" << std::endl;
    }
    return g_fail_count == 0 ? 0 : 1;
}
