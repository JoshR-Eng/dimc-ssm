#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "ref/testbench.hpp"
#include "ref/lane_ref.hpp"
#include "ref/vectors.hpp"
#include "Vdimc_lane.h"

#ifndef TB_DATA_W
#define TB_DATA_W 16
#endif
static_assert(TB_DATA_W == 16, "ref::lane_step models Q3.13 (16-bit): run dimc_lane at DATA_W=16");

///////////////////
// Configuration //
///////////////////
namespace cfg {
    constexpr unsigned W        = TB_DATA_W;
    // Latency from the design spec: 3 passes x (START + W shift cycles + done) + accept + COMMIT
    constexpr uint64_t LATENCY  = 3 * (W + 2) + 2;          // = 3W + 8 = 56 at W = 16
    constexpr uint64_t WATCHDOG = 4 * LATENCY;              // per-step timeout
    constexpr size_t   T        = 3600;                     // timesteps in golden data
    constexpr size_t   N        = 8;                        // channels in golden data
    const std::string  DIR      = "model/vectors/";
    constexpr int32_t  QMIN     = -(1 << (W - 1));
    constexpr int32_t  QMAX     =  (1 << (W - 1)) - 1;
    constexpr uint32_t SEED     = 12345;

    constexpr double      CLK_PERIOD_NS = 10.0;
    constexpr uint64_t    MAX_CYCLES    = 10'000'000;
    constexpr const char* TRACE_FILE    = "waveform.fst";
}

using TB = Testbench<Vdimc_lane>;
std::mt19937 rng(cfg::SEED)

inline int32_t sext16(uint32_t v) { return int32_t(int16_t(uint16_t(v))); }
inline int64_t sext32(uint32_t v) { return int64_t(int32_t(v)); }




//////////////////////
// Helper Functions //
//////////////////////

// Value Checker
struct Check {
    std::string name;
    uint64_t    compared   = 0;
    uint64_t    mismatches = 0;
    static constexpr uint64_t MAX_PRINT = 5;

    void expect(int64_t got, int64_t exp, const std::string& where) {
        ++compared;
        if (got != exp) {
            if (mismatches < MAX_PRINT)
                std::cerr << "[ERROR] " << name << " " << where
                          << ": got " << got << ", expected " << exp << "\n";
            ++mismatches;
        }
    }

    bool report(uint64_t expected_count) const {
        const bool ok = (mismatches == 0) && (compared == expected_count);
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << ": "
                  << mismatches << " mismatches / " << compared << " compared"
                  << " (expected " << expected_count << ")\n";
        return ok;
    }
};


// Per-cycle data flow monitor
struct Monitor {
    uint64_t errors  = 0;
    uint64_t steps   = 0;
    uint64_t lat_min = UINT64_MAX;
    uint64_t lat_max = 0;
    static constexpr uint64_t MAX_PRINT = 10;

    void fail(const TB& tb, const std::string& msg) {
        if (errors < MAX_PRINT)
            std::cerr << "[FAIL] @cycle " << tb.cycle_count_ << ": " << msg << "\n";
        ++errors;
    }
} monitor;


//////////////////
// Port Drivers //
//////////////////

// Selects which register a write goes to - drives `wr_sel_i`
enum Sel : uint8_t {
    SEL_A = 0,
    SEL_B = 1,
    SEL_C = 2,
    SEL_H = 3
};

// Drive every input low for clean reset
void clear_inputs(TB& tb) {
    tb.dut()->in_valid_i = 0;
    tb.dut()->x_i        = 0;
    tb.dut()->wr_en_i    = 0;
    tb.dut()->wr_sel_i   = 0;
    tb.dut()->wr_data_i  = 0;
}


// Function to write value into selected register -- Only legal while IDLE
void write(TB& tb, Sel sel, int32_t value) {
    if (!tb.dut()->in_ready_o) monitor.fail(tb, "Write attempted while lane not idle");
    tb.dut()->wr_en_i   = 1;
    tb.dut()->wr_sel_i  = sel;
    tb.dut()->wr_data_i = uint16_t(value);
    tb.tick();
    tb.dut()->wr_en_i   = 0;
}

// Load coeffs and initial state into the lane
void load(TB& tb, int32_t A, int32_t B, int32_t C, int32_t h0) {
    write(tb, SEL_A, A);
    write(tb, SEL_B, B);
    write(tb, SEL_C, C);
    write(tb, SEL_H, h0);
}


// Increment DIMC by one timestep
    // Results
