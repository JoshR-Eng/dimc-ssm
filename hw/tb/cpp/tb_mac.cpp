#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "testbench.hpp"    // Testbench Template
#include "top_mac_cvxif.h"  // Top Hardware module

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
    // Append functions here that run complete scenarions...
}


//////////////////////////////////////////
//                MAIN                  //
//////////////////////////////////////////

int main(int argc, char* argv[]) {

    Testbench<top_mac_cvxif> tb(argc, argv,
                                tb_cfg::CLK_PERIOD_NS,
                                tb_cfg::TIMEOUT_CYCLES,
                                tb_cfg::TRACE_FILE
                               );

    std::cout << "[tb] Starting Simulation\n";
    tb.reset();

    // --- Test Scenarios ---
    // |--------------------|
    // --- Test Scenarios ---

    while (!tb.finished()) {
        tb.tick();
    }

    const bool timed_out = tb.cycle_count_() >= tb_cfg::TIMEOUT_CYCLES;
    std::cout << "[tb] Simulation Finished after " << tb.cycle_count_
          << " cycles" << (timed_out ? " (TIMEOUT)" : "") << "\n";

    if (tb.fail_count_ == 0) {
        std::cout << "[PASS] All Test Cases Passed!" << std::endl
    } else {
        std::cerr << "[FAIL] " << tb.fail_count_ << " check(s) failed" << std::endl;
    }
    return tb.fail_count_ == 0 ? 0 : 1;
}
