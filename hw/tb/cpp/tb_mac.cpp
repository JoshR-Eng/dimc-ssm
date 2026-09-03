#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <queue>

#include "testbench.hpp"    // Testbench Template
#include "Vtop_mac_cvxif.h"  // Top Hardware module

#define ASSERT_CHECK(condition, cycle, message)                      \
    do {                                                             \
        if (!(condition)) {                                          \
            std::cerr << "[FATAL] "  << __FILE__  << ":" << __LINE__ \
                      << " @ Cycle " << (cycle)   << "\n"            \
                      << "\tError: " << (message) << std::endl;      \
            throw std::runtime_error("Testbench Assertion Failed");  \
        }                                                            \
    } while (false)


/////////////////////////////////////////
// Compile-time / Global Configuration //
/////////////////////////////////////////

namespace tb_cfg {
    constexpr double   CLK_PERIOD_NS  = 10.0; // 100MHz Default
    constexpr uint64_t TIMEOUT_CYCLES = 500;  // Max cycles till timeout
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

        // A Struct used to store the desired data and a cycle timestamp in the queue
        struct Transaction {
            uint32_t expected_data;
            uint64_t injection_cycle;
        };

        std::queue<Transaction> scoreboard;    // Init model scoreboard

        // Stimuls (Stream in multiple inputs)
        tb.dut()->result_ready_i = 1;

        for (uint32_t i = 1; i <= 3; ++i) {
            uint32_t rs1 = i * 2;
            uint32_t rs2 = 5;
            uint32_t expected_sum = rs1 * rs2;

            scoreboard.push({expected_sum, tb.cycle_count_}); // Push expected results into software FIFO

            // Push results into hardware fifo
            tb.dut()->issue_rs1_i = rs1;
            tb.dut()->issue_rs2_i = rs2;
            tb.dut()->issue_valid_i = 1;

            tb.tick();
        }
        tb.dut()->issue_valid_i = 0; // Stop driving input

        // Monitor
        int timeout = 0;
        while (!scoreboard.empty() && timeout <20) {
            tb.tick();
            timeout++;

            if (tb.dut()->result_valid_o == 1) {
                Transaction current_tx = scoreboard.front();
                scoreboard.pop();

                // Calculate latency between data ingestion and output
                uint64_t latency = tb.cycle_count_ - current_tx.injection_cycle;

                std::cout << "[tb] \tMAC Result: " << tb.dut()->result_data_o
                          << " | Latency: " << latency << " cycles\n";

                ASSERT_CHECK(tb.dut()->result_data_o == current_tx.expected_data,
                             tb.cycle_count_,
                            "MAC computation incorrect");
            }
        }
        ASSERT_CHECK(scoreboard.empty(), tb.cycle_count_, "Pipeline swallowed data");
        std::cout << "[PASS] Pipeline Flow Test\n";
    }

    /* 2. TCDM FIFO Saturation
            Stimulus: Force `result_ready_i = 0` and pass 10 instruction to issue stage
            Check: on 8th instruction FIFO `full_o` should assert, dropping
                 `issue_ready_o` to `0` to backpressure host core */
    void fifo_saturation_test(Testbench<Vtop_mac_cvxif>& tb) {
        std::cout << "[tb] Beginning Test 2. 'TCDM FIFO Saturation'\n";
        tb.reset();

        tb.dut()->result_ready_i = 0; // Force downstream bottleneck by not accepting MAC result

        // Stimulus
        tb.dut()->issue_valid_i = 1;
        int instructions_pushed = 0;

        for (int i = 1; i <= 15; i++) {
            tb.dut()->issue_rs1_i = i;
            tb.dut()->issue_rs2_i = 2;

            // Check if the hardware is accepting data on this exact cycle
            bool accepted_this_cycle = (tb.dut()->issue_ready_o == 1);

            if (accepted_this_cycle) {
                instructions_pushed++;
            }

            tb.tick();

            // Check backpressure assertion
            //  FIFO depth is 8 but downstream pipeline (MAC & Skid-buffer) holds
            //  exactly 3 instructions as well so check at == 11
            if (instructions_pushed == 11) {
                // Since our FIFO depth = 8, it should now be full
                // The combinational ready singal must drop immediately.
                ASSERT_CHECK(tb.dut()->issue_ready_o == 0,
                             tb.cycle_count_,
                            "Backpressure failure: Coprocessor left `ready_o` HIGH");
                std::cout<< "[tb] Coprocessor successfully stalled at 8 instructions\n";
                break;
            }
        }
        tb.dut()->issue_valid_i = 0;
        std::cout << "[PASS] TCDM FIFO Saturation Test\n";
    }

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

    try {
        // --- Test Scenarios ---
        // |--------------------|
        testcases::pipeline_flow_test(tb);
        testcases::fifo_saturation_test(tb);
        // |--------------------|
        // --- Test Scenarios ---

        while (!tb.finished()) {
            tb.tick();
        }
    } catch (const std::runtime_error& e) {
        tb.fail_count_++;
        std::cerr << "[tb] Simulation halted due to exception" << std::endl;
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
