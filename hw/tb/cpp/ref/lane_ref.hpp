# pragma once
#include <cstdint>

namespace ref {
    constexpr unsigned FRAC = 13, OUT_BITS = 16;

    // mirror _rescale(..., round_mode="round") from golden model
    //      (wide + 2^(shift-1)) >> shift, then clip.
    int32_t _rnd_sat(int64_t wide, unsigned shift = FRAC, unsigned out_bits = OUT_BITS) {
        int32_t half = 1 << (shift - 1);
        int32_t x_shifted = (wide + half) >> shift;
        return x_shifted;
    }

    // One timestep of one channel
    //      h_k = rnd_sat(A*h_prev + B*x);
    //      yp = C*h_k (unrounded)
    struct LaneOut {
        int32_t h;
        int64_t yp;
    };

    LaneOut lane_step(int32_t A, int32_t B, int32_t C, int32_t h_prev, int32_t x) {
       LaneOut results;
       results.h  = _rnd_sat(A*h_prev + B*x);
       results.yp = C*results.h;
       return results;
    }

    // y = rnd_sat( sum(yp[c]) + D*x )
    int32_t array_y(const int64_t* yp, unsigned n, int32_t D, int32_t x) {

    }
}
