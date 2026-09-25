#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <queue>
#include <random>

#include "testbench.hpp"    // Testbench Template
#include "Vbs_mult.h"  // Top Hardware module

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
    void multiply_test(Testbench<Vtop_mac_cvxif>& tb) {
        std::cout << "[tb] Beginning test 1. 'Multiply_test'" << std::endl
        tb.reset();

        struct Transaction {
            uint32_t expected_data;
            uint64_t injection_cycle;
        };

        std::queue<Transaction> scoreboard;

        for (uint16_t = 0; i < 10; i++) {
            do {
            uint16_t a = 5;
            uint16_t b = i;
            uint32_t expected_prod = 5 * i;

            scoreboard.push({expected_prod, tb.cycle_count_})

            tb.dut()->a_i = a;
            tb.dut()->b_i = b;
            tb.dut()->start_i = 1;
            } while (i < 5)

            tb.tick;

            if (tb.dut()->done_o == 1) {
                Transaction current_tx = scoreboard.front()
                scoreboard.pop()

                // Calculate latency between data ingestion and output
                uint64_t latency = tb.cycle_count_ - current_tx.injection_cycle;

                std::cout << "[tb] \tBit-Serial Multiplier Result: " << tb.dut()->result_data_o
                          << " | Latency: " << latency << " cycles\n";

                ASSERT_CHECK(tb.dut()->product_o == current_tx.expected_data,
                             tb.cycle_count_,
                            "MAC computation incorrect");
            }
        }
    }
}   // namespace testcases


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
        testcases::multiply_test(tb);
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
