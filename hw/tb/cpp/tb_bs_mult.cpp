#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <iterator>

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
constexpr int32_t SMIN = -(1 << (TB_WIDTH - 1));
constexpr int32_t SMAX =  (1 << (TB_WIDTH - 1)) - 1;

inline int64_t sext(uint64_t v, unsigned bits) {      // interpret low bits as two's complement
    return int64_t(v << (64 - bits)) >> (64 - bits);
}

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
    MulResult mult(Testbench<Vbs_mult>& tb, int32_t a, int32_t b,
                       uint64_t max_wait = 100) {
        tb.dut()->a_i     = uint32_t(a) & OPERAND_MASK;
        tb.dut()->b_i     = uint32_t(b) & OPERAND_MASK;
        tb.dut()->start_i = 1;
        tb.tick();
        tb.dut()->start_i = 0;
        tb.dut()->b_i     = ~uint32_t(b) & OPERAND_MASK;

        uint64_t latency = 1;
        while (!tb.dut()->done_o) {
            ASSERT_CHECK(latency < max_wait, tb.cycle_count_,
                        "Timeout: done_o never asserted");
            tb.tick();
            ++latency;
        }
        return {tb.dut()->product_o, latency};
    } // MulResult mult


    // Check one multiply against hte reference model
    void check_result(Testbench<Vbs_mult>& tb, int32_t a, int32_t b, const MulResult& r) {
        const int64_t expected = int64_t(a) * int64_t(b);
        const std::string ops = "a=" + std::to_string(a) + " b=" + std::to_string(b);
        const int64_t actual = sext(r.product, 2*TB_WIDTH);

        ASSERT_CHECK(actual == expected, tb.cycle_count_,
                    ops + ": product " + std::to_string(actual) +
                    ", expected " + std::to_string(expected));
        ASSERT_CHECK(r.latency == TB_WIDTH + 1, tb.cycle_count_,
                    ops + ": latency " + std::to_string(r.latency) +
                    ", expected " + std::to_string(TB_WIDTH + 1));
    } // Check Result

} // namespace helpfunc


