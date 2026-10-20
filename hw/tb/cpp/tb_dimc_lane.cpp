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
std::mt19937 rng(cfg::SEED);

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
/* 1. Hand-Computed Steps
            Stimulus: Values that are exact in Q3.13 (raw = real * 8192), so no rounding
                      A = 0.75 (6144), B = 0.5 (4096), C = -1.25 (-10240)
                      a) h0 = +1.0, x = -0.5  ->  h = +0.5 (4096),  yp = -0.625 * 2^26
                      b) h0 = -1.0, x = +0.5  ->  h = -0.5 (-4096), yp = +0.625 * 2^26
            Check: DUT matches the paper values, and so does ref::lane_step
                   Case b has A*h < 0, so the accumulator load must sign-extend */
    bool hand_computed_test(TB& tb) {
        std::cout << "[tb] Beginning Test 1. 'Hand-Computed Steps'\n";
        struct Case { const char* name; int32_t A, B, C, h0, x, h; int64_t yp; };
        const Case cases[] = {
            {"a (Ah>0, Bx<0)", 6144, 4096, -10240,  8192, -4096,  4096, -41943040},
            {"b (Ah<0, Bx>0)", 6144, 4096, -10240, -8192,  4096, -4096,  41943040},
        };

        Check h_chk{"Test 1 h vs paper"}, yp_chk{"Test 1 yp vs paper"}, ref_chk{"Test 1 ref vs paper"};
        for (const auto& c : cases) {
            load(tb, c.A, c.B, c.C, c.h0);
            const StepOut     got = step(tb, c.x);
            const ref::LaneOut exp = ref::lane_step(c.A, c.B, c.C, c.h0, c.x);
            h_chk.expect(got.h,    c.h,  c.name);
            yp_chk.expect(got.yp,  c.yp, c.name);
            ref_chk.expect(exp.h,  c.h,  c.name);    // the oracle must agree with the paper too
            ref_chk.expect(exp.yp, c.yp, c.name);
        }

        bool ok = h_chk.report(2);
        ok &= yp_chk.report(2);
        ok &= ref_chk.report(4);
        return ok;
    }


/* 2. Directed Steps
            Stimulus: Single steps aimed at specific arithmetic corners, then a 20-step
                      chain where the DUT carries h itself
            Check: h and yp match ref::lane_step on every step */
    bool directed_test(TB& tb) {
        std::cout << "[tb] Beginning Test 2. 'Directed Steps'\n";
        struct Case { const char* name; int32_t A, B, C, h0, x; };
        const Case cases[] = {
            // Rounding ties: acc = (2k+1) * 2^12, exactly halfway between two outputs
            {"tie +0.5 -> +1",           1,     0,   8192,   4096,      0},
            {"tie -0.5 -> 0",            1,     0,   8192,  -4096,      0},
            {"tie +1.5 -> +2",           3,     0,   8192,   4096,      0},
            {"tie -1.5 -> -1",           3,     0,   8192,  -4096,      0},
            {"just below +0.5 -> 0",     1,     0,   8192,   4095,      0},
            // |product| >= 2^15: bit 15 is set but is not the sign bit
            {"product +33000",           3,     0,   8192,  11000,      0},
            {"product -33000",          -3,     0,   8192,  11000,      0},
            // All four sign combinations of (A*h, B*x)
            {"Ah<0 Bx>0",             7000,  3000,  -3000,  -9000,  12000},
            {"Ah>0 Bx<0",            -7000,  3000,  -3000,  -9000, -12000},
            {"Ah<0 Bx<0",            -7000, -3000,   5000,   9000,  12000},
            {"Ah>0 Bx>0",             7000,  3000,   5000,   9000,  12000},
            // Zero and extreme operands
            {"all-zero coefficients",    0,     0,      0,   1234,  -1234},
            {"A = -4.0 (QMIN)",     -32768,     0,   8192,   1000,      0},
            {"x path only",              0,  8192,   8192,      0, -20000},
            {"C = QMIN, h = QMAX",    8192,     0, -32768,  32767,      0},
            {"C = QMAX, h = QMIN",    8192,     0,  32767, -32768,      0},
        };
        constexpr size_t N_CASES = sizeof(cases) / sizeof(cases[0]);

        Check h_chk{"Test 2 h"}, yp_chk{"Test 2 yp"};
        for (const auto& c : cases) {
            load(tb, c.A, c.B, c.C, c.h0);
            compare(h_chk, yp_chk, step(tb, c.x), ref::lane_step(c.A, c.B, c.C, c.h0, c.x), c.name);
        }

        // Chain: the reference carries its own h, never copied from the DUT
        const int32_t A = 7987, B = 205, C = -3000;     // ~0.975, ~0.025, ~-0.366
        const int32_t xs[] = {32767, 32767, -32768, 0, 12345, -12345, 1, -1, 30000, -30000,
                              0, 0, 0, 16384, -16384, 8192, -8192, 100, -100, 0};
        constexpr size_t N_CHAIN = sizeof(xs) / sizeof(xs[0]);

        load(tb, A, B, C, 0);
        int32_t h_ref = 0;
        for (size_t k = 0; k < N_CHAIN; ++k) {
            const ref::LaneOut exp = ref::lane_step(A, B, C, h_ref, xs[k]);
            compare(h_chk, yp_chk, step(tb, xs[k]), exp, "chain k=" + std::to_string(k));
            h_ref = exp.h;
        }

        bool ok = h_chk.report(N_CASES + N_CHAIN);
        ok &= yp_chk.report(N_CASES + N_CHAIN);
        return ok;
    }


