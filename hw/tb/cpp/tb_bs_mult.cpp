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

// Multiplier Config
#ifndef TB_WIDTH
#define TB_WIDTH 16
#endif
constexpr uint32_t OPERAND_MASK = (1u << TB_WIDTH) - 1;

namespace tb_cfg {
    constexpr double   CLK_PERIOD_NS  = 10.0; // 100MHz Default
    constexpr uint64_t TIMEOUT_CYCLES = 1500000;  // Max cycles till timeout
    constexpr const char* TRACE_FILE  = "waveform.fst";
}   // namespace tb_cfg



//////////////////////
// Helper Functions //
//////////////////////

struct MulResult {
    uint64_t product;
    uint64_t latency;
};

namespace helpfunc{

    // Software bit-serial Multiplier
    MulResult mult(Testbench<Vbs_mult>& tb, uint32_t a, uint32_t b,
                       uint64_t max_wait = 100) {
        tb.dut()->a_i     = a & OPERAND_MASK;
        tb.dut()->b_i     = b & OPERAND_MASK;
        tb.dut()->start_i = 1;
        tb.tick();
        tb.dut()->start_i = 0;
        tb.dut()->b_i     = ~b & OPERAND_MASK;

        uint64_t latency = 1;
        while (!tb.dut()->done_o) {
            ASSERT_CHECK(latency < max_wait, tb.cycle_count_,
                        "Timeout: done_o never asserted");
            tb.tick();
            ++latency;
        }
        return {tb.dut()->product_o, latency};
    } // MulResult mult
} // namespace helpfunc


///////////////////////////////
// Stimulus & Checking Logic //
///////////////////////////////
namespace testcases {
    void multiply_test(Testbench<Vbs_mult>& tb) {
        std::cout << "[tb] Beginning test 1. 'Multiply_test'\n";
        tb.reset();

        const uint32_t cases[][2] = {{0xF, 0xA}, {0xD, 0xB}};
        for (const auto& c : cases) {
            const MulResult r       = helpfunc::mult(tb, c[0], c[1]);
            const uint64_t expected = uint64_t(c[0]) * c[1];

            ASSERT_CHECK(r.product == expected, tb.cycle_count_,
                        "Multiplication Results: " + std::to_string(r.product) +
                        ", expected " + std::to_string(expected));
            ASSERT_CHECK(r.latency == TB_WIDTH + 1, tb.cycle_count_,
                        "Multplication Latency " + std::to_string(r.latency) +
                        ", expected " + std::to_string(TB_WIDTH + 1));
        }
        std::cout << "[PASS] Multiplication test\n";
    }   // multiply test



    void exhaustive_test(Testbench<Vbs_mult>& tb) {
        if (TB_WIDTH > 8) {
            std::cout << "[tb] Test 2 'Exhaustive' skipped (TB_WIDTH > 8)\n";
            return;
        }
        std::cout << "[tb] Beginning test 2. 'Exhaustive'\n";
        tb.reset();

        uint64_t checks = 0;
        for (uint32_t a = 0; a <= OPERAND_MASK; ++a) {
            for (uint32_t b = 0; b <= OPERAND_MASK; b++) {
                const MulResult r        = helpfunc::mult(tb, a, b);
                const __int64_t expected = uint64_t(a) * b;
                ASSERT_CHECK(r.product == expected, tb.cycle_count_,
                            "a=" + std::to_string(a) + " b=" + std::to_string(b) +
                            ": Output " + std::to_string(r.product) +
                            ", expected " + std::to_string(expected));
                ++checks;
            }
        }
        const uint64_t n = uint64_t(OPERAND_MASK) + 1;
        ASSERT_CHECK(checks == n*n, tb.cycle_count_,
                    "Ran " + std::to_string(checks) + " checks, expected "
                    + std::to_string(n*n));
        std::cout<< "[PASS] Exhaustive test (" << checks << " products)\n";
    } // exhaustive test



    void random_test(Testbench<Vbs_mult>& tb) {
        std::cout << "[tb] Test 3. Random + Corner Cases\n";
        tb.reset();

        uint64_t checks = 0;
        const uint32_t corners[] = {
            0u,                             // Zero
            1u,                             // One
            OPERAND_MASK,                   // All ones
            OPERAND_MASK - 1u,              // All ones except LSB
            1u << (TB_WIDTH - 1),           // All zeros except MSB
            0x55555555u & OPERAND_MASK,     // 0101... alternating
            0xAAAAAAAAu & OPERAND_MASK,     // 1010... alternating
        };

        for (const uint32_t a : corners) {
            for (const uint32_t b : corners) {
                const MulResult r        = helpfunc::mult(tb, a, b);
                const __int64_t expected = uint64_t(a) * b;
                ASSERT_CHECK(r.product == expected, tb.cycle_count_,
                            "a=" + std::to_string(a) + " b=" + std::to_string(b) +
                            ": Output " + std::to_string(r.product) +
                            ", expected " + std::to_string(expected));
                ++checks;
            }
        }
        const uint64_t n = uint64_t(OPERAND_MASK) + 1;
        ASSERT_CHECK(checks == n*n, tb.cycle_count_,
                    "Ran " + std::to_string(checks) + " checks, expected "
                    + std::to_string(n*n));
        std::cout<< "[PASS] Random Test (" << checks << " products)\n";
    } // random test


}   // namespace testcases


//////////////////////////////////////////
//                MAIN                  //
//////////////////////////////////////////

int main(int argc, char* argv[]) {

    Testbench<Vbs_mult> tb(argc, argv,
                                tb_cfg::CLK_PERIOD_NS,
                                tb_cfg::TIMEOUT_CYCLES,
                                tb_cfg::TRACE_FILE
                               );

    std::cout << "[tb] Starting Simulation\n";
    std::cout << "[tb] TB_WIDTH = " << TB_WIDTH << "\n";
    tb.reset();

    try {
        // --- Test Scenarios ---
        // |--------------------|
        testcases::multiply_test(tb);
        testcases::exhaustive_test(tb);
        testcases::random_test(tb);
        // |--------------------|
        // --- Test Scenarios ---

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