///////////////////////////////
// Stimulus & Checking Logic //
///////////////////////////////
namespace testcases {
    void multiply_test(Testbench<Vbs_mult>& tb) {
        std::cout << "[tb] Beginning test 1. 'Multiply_test'\n";
        tb.reset();

        const int32_t cases[][2] = {{-3, -5}, {-8, 5}};
        for (const auto& c : cases) {
            const MulResult r       = helpfunc::mult(tb, c[0], c[1]);
            helpfunc::check_result(tb, c[0], c[1], r);
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

        uint64_t bad[2][2] = {};
        uint64_t checks = 0;
        for (int32_t a = SMIN; a <= SMAX; ++a) {
            for (int32_t b = SMIN; b <= SMAX; b++) {
                const MulResult r = helpfunc::mult(tb, a, b);
                if (sext(r.product, 2 * TB_WIDTH) != int64_t(a) * b) {
                    ++bad[a < 0][b < 0];
                }
                ++checks;
                const std::string ops = "a=" + std::to_string(a) + " b=" + std::to_string(b);
                ASSERT_CHECK(r.latency == TB_WIDTH + 1, tb.cycle_count_,
                    ops + ": latency " + std::to_string(r.latency) +
                    ", expected " + std::to_string(TB_WIDTH + 1));
            }
        }
        std::cout << "[tb] mismatches  a+b+ " << bad[0][0] << "  a+b- " << bad[0][1]
                << "  a-b+ " << bad[1][0] << "  a-b- " << bad[1][1] << "\n";
        ASSERT_CHECK(bad[0][0] + bad[0][1] + bad[1][0] + bad[1][1] == 0, tb.cycle_count_,
                    "Signed exhaustive test has mismatches (see quadrant table)");

        const uint64_t n = uint64_t(OPERAND_MASK) + 1;
        ASSERT_CHECK(checks == n*n, tb.cycle_count_,
                    "Ran " + std::to_string(checks) + " checks, expected "
                    + std::to_string(n*n));
        std::cout<< "[PASS] Exhaustive test (" << checks << " products)\n";
    } // exhaustive test



    void random_test(Testbench<Vbs_mult>& tb) {
        std::cout << "[tb] Beginning Test 3. Random + Corner Cases\n";
        tb.reset();

        // Corner Checks
        uint64_t checks = 0;
        const int32_t corners[] = {
            SMIN,
            SMIN + 1,
            -1,
            0,
            1,
            SMAX,
            int32_t(sext(0x55555555u & OPERAND_MASK, TB_WIDTH)),
            int32_t(sext(0xAAAAAAAAu & OPERAND_MASK, TB_WIDTH)),
        };

        for (const uint32_t a : corners) {
            for (const int32_t b : corners) {
                const MulResult r        = helpfunc::mult(tb, a, b);
                helpfunc::check_result(tb, a, b, r);
                ++checks;
            }
        }

        const uint64_t expected_checks = std::size(corners) * std::size(corners);
        ASSERT_CHECK(checks == expected_checks, tb.cycle_count_,
                    "Ran " + std::to_string(checks) + " checks, expected "
                    + std::to_string(expected_checks));


        // Random Checks
        constexpr uint32_t SEED         = 12345;
        constexpr uint64_t NUM_RANDOM   = 10000;
        std::cout << "[tb] Random seed = " << SEED << std::endl;

        std::mt19937 rng(SEED);
        std::uniform_int_distribution<int32_t> dist(SMIN, SMAX);

        uint64_t random_checks = 0;
        for (uint64_t i = 0; i < NUM_RANDOM; ++i) {
            const int32_t a = dist(rng);
            const int32_t b = dist(rng);
            helpfunc::check_result(tb, a, b, helpfunc::mult(tb, a, b));
            ++random_checks;
        }
        ASSERT_CHECK(random_checks == NUM_RANDOM, tb.cycle_count_,
                    "Ran " + std::to_string(random_checks) + " random checks, expected"
                    + std::to_string(NUM_RANDOM));

        // Random & Corner Pass statement
        std::cout << "[PASS] Test 3. Random Test (" << checks <<  " corner + "
                  << random_checks << " random products)\n";
    } // random test


    void start_while_busy_test(Testbench<Vbs_mult>& tb) {
        std::cout << "[tb] Beginning Test 4. 'Start While Busy'\n";
        tb.reset();
        const int32_t a = 5;
        const int32_t b = -3;

        uint64_t latency  = 0;
        uint64_t max_wait = 500;
        int64_t expected = int64_t(a) * b;

        tb.dut()->a_i       = a & OPERAND_MASK;
        tb.dut()->b_i       = b & OPERAND_MASK;
        tb.dut()->start_i   = 1;
        tb.tick();          ++latency;
        tb.dut()->start_i   = 0;
        tb.tick();          ++latency;
        tb.tick();          ++latency;
        tb.dut()->b_i       = (b+1) & OPERAND_MASK;
        tb.dut()->start_i   = 1;
        tb.tick();          ++latency;
        tb.dut()->start_i   = 0;

        while (!tb.dut()->done_o) {
            ASSERT_CHECK(latency < max_wait, tb.cycle_count_,
                        "Timeout: done_o never asserted");
            tb.tick();
            ++latency;
        }

        uint64_t product = tb.dut()->product_o;

        ASSERT_CHECK(sext(product, 2*TB_WIDTH) == expected, tb.cycle_count_,
                    "a=" + std::to_string(a) + " b=" + std::to_string(b) +
                    ": Output " + std::to_string(product) +
                    ", expected " + std::to_string(expected));
        ASSERT_CHECK(latency == TB_WIDTH+1, tb.cycle_count_,
                    "Latency: " + std::to_string(latency) +
                    " | Expected: " + std::to_string(TB_WIDTH+1));
        for (int i = 0; i < 2; ++i) {
            tb.tick();
            ASSERT_CHECK(!tb.dut()->busy_o, tb.cycle_count_,
                        "`busy_o` re-asserted after done");
        }

        std::cout << "[PASS] Test 4. Complete\n";
    } // start_while_busy_test

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
        testcases::start_while_busy_test(tb);
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
        std::cout << "[PASS] All Test Cases Passed!\n\n";
    } else {
        std::cerr << "[FAIL] " << tb.fail_count_ << " check(s) failed" << std::endl;
    }
    return tb.fail_count_ == 0 ? 0 : 1;
}