/* 3. Golden Channels
            Stimulus: Each channel's A/B/C from the Python model, h0 = 0, all 3600 x samples
            Check: per step,
                     h  vs ref::lane_step and vs the golden h_trace
                     yp vs ref::lane_step and vs C * (DUT's own h)
                   The last check localises bugs: if it passes while the rest fail,
                   the fault is in the h path, not pass 2 */
    bool golden_test(TB& tb, const Golden& g, size_t first, size_t last, const std::string& name) {
        std::cout << "[tb] Beginning Test 3. 'Golden " << name << "'\n";
        Check h_ref{name + " h vs ref"},  h_gold{name + " h vs golden"};
        Check yp_ref{name + " yp vs ref"}, yp_id{name + " yp == C*h"};

        for (size_t c = first; c <= last; ++c) {
            load(tb, g.A.at(c), g.B.at(c), g.C.at(c), 0);
            int32_t h_prev = 0;
            for (size_t k = 0; k < cfg::T; ++k) {
                const std::string  where = "c=" + std::to_string(c) + " k=" + std::to_string(k);
                const StepOut      got   = step(tb, g.x.at(k));
                const ref::LaneOut exp   = ref::lane_step(g.A.at(c), g.B.at(c),
                                                          g.C.at(c), h_prev, g.x.at(c));
                h_ref.expect(got.h,   exp.h,                        where);
                h_gold.expect(got.h,  g.h_trace.at(k * cfg::N + c), where);
                yp_ref.expect(got.yp, exp.yp,                       where);
                yp_id.expect(got.yp,  int64_t(g.C.at(c)) * got.h,   where);
                h_prev = exp.h;
            }
        }

        const uint64_t n = (last - first + 1) * cfg::T;
        bool ok = h_ref.report(n);
        ok &= h_gold.report(n);
        ok &= yp_ref.report(n);
        ok &= yp_id.report(n);
        return ok;
    }


