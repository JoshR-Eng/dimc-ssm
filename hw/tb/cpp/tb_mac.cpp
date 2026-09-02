#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "testbench.hpp"    // Testbench Template
#include "Vtop_mac_cvxif.h"  // Top Hardware module

#define ASSERT_CHECK(condition, cycle, message)                      \
    do {                                                             \
        if (!(condition)) {                                          \
            std::cerr << "[FATAL] "  << __FILE__  << ":" << __LINE__ \
                      << " @ Cycle " << (cycle)   << "\n"            \
                      << "\tError: " << (message) << std::endl;      \
            exit(EXIT_FAILURE);                                      \
        }                                                            \
    } while (false)


/////////////////////////////////////////
// Compile-time / Global Configuration //
/////////////////////////////////////////

namespace tb_cfg {
    constexpr double   CLK_PERIOD_NS  = 10.0; // 100MHz Default
    constexpr uint64_t TIMEOUT_CYCLES = 200;  // Max cycles till timeout
    constexpr const char* TRACE_FILE  = "waveform.fst";
}   // namespace tb_cfg



///////////////////////////////
// Stimulus & Checking Logic //
///////////////////////////////
namespace testcases {

/* 1. Unimpeded Pipeline Flow
            Stimulus: Drive `results_ready_i = 1`  & Pulse `issue_valid_i = 1`
            Check: `results_valid_o` should assert 3 clock cycles later
                    and `results_data_o` contains correct accumulated product */
    void pipeline_flow_test(Testbench<Vtop_mac_cvxif>& tb) {
        std::cout << "[tb] Beginning Test 1. 'Unimpeded Pipeline Flow'\n";
        tb.reset();

        // Test Inputs
        static uint32_t test_rs1 = 5;
        static uint32_t test_rs2 = 4;
        static uint32_t expected_sum = test_rs1 * test_rs2;

        // Stimulus
        tb.dut()->issue_rs1_i = test_rs1;
        tb.dut()->issue_rs2_i = test_rs2;
        tb.dut()->issue_valid_i = 1;
        tb.dut()->result_ready_i = 1;

        tb.tick();                      // Advance 1 Cycle
        tb.dut()->issue_valid_i = 0;    // Clear stimulus
        tb.tick();                      // advance pipeline by 2 cycles
        tb.tick();

        // Check
        ASSERT_CHECK(tb.dut()->result_valid_o == 1,
                     tb.cycle_count_,
                    "Result no valid after 3 cycles.");
        ASSERT_CHECK(tb.dut()->result_data_o == expected_sum,
                     tb.cycle_count_,
                    "MAC computation incorrect.");
        std::cout << "[PASS] Pipeline Flow Test";
    }

    /* 2. TCDM FIFO Saturation
            Stimulus: Force `result_ready_i = 0` and pass 10 instruction to issue stage
            Check: on 8th instruction FIFO `full_o` should assert, dropping
                 `issue_ready_o` to `0` to backpressure host core */

    /* 3. Skid-Buffer Catch
            Stimulus: Continous stream of instructions. Randomly toggle `result_ready_i`
                      low for random intervals (1-5 cycles)
            Check: `mac_ready_o` should drop, MAC pipeline freezes and NO operand
                    data is dropped or overwritten */

    /* 4. State Persistence & Initilisation
            Stimulus: Assert `issue_clear_i` concurrent with with valid instruction
            Check: Accumulator should wipe previous state and begin fresh accumulation
                   exactly on the clock cycle new data arrives at stage 2
    */
}


//////////////////////////////////////////
//                MAIN                  //
//////////////////////////////////////////

int main(int argc, char* argv[]) {

    Testbench<Vtop_mac_cvxif> tb(argc, argv,
                                tb_cfg::CLK_PERIOD_NS,
                                tb_cfg::TIMEOUT_CYCLES,
                                tb_cfg::TRACE_FILE
                               );

    std::cout << "[tb] Starting Simulation\n";
    tb.reset();

    // --- Test Scenarios ---
    // |--------------------|
    testcases::pipeline_flow_test(tb);
    // --- Test Scenarios ---

    while (!tb.finished()) {
        tb.tick();
    }

    const bool timed_out = tb.cycle_count_ >= tb_cfg::TIMEOUT_CYCLES;
    std::cout << "[tb] Simulation Finished after " << tb.cycle_count_
          << " cycles" << (timed_out ? " (TIMEOUT)" : "") << "\n";

    if (tb.fail_count_ == 0) {
        std::cout << "[PASS] All Test Cases Passed!" << std::endl;
    } else {
        std::cerr << "[FAIL] " << tb.fail_count_ << " check(s) failed" << std::endl;
    }
    return tb.fail_count_ == 0 ? 0 : 1;
}