struct StepOut {
    int32_t h;  // new state h_k (Q3.13)
    int64_t yp; // Output C*h_k  (Q.26)
};
    // Function to run one timestep & check handshake + latency on each cycle
    //      spam_while_busy: drive random valid/x/write on every busy cycle (should be ignored)
StepOut step(TB& tb, int32_t x, bool spam_while_busy = false) {
    if (!tb.dut()->in_ready_o) monitor.fail(tb, "Step offered while lane not idle");

    // --- Cycle 0: accept
    tb.dut()->in_valid_i = 1;
    tb.dut()->x_i        = uint16_t(x);
    tb.tick();
    clear_inputs(tb);

    // --- Busy cycles: wait for out_valid_o
    uint64_t latency = 1;
    while (!tb.dut()->out_valid_o) {
        if (tb.dut()->in_ready_o) monitor.fail(tb, "in_ready_o high while a step is in flight");
        if (latency >= cfg::WATCHDOG) {
            monitor.fail(tb, "Watchdog: out_valid_o never asserted");
            throw std::runtime_error("step watchdog expired");
        }
        if (spam_while_busy) {
            tb.dut()->in_valid_i = 1;
            tb.dut()->x_i        = uint16_t(rng());
            tb.dut()->wr_en_i    = 1;
            tb.dut()->wr_sel_i   = rng() & 3;
            tb.dut()->wr_data_i  = uint16_t(rng());
        }
        tb.tick();
        ++latency;
    }
    clear_inputs(tb);

    // --- Result cycle: outputs valid, lane idle again
    if (!tb.dut()->in_ready_o) monitor.fail(tb, "in_ready_o low on the out_valid_o cycle");
    if (latency != cfg::LATENCY)
        monitor.fail(tb, "Latency " + std::to_string(latency) +
                         ", expected " + std::to_string(cfg::LATENCY));
    ++monitor.steps;
    monitor.lat_min = std::min(monitor.lat_min, latency);
    monitor.lat_max = std::max(monitor.lat_max, latency);

    return {sext16(tb.dut()->h_o), sext32(tb.dut()->yp_o)};
} // step


// Scoreboard function to validate that results match expected
void compare(Check& h_chk, Check& yp_chk, const StepOut& got,
             const ref::LaneOut& exp, const std::string& where) {
    h_chk.expect(got.h,   exp.h, where);
    yp_chk.expect(got.yp, exp.yp, where);
}

// struct to prove a test reaches the up limits/rails
struct Coverage {
    uint64_t rail_hi  = 0;
    uint64_t rail_lo  = 0;
    uint64_t in_range = 0;

    void add(int32_t h) {
        if      (h == cfg::QMAX) ++rail_hi;
        else if (h == cfg::QMIN) ++rail_lo;
        else                     ++in_range;
    }

    bool report(const std::string& name, uint64_t min_each) const {
    const bool ok = (rail_hi >= min_each) && (rail_lo >= min_each) && (in_range >= min_each);
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << " coverage: +rail " << rail_hi
                << ", -rail " << rail_lo << ", in-range " << in_range
                << " (need >= " << min_each << " each)\n";
    return ok;
    }
};

// Golden vectors exported by the python model (model/ssm_model/s4_tests.py)
struct Golden {
    std::vector<int32_t> x, h_trace, A, B, C;
};


///////////////
// Testcases //
///////////////
namespace testcases {

} // testcases


////////////////////////////
//          MAIN          //
////////////////////////////

int main(int argc, char* argv[]) {
    TB tb(argc, argv,
          cfg::CLK_PERIOD_NS,
          cfg::MAX_CYCLES,
          cfg::TRACE_FILE
    );

    std::cout << "[tb] Starting Simulation: dimc_lane W=" << cfg::W
              << ", latency " << cfg::LATENCY << " cycles\n";
    clear_inputs(tb);   // before reset, see clear_inputs()
    tb.reset();

    bool ok = true;
    try {
        // --- Test Scenarios ---
        // |--------------------|

        // |--------------------|
        // --- Test Scenarios ---
    } catch (const std::exception& e) {
        std::cerr << "[tb] Simulation halted: " << e.what() << "\n";
        ok = false;
    }

    std::cout << "[tb] " << monitor.steps << " steps, latency min/max "
              << (monitor.steps ? monitor.lat_min : 0) << "/" << monitor.lat_max
              << ", " << monitor.errors << " protocol violations\n";
    ok &= (monitor.errors == 0);

    std::cout << "[tb] Simulation Finished after " << tb.cycle_count_ << " cycles\n"
              << (ok ? "[PASS] All Test Cases Passed!\n" : "[FAIL] See errors above\n");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