/* 4. Saturation
            The golden data never reaches a rail (ref_check: 0 rail hits), so drive it there
            Stimulus: a) A = B = 1.0: climb to +rail, fall to -rail, then decay back (A = 0.5)
                      b) |acc| = 2^31, the largest the 33-bit accumulator ever holds
                      c) Rounding exactly at the rail boundary (32767.5 and -32768.5 LSB)
                      d) Overflow sweep: h0 on a rail, x swept across its whole range, so
                         overflowed sums occur with bit 15 both 0 and 1
            Check: h and yp match ref::lane_step; both rails hit >= 50 times */
    bool saturation_test(TB& tb) {
        std::cout << "[tb] Beginning Test 4. 'Saturation'\n";
        Check    h_chk{"Test 4 h"}, yp_chk{"Test 4 yp"};
        Coverage cov;

        // One step, chaining h_ref through the reference
        auto run = [&](int32_t A, int32_t B, int32_t C, int32_t& h_ref, int32_t x, const std::string& where) {
            const ref::LaneOut exp = ref::lane_step(A, B, C, h_ref, x);
            compare(h_chk, yp_chk, step(tb, x), exp, where);
            cov.add(exp.h);
            h_ref = exp.h;
        };

        // a) Climb, fall, recover. h is NOT rewritten before recovery
        int32_t h = 0;
        load(tb, 8192, 8192, 8192, 0);
        for (int i = 0; i < 3; ++i)  run(8192, 8192, 8192, h, cfg::QMAX, "a climb " + std::to_string(i));
        for (int i = 0; i < 4; ++i)  run(8192, 8192, 8192, h, cfg::QMIN, "a fall "  + std::to_string(i));
        write(tb, SEL_A, 4096);
        write(tb, SEL_B, 0);
        for (int i = 0; i < 12; ++i) run(4096, 0, 8192, h, 0, "a recover " + std::to_string(i));

        // b) Largest accumulator magnitude: (-4)(-4) + (-4)(-4) = +2^31, and the negative side
        h = cfg::QMIN;
        load(tb, cfg::QMIN, cfg::QMIN, 8192, h);
        run(cfg::QMIN, cfg::QMIN, 8192, h, cfg::QMIN, "b acc = +2^31");
        h = cfg::QMAX;
        load(tb, cfg::QMIN, cfg::QMIN, 8192, h);
        run(cfg::QMIN, cfg::QMIN, 8192, h, cfg::QMAX, "b acc ~ -2^31");

        // c) Rail boundary: x = 4096 rounds up past QMAX, x = 4095 just stays in range
        for (int32_t x : {4096, 4095}) {
            h = cfg::QMAX;
            load(tb, 8192, 1, 8192, h);
            run(8192, 1, 8192, h, x, "c +boundary x=" + std::to_string(x));
        }
        for (int32_t x : {-4096, -4097}) {
            h = cfg::QMIN;
            load(tb, 8192, 1, 8192, h);
            run(8192, 1, 8192, h, x, "c -boundary x=" + std::to_string(x));
        }

        // d) Overflow sweep. A rail chosen from bit 15 instead of the true MSB fails here
        for (int32_t h0 : {cfg::QMAX, cfg::QMIN}) {
            for (int32_t x = cfg::QMIN; x <= cfg::QMAX; x += 257) {
                h = h0;
                load(tb, 8192, 8192, cfg::QMIN, h);
                run(8192, 8192, cfg::QMIN, h, x, "d h0=" + std::to_string(h0) + " x=" + std::to_string(x));
            }
        }

        // a) 19 + b) 2 + c) 4 + d) 2 x 256 = 537
        constexpr uint64_t N_STEPS = 19 + 2 + 4 + 2 * 256;
        bool ok = h_chk.report(N_STEPS);
        ok &= yp_chk.report(N_STEPS);
        ok &= cov.report("Test 4", 50);
        return ok;
    }


/* 5. Random Steps
            Stimulus: 10k single steps, seed cfg::SEED
                      odd i:  any 16-bit values (mostly saturates)
                      even i: |A|,|B| <= 1.0 and |h|,|x| <= 2.0 (mostly in range)
            Check: h and yp match ref::lane_step; each output class hit >= 500 times */
    bool random_test(TB& tb) {
        constexpr int N_STEPS = 10000;
        std::cout << "[tb] Beginning Test 5. 'Random Steps' (seed " << cfg::SEED << ")\n";
        std::uniform_int_distribution<int32_t> any(cfg::QMIN, cfg::QMAX);
        std::uniform_int_distribution<int32_t> coeff(-8192, 8192);
        std::uniform_int_distribution<int32_t> data(-16384, 16383);

        Check    h_chk{"Test 5 h"}, yp_chk{"Test 5 yp"};
        Coverage cov;
        for (int i = 0; i < N_STEPS; ++i) {
            const bool    wide = (i & 1);
            const int32_t A  = wide ? any(rng) : coeff(rng);
            const int32_t B  = wide ? any(rng) : coeff(rng);
            const int32_t C  = any(rng);
            const int32_t h0 = wide ? any(rng) : data(rng);
            const int32_t x  = wide ? any(rng) : data(rng);

            load(tb, A, B, C, h0);
            const ref::LaneOut exp = ref::lane_step(A, B, C, h0, x);
            compare(h_chk, yp_chk, step(tb, x), exp, "i=" + std::to_string(i));
            cov.add(exp.h);
        }

        bool ok = h_chk.report(N_STEPS);
        ok &= yp_chk.report(N_STEPS);
        ok &= cov.report("Test 5", 500);
        return ok;
    }


