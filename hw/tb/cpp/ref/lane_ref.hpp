# pragma once
#include <cstdint>
#include <algorithm>

namespace ref {
    constexpr unsigned FRAC = 13, OUT_BITS = 16;
    constexpr int32_t RAIL_MAX =  (1 << (OUT_BITS - 1)) - 1;
    constexpr int32_t RAIL_MIN = -(1 << (OUT_BITS - 1));

    // mirror _rescale(..., round_mode="round") from golden model
    //      (wide + 2^(shift-1)) >> shift, then clip.
    inline int32_t rnd_sat(int64_t wide, unsigned shift = FRAC, unsigned out_bits = OUT_BITS) {
        const int64_t half    = int64_t(1) << (shift - 1);
        const int64_t q_min   = -(int64_t(1) << (out_bits -1));
        const int64_t q_max   =  (int64_t(1) << (out_bits -1)) - 1;
        const int64_t shifted = (wide + half) >> shift;
        return int32_t(std::clamp(shifted, q_min, q_max));
    }

    // One timestep of one channel
    //      h_k = rnd_sat(A*h_prev + B*x);
    //      yp = C*h_k (unrounded)
    struct LaneOut {
        int32_t h;
        int64_t yp;
    };

    inline LaneOut lane_step(int32_t A, int32_t B, int32_t C, int32_t h_prev, int32_t x) {
       LaneOut results;
       results.h  = rnd_sat(int64_t(A)*h_prev + int64_t(B)*x);
       results.yp = int64_t(C)*results.h;
       return results;
    }

    // y = rnd_sat( sum(yp[c]) + D*x )
    inline int32_t array_y(const int64_t* yp, unsigned n, int32_t D, int32_t x) {
        int64_t y = 0;
        int64_t Dx = int64_t(D)*x;
        for (unsigned i = 0; i < n; i++){
            y += yp[i];
        }
        y += Dx;
        return rnd_sat(y);
    }
}
