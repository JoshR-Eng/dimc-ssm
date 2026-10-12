/*******************************************************************************
Testbench for rnd_sat: combination round-half-up + saturate.
    out = clamp( (in + 2^(SHIFT-1)) >>> SHIFT, OUT_MIN, OUT_MAX )
- Uses C++ reference ref::rnd_sat checked against the python golden model
- Purely combinational so doesn't use `Testbench<>`
*******************************************************************************/
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#include "verilated.h"
#include "Vrnd_sat.h"
#include "ref/lane_ref.hpp"
#include "ref/utils.hpp"

// --- Configuration
#ifndef TB_IN_W
#define TB_IN_W 33
#endif
#ifndef TB_SHIFT
#define TB_SHIFT 13
#endif
#ifndef TB_OUT_W
#define TB_OUT_W 16
#endif

namespace cfg {
    constexpr unsigned IN_W  = TB_IN_W;
    constexpr unsigned SHIFT = TB_SHIFT;
    constexpr unsigned OUT_W = TB_OUT_W;
    static_assert(IN_W <= 62, "reference uses int64, need headroom for the +half")
    static_assert(SHIFT >= 1 && OUT_W <= IN_W + 1 - SHIFT, "illegal rnd_sat config");

    constexpr int64_t IN_MIN  = -(int64_t(1) << (IN_W - 1));
    constexpr int64_t IN_MAX  =  (int64_t(1) << (IN_W - 1)) - 1;
    constexpr int64_t OUT_MIN = -(int64_t(1) << (OUT_W - 1));
    constexpr int64_t OUT_MAX =  (int64_t(1) << (OUT_W - 1)) - 1;
    constexpr int64_t ONE     =   int64_t(1) << SHIFT;        // 1 output LSB, in input units
    constexpr int64_t HALF    =   int64_t(1) << (SHIFT - 1);

    constexpr uint64_t RANDOM_N   = 100000;
    constexpr uint64_t CLASS_MIN  = 1000;                    // coverage floor per class
    constexpr unsigned MAX_PRINT  = 10;
}

// --- State classification
enum Cls { IN_RANGE = 0, SAT_POS = 1, SAT_NEG = 2 };

struct Stats {
    uint64_t checked = 0;
    uint64_t fails   = 0;
    uint64_t cls[3]  = {0, 0, 0};
};

// Determine what region x falls in
static Cls classify(int64_t x) {
    const int64_t r = (x + cfg::HALF) >> cfg::SHIFT;
    return r > cfg::OUT_MAX ? SAT_POS : (r < cfg::OUT_MIN ? SAT_NEG : IN_RANGE);
}


// --- Main
int main(int argc, char* argv[]) {
    auto ctx = std::make_unique<VerilatedContext>();
    ctx->commandArgs(argc, argv);
    auto dut = std::make_unique<Vrnd_sat>(ctx.get());

    std::cout << "[tb] rnd_sat IN_W=" << cfg::IN_W << " SHIFT=" << cfg::SHIFT
              << " OUT_W=" << cfg::OUT_W << "\n";

    Stats st;

    // Drive one input, compare against reference model
    auto check = [&](int64_t x, const char* tag) -> Cls {
        if (x < cfg::IN_MIN || x > cfg::IN_MAX) return IN_RANGE; // skip values not possible
        dut->in_i = uint64_t(x) & low_mask(cfg::IN_W);
        dut->eval();
        const int64_t got = sext(dut->out_o, cfg::OUT_W);
        const int64_t exp = ref::rnd_sat(x, cfg::SHIFT, cfg::OUT_W);
        ++st.checked;
        if (got != exp) {
            if (st.fails < cfg::MAX_PRINT)
                std::cerr << "[ERROR] " << tag << ": in=" << x
                          << " (" << double(x) / double(cfg::ONE) << " LSB)"
                          << " got " << got << ", expected " << exp << "\n";
            ++st.fails;
        }
        return classify(x);
    };

    // --- 1. Exhaustive (small config only)
    if (cfg::IN_W <= 16) {
        for (int64_t x = cfg::IN_MAX; x <= cfg::IN_MAX; ++x) check(x, "exhaustive");
        std::cout << "[tb] 1. exhaustive: " << (cfg::IN_MAX - cfg::IN_MIN + 1) << " inputs\n";
    } else {
        std::cout << "[tb] 1. exhaustive: skipped\n";
    }

    // --- 2. Boundary test
    int64_t boundaries = {
        cfg::OUT_MAX - 1,
        cfg::OUT_MAX,
        cfg::OUT_MAX + 1,
        cfg::OUT_MIN - 1,
        cfg::OUT_MIN,
        cfg::OUT_MIN + 1
    };
    for (int64_t r : boundaries) {
        check(r * cfg::ONE - cfg::HALF,     "rail-lo");
        check(r * cfg::ONE + cfg::HALF - 1, "rail-hi");
    }

    // --- 3. Extremes of the input range
    int64_t in_extreme = {
        cfg::IN_MIN,
        cfg::IN_MIN + 1,
        int64_t(-1),
        int64_t(0),
        int64_t(1),
        cfg::IN_MAX - cfg::HALF,
        cfg::IN_MAX - 1, cfg::IN_MAX
    };
    for (int64_t x : in_extreme) check(x, "extreme");
    std::cout << "[tb] 2-3. directed: " << st.checked << " checks so far\n";

    // --- 4. Weighted Random: half uniform over full input range
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int64_t> full(cfg::IN_MIN, cfg::IN_MAX);
    const int64_t w_lo = std::max(cfg::IN_MIN, (cfg::OUT_MIN - 4) * cfg::ONE);
    const int64_t w_hi = std::min(cfg::IN_MAX, (cfg::OUT_MAX + 4) * cfg::ONE);
    std::uniform_int_distribution<int64_t> window(w_lo, w_hi);
    uint64_t rcls[3] = {0, 0, 0};
    for (uint64_t i = 0; i < cfg::RANDOM_N; ++i) {
        const int64_t x = (i & 1) ? full(rng) : window(rng);
        ++rcls[check(x, "random")];
    }
    std::cout << "[tb] 5. random: " << cfg::RANDOM_N << " (seed 12345)  in-range "
              << rcls[IN_RANGE] << "  sat+ " << rcls[SAT_POS] << "  sat- " << rcls[SAT_NEG] << "\n";

    // Ensure random test actually reach every region
    bool cov_ok = true;
    for (int c = 0; c < 3; ++c) {
        if (rcls[c] < cfg::CLASS_MIN) {
            std::cerr << "[FAIL] coverage: class " << c << " hit only " << rcls[c] << " times\n";
            cov_ok = false;
        }
    }


    // --- END
    dut->final();
    const bool ok = (st.fails == 0) && cov_ok;
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << "rnd_sat: " << st.fails << " mismatches / "
              << st.checked << " checked\n\n";
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
