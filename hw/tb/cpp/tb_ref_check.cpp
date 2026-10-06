// Validates the C++ reference model (ref/lane_ref.hpp) against
// the python golden model vectors
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "ref/vectors.hpp"
#include "ref/lane_ref.hpp"

namespace cfg {
    constexpr size_t T = 3600;  // timesteps
    constexpr size_t N = 8;     // Channels
    const std::string DIR = "model/vectors/";
}

struct Check {
    std::string name;
    uint64_t    compared = 0;
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

int main() {
    try {
        // --- Load (sizes are count checks on the data itself)
        const auto x       = vec::load_sized(cfg::DIR + "x_seq_q.txt",   cfg::T);
        const auto y_gold  = vec::load_sized(cfg::DIR + "y_seq_q.txt",   cfg::T);
        const auto h_trace = vec::load_sized(cfg::DIR + "h_trace_q.txt", cfg::T * cfg::N);
        const auto A       = vec::load_sized(cfg::DIR + "A_bar_q.txt",   cfg::N);
        const auto B       = vec::load_sized(cfg::DIR + "B_bar_q.txt",   cfg::N);
        const auto C       = vec::load_sized(cfg::DIR + "C_bar_q.txt",   cfg::N);
        const auto D       = vec::load_sized(cfg::DIR + "D_bar_q.txt",   1);

        Check h_chk{"h"}, y_chk{"y"};
        std::vector<std::array<int64_t, cfg::N>> yp(cfg::T);
        std::array<int32_t, cfg::N> max_abs_h{};
        uint64_t rail_hits = 0;

        // --- H-Check
        for (size_t c = 0; c < cfg::N; ++c) {
            int32_t h = 0;
            for (size_t k = 0; k < cfg::T; ++k) {
                // step the reference for channel c at timestep k
                const auto [h_new, yp_new] = ref::lane_step(A[c], B[c], C[c], h, x[k]);
                // check the result is as expected
                h_chk.expect(h_new, h_trace[(k*cfg::N) + c],
                             "c=" + std::to_string(c) + " k=" + std::to_string(k));
                // record yp[k][c], advance h, update max_abs_h[c]
                yp[k][c] = yp_new;
                h = h_new;
                max_abs_h[c] = std::max(max_abs_h[c], std::abs(h_new));
                // if the new h sits on either rail (-32768 or 32767), ++rail_hits
                if ((h == ref::RAIL_MAX) || (h == ref::RAIL_MIN)) ++rail_hits;
            }
        }

        // --- Y-Check
        for (size_t k = 0; k < cfg::T; ++k) {
            y_chk.expect(ref::array_y(yp[k].data(), cfg::N, D[0], x[k]), y_gold[k],
                         "k=" + std::to_string(k));
        }

        // --- Report
        bool ok = h_chk.report(cfg::T * cfg::N);
        ok     &= y_chk.report(cfg::T);
        std::cout << "[INFO] max|h| per channel:";
        for (auto m : max_abs_h) std::cout<< " " << m;
        std::cout << "\n[INFO] steps at a saturation rail: " << rail_hits
                  << (rail_hits == 0 ? " (golden data does NOT exercise saturation)" : "")
                  << "\n";
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