/* 6. Handshake
            Every step() already checks latency and in_ready_o. This test covers the rest:
            a) out_valid_o is a one-cycle pulse; outputs and in_ready_o hold while idle
            b) Random valid/x/writes on every busy cycle are ignored (writes only in IDLE)
            c) A write and an accept on the same edge are legal; the step uses the new value
            Check: results match ref::lane_step and no protocol violations */
    bool handshake_test(TB& tb) {
        std::cout << "[tb] Beginning Test 6. 'Handshake'\n";
        Check          h_chk{"Test 6 h"}, yp_chk{"Test 6 yp"};
        const uint64_t errors_before = monitor.errors;

        // a) One step, then 100 idle cycles
        load(tb, 6144, 4096, -10240, 8192);
        const StepOut got = step(tb, -4096);
        compare(h_chk, yp_chk, got, ref::lane_step(6144, 4096, -10240, 8192, -4096), "a");
        for (int i = 0; i < 100; ++i) {
            tb.tick();
            if (tb.dut()->out_valid_o)           monitor.fail(tb, "out_valid_o high while idle");
            if (!tb.dut()->in_ready_o)           monitor.fail(tb, "in_ready_o low while idle");
            if (sext16(tb.dut()->h_o)  != got.h)  monitor.fail(tb, "h_o changed while idle");
            if (sext32(tb.dut()->yp_o) != got.yp) monitor.fail(tb, "yp_o changed while idle");
        }

        // b) 50 steps with interference, then one clean step to prove the coefficients survived
        const int32_t A = 7987, B = 205, C = -3000;
        load(tb, A, B, C, 0);
        int32_t h_ref = 0;
        std::uniform_int_distribution<int32_t> any(cfg::QMIN, cfg::QMAX);
        for (int k = 0; k <= 50; ++k) {
            const bool         spam = (k < 50);
            const int32_t      x    = spam ? any(rng) : 1000;
            const ref::LaneOut exp  = ref::lane_step(A, B, C, h_ref, x);
            compare(h_chk, yp_chk, step(tb, x, spam), exp, "b k=" + std::to_string(k));
            h_ref = exp.h;
        }

        // c) Write A = 0.5 on the same edge as the accept (step() ticks before clearing wr_*)
        load(tb, 8192, 8192, 8192, 1000);
        tb.dut()->wr_en_i   = 1;
        tb.dut()->wr_sel_i  = SEL_A;
        tb.dut()->wr_data_i = 4096;
        compare(h_chk, yp_chk, step(tb, 0), ref::lane_step(4096, 8192, 8192, 1000, 0), "c new A used");

        bool ok = h_chk.report(53);
        ok &= yp_chk.report(53);
        const uint64_t new_errors = monitor.errors - errors_before;
        std::cout << (new_errors == 0 ? "[PASS] " : "[FAIL] ") << "Test 6 protocol: "
                  << new_errors << " violations\n";
        return ok && (new_errors == 0);
    }


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
        Golden g;
        g.x       = vec::load_sized(cfg::DIR + "x_seq_q.txt",   cfg::T);
        g.h_trace = vec::load_sized(cfg::DIR + "h_trace_q.txt", cfg::T * cfg::N);
        g.A       = vec::load_sized(cfg::DIR + "A_bar_q.txt",   cfg::N);
        g.B       = vec::load_sized(cfg::DIR + "B_bar_q.txt",   cfg::N);
        g.C       = vec::load_sized(cfg::DIR + "C_bar_q.txt",   cfg::N);

        // --- Test Scenarios ---
        // |--------------------|
        ok &= testcases::hand_computed_test(tb);
        ok &= testcases::directed_test(tb);
        ok &= testcases::golden_test(tb, g, 0, 0,          "channel 0");
        ok &= testcases::golden_test(tb, g, 1, cfg::N - 1, "channels 1-7");
        ok &= testcases::saturation_test(tb);
        ok &= testcases::random_test(tb);
        ok &= testcases::handshake_test(tb);
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
              << (ok ? "[PASS] All Test Cases Passed!\n\n" : "[FAIL] See errors above\n\n");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
