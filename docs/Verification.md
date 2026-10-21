# Verification

This document records how each block of the DIMC-SSM accelerator is verified, and the evidence that the checks themselves can fail.
The arithmetic contract every block is checked against is in the project README.
(This document is produced by AI, but only by summarising my existing verification notes and test logs)

---

## 1. Reference model vs. golden model (`make ref_check`)

The C++ reference model (`hw/tb/cpp/ref/lane_ref.hpp`) is the oracle for every RTL testbench.
It is itself checked bit-exact against the Python fixed-point golden model (`model/ssm_model/`) using the exported vectors in `model/vectors/`.

| Check | Compared | Mismatches |
|---|---|---|
| h (8 channels × 3600 steps) | 28,800 | 0 |
| y (3600 steps) | 3,600 | 0 |

- max\|h\| per channel (LSB): 164, 224, 302, 391, 517, 685, 836, 1061. The rail is ±32767.
- Steps at a saturation rail: **0**. The golden data does not exercise saturation (see M3).
---

## 2. Mutation testing of the reference check

**Purpose.** A run with 0 mismatches only means something if the check could have failed.
Each mutation inserts one plausible bug into the reference or the checker.
The expected outcome is written down *before* the run, then compared with the actual result.
Each mutation corresponds to a bug class that is likely in the `dimc_lane` / `dimc_array` RTL.

**Method.** Each mutation was a single-line edit, applied by hand, run with `make sim_unit`, then reverted with `git restore`.

| # | Mutation | Where | Predicted | h mismatches | y mismatches | First failure | Outcome |
|---|---|---|---|---|---|---|---|
| M1 | Round → truncate (drop `+ half`) | `ref::rnd_sat` | h and y fail | 28,788 / 28,800 | 3,600 / 3,600 | h: c=0 k=1; y: k=0 | **Caught** |
| M2 | `yp` computed from `h_prev` instead of the new `h` | `ref::lane_step` | h passes, y fails | 0 | 3,576 / 3,600 | y: k=0 | **Caught** |
| M3 | Remove the saturation clamp | `ref::rnd_sat` | Survives | 0 | 0 | — | **Survived (expected)** |
| M4a | Compare h against row k+1 of `h_trace` | `tb_ref_check.cpp` h-check | Fails from k=0 | 28,536 / 28,800 | 0 | h: c=0 k=0 | **Caught** |
| M4b | Feed `x[k+1]` into the y computation | `tb_ref_check.cpp` y-check | Fails from k=0 | 0 | 0 | — | **Survived (unexpected)** |

### Analysis

**M1: rounding error accumulates through the recurrence.**
Truncation is floor, a biased error of about −0.5 LSB on every step.
Because h_k feeds back through Ā, that bias settles to an offset of about 0.5 / (1 − Ā) LSB.
So the effect grows the closer a pole is to 1:

| | max\|h\| baseline | max\|h\| under M1 | Change |
|---|---|---|---|
| ch 0 (Ā ≈ 0.61) | 164 | 162 | −2 |
| ch 7 (slowest pole) | 1061 | 1011 | −50 |

This is why nearly every step mismatches rather than a scattered few.
It is also the same mechanism behind the rejection of INT8 (Q2.5):
- the near-unity poles that give an SSM its long memory are exactly where quantisation error is amplified most;
- 16-bit Q3.13 was chosen with this in mind.
y fails at k=0, before h has drifted, because the y path has its own rounding stage.

**M2: the h and y checks are independent.**
The mutant's y_k equals the golden y_{k−1} exactly (got 0 → expected 11, got 11 → expected −19, …).
The 24 "matches" are steps where y_k = y_{k−1} by coincidence.
An exact one-step shift is only possible if y has no direct-feedthrough term, which led to the finding under M4b.
This is the RTL bug class "pass 2 reads `h_q` before commit" (lane risk #1).

**M3: survivor, a known coverage gap.**
The largest |h| in the golden data is 1061 LSB against a rail of 32767, so the clamp is never active.
An RTL lane with the saturation logic deleted would pass the golden tests.
→ **A lane saturation test is required:** Ā = 8192, large ±x, hit both rails and check recovery. This is lane Test 4 (section 4).

**M4a: indexing convention confirmed.**
Row k of `h_trace` is h *after* step k.
The 264 coincidental matches are steps where h_{k+1} = h_k, mostly on the slow channels.
*Note:* at k = 3599 this mutation reads `h_trace[28800 + c]`, past the end of the vector.
`std::vector::operator[]` does not bound-check, so this was undefined behaviour that happened to produce a mismatch.

**M4b: survivor, a gap in the golden data.**
`D_bar_q.txt` = 0, because the golden-model fixture sets `D = 0.0` (`model/ssm_model/s4_tests.py`).
So D·x contributes nothing whatever value of x is supplied. As a result:
- the D·x term in `ref::array_y` has never been checked against Python;
- the D·x multiply in `dimc_array` cannot be verified by `y_seq_q`. An array with that multiply deleted would pass 3600/3600.
This mutation also reads past the end of `x` at k = 3599 (the same UB note as M4a).

### Follow-ups arising

| Finding | Action | Status |
|---|---|---|
| M3: saturation never exercised | Lane saturation test (both rails + recovery) | ✅ Done: lane Test 4, 537 steps, 134 at each rail (section 4) |
| M4b: D = 0 in the golden data | Export a second golden vector set with D ≠ 0; re-run `ref_check`; use it in the array testbench | Open: needed for `dimc_array` (M1b) |
| M4a/M4b: out-of-range reads were silent | Index golden vectors with `.at()` in all testbenches | `tb_rnd_sat` ✅; `tb_dimc_lane` still uses `[]`, to be switched |

---

## 3. Unit verification

### `bs_mult` (signed bit-serial multiplier, W × W → 2W, latency W+1)

| Test | Coverage | W = 4 | W = 8 | W = 16 |
|---|---|---|---|---|
| 1. Directed | Mixed-sign products | ✅ | ✅ | ✅ |
| 2. Exhaustive | All signed operand pairs | ✅ 256 | ✅ 65,536 | skipped |
| 3. Corners + random | 64 corner + 10k random (seed 12345) | ✅ | ✅ | ✅ |
| 4. Start while busy | `start_i` ignored mid-multiply | ✅ | ✅ | ✅ |
| 5. Done pulse | `done_o` is one cycle; result held while idle | ✅ | ✅ | ✅ |
| 6. Back-to-back | Start accepted on the done cycle (zero-bubble chaining) | ✅ | ✅ | ✅ |

Latency (W+1) is asserted on every product.
Every consecutive pair of multiplies in tests 1–3 is already a back-to-back start, because the helper raises `start_i` on the done cycle.
Test 6 makes this coverage explicit and guards it.
The design is `-Wall` lint-clean.

### `rnd_sat` (requantiser: round-half-up, then saturate)

Combinational. `out = clamp(floor(in / 2^SHIFT + 1/2), OUT_MIN, OUT_MAX)`.
Oracle: `ref::rnd_sat`, itself checked bit-exact against the Python golden model (section 1).

| Config (IN_W / SHIFT / OUT_W) | Purpose | Exhaustive | Directed | Random: in-range / sat+ / sat− | Total checked | Result |
|---|---|---|---|---|---|---|
| 12 / 4 / 6 | Algorithm proof (every input) | 4,096 | 91 | 57,557 / 21,303 / 21,140 | 104,187 | ✅ 0 mismatches |
| 33 / 13 / 16 | Lane instance: h_k = rnd_sat(Ā·h + B̄·x) | — | 98 | 53,123 / 23,348 / 23,529 | 100,098 | ✅ 0 mismatches |
| 36 / 13 / 16 | Array instance: y_k = rnd_sat(Σ yp + D·x) | — | 98 | 50,389 / 24,698 / 24,913 | 100,098 | ✅ 0 mismatches |

**Directed stages:**
- ties: exactly ±0.5 LSB for k = −4…4, plus each tie ±1. Covers negative ties (−0.5 → 0, −3.5 → −3).
- rail boundaries: the input windows that round to OUT_MAX−1, OUT_MAX, OUT_MAX+1, OUT_MIN+1, OUT_MIN and OUT_MIN−1.
- far out of range: where a missing clamp wraps instead of saturating.
- input extremes: IN_MIN, IN_MAX, 0, ±1.
**Random stage:** 100k samples (seed 12345). Half are uniform over the full input range; half are in a window across both rails, because uniform sampling alone saturates ~94 % of the time at IN_W = 33. Every class must be hit ≥ 1,000 times, or the run fails.

#### Mutation testing (RTL)

**Method:** one mutation at a time in `rnd_sat.sv`, with temporary `lint_off UNUSEDSIGNAL`/`WIDTH` waivers, run at the lane configuration (33/13/16), then reverted with `git restore`.

| # | Mutation | Predicted first failure | First failure (actual) | Mismatches | Outcome |
|---|---|---|---|---|---|
| R1 | No overflow headroom: `RND_W = IN_W − SHIFT` | extreme, IN_MAX | extreme: in = IN_MAX−1 and IN_MAX → −32768 (expected +32767) | **2** / 100,098 | **Caught only by directed extremes** |
| R2 | Rail chosen from `rounded[OUT_W−1]`, not the true MSB | rail-lo | rail-lo: 32767.5 LSB → −32768 (expected +32767) | 25,057 / 100,098 | Caught |
| R3 | No rounding carry-in (`rounded = truncated`, i.e. truncation) | tie | tie: −3.5 LSB → −4 (expected −3) | 26,468 / 100,098 | Caught |
| R4 | No saturation (`out_o = rounded[OUT_W−1:0]`) | rail-hi | rail-lo: 32767.5 LSB → −32768 (wraps) | 46,908 / 100,098 | Caught |

All four were caught, and each first failure was in the stage designed to catch it.

#### Analysis

**R1: overflow at the top of the input range (the key finding).**
- Without the extra bit, rounding up the largest inputs (truncated value at its maximum, with a carry-in of 1) wraps the sign. The value then saturates to the **wrong rail**.
- This affects only inputs in [IN_MAX − 2^(SHIFT−1) + 1, IN_MAX], which is the top 2¹² of the 2³³ input values.
- A uniformly random sample hits that region with p = 2¹² / 2³³ = 2⁻²¹. The 50,000 uniform samples would catch the bug with probability 1 − (1 − 2⁻²¹)^50000 ≈ **2.4 %**. In this run they didn't: both mismatches came from the directed extreme-value test.
- Conclusion: random testing alone is not enough for arithmetic corner cases. Directed boundary tests are needed alongside it.
**R2: the rail must come from the true sign.**
- Only *saturating* inputs are affected. In range, the rail logic isn't used.
- Among saturating inputs, the wrong rail is chosen whenever bit OUT_W−1 differs from the true sign. In the far region that's about half the time: 25,057 ≈ ½ × 46,877 saturating random samples.
- So in-range tests alone can never detect this bug.
**R3: rounding vs truncation.**
- Errors appear only for in-range inputs whose discarded fraction is ≥ 0.5 LSB. That's about half the in-range samples: 26,468 ≈ ½ × 53,123.
- The printed errors are negative ties only because the log stops after 10 errors and the tie loop starts at k = −4. Positive ties fail too (e.g. 2.5 → 2, expected 3).
- This is the hardware equivalent of reference mutation M1 (section 2). In the lane's recurrence, its −0.5 LSB bias is amplified by 1/(1 − Ā).
**R4: missing saturation wraps around.**
- **Every** saturating input fails: 46,908 ≈ 46,877 saturating random samples + 31 directed.
- The far-out-of-range stage shows the wrap explicitly: 32769.5 LSB → −32766, and 65536 LSB → 0.
- R2 and R4 fail on the same rail-boundary inputs, but the **far-out-of-range stage tells them apart**: R2 always gives a clean (wrong) rail, while R4 gives wrapped values.
**Consequence for the lane:** golden-vector testing never saturates (section 2, M3). So R2 and R4 would only be caught at lane level by the saturation test (lane Test 4), which is why it's required. This unit test is the first line of defence, and Test 4 confirms it at lane level.

---

## 4. Lane verification: `dimc_lane`

One S4D channel. One bit-serial multiplier (`bs_mult`) runs three passes per timestep:

| Pass | Operation |
|---|---|
| 0 | acc = sext(Ā·h_{k−1}) |
| 1 | acc += sext(B̄·x_k) |
| COMMIT | h_k = rnd_sat(acc) |
| 2 | yp_k = C·h_k |

Latency is 3W + 8 = **56 cycles** per timestep at W = 16.

**Oracles.** The testbench drives and observes ports only; it never reads internal signals. Expected values come from four places that do not depend on the RTL:
1. **Paper:** hand-computed values that are exact in Q3.13 (Test 1). `ref::lane_step` is checked against the same values.
2. **`ref::lane_step`:** the reference keeps its own h from step to step and is never re-seeded from the DUT.
3. **Golden trace:** `h_trace_q.txt` from the Python model (Test 3).
4. **Identity yp = C·h, using the DUT's own h:** this still holds when the h chain has diverged, so it separates h-path bugs from pass-2 bugs.
**Protocol checks on every step** (inside the `step()` driver, in every test):
- `in_ready_o` is high before the accept, low while busy, and high again on the result cycle;
- `out_valid_o` arrives exactly 56 cycles after the accept;
- a watchdog stops the run if `out_valid_o` never arrives.
### Results (`make sim_unit`, 20 Oct 2026)

| Test | Coverage | Compared | Mismatches |
|---|---|---|---|
| 1. Hand-computed | Two paper cases, both signs of Āh (oracle checked too) | 2 h + 2 yp + 4 oracle | ✅ 0 |
| 2. Directed | Rounding ties (incl. −0.5 → 0, −1.5 → −1), \|product\| ≥ 2¹⁵, all four sign combinations of (Āh, B̄x), zero and extreme operands, 20-step chain | 36 h + 36 yp | ✅ 0 |
| 3. Golden, channel 0 | 3600 steps: h vs ref, h vs golden, yp vs ref, yp == C·h | 3,600 × 4 | ✅ 0 |
| 3. Golden, channels 1–7 | As above | 25,200 × 4 | ✅ 0 |
| 4. Saturation | Climb to +rail, fall to −rail, recovery; \|acc\| = 2³¹; rail-boundary rounding; overflow sweep | 537 h + 537 yp | ✅ 0 |
| 5. Random | 10k steps, seed 12345 (half full-range, half in-range biased) | 10,000 h + 10,000 yp | ✅ 0 |
| 6. Handshake | One-cycle `out_valid_o`, outputs held while idle; random valid/x/writes on every busy cycle ignored; write + accept on the same edge | 53 h + 53 yp | ✅ 0, 0 violations |

**Totals:** 39,428 steps, 2,250,268 cycles. Latency min/max **56/56**, 0 protocol violations. Golden h: **28,800 / 28,800** bit-exact. The design is `-Wall` lint-clean.

**Coverage** (each output class must be hit, or the run fails):

| Test | +rail | −rail | In range | Required |
|---|---|---|---|---|
| 4. Saturation | 134 | 134 | 269 | ≥ 50 each |
| 5. Random | 1,432 | 1,451 | 7,117 | ≥ 500 each |

Test 4 closes the M3 gap: the golden data never reaches a rail, and this test reaches both rails 134 times each.

### Bug caught during bring-up (red → green)

The first version of the lane sign-extended the 32-bit product into the 33-bit accumulator from `mult_product[DATA_W-1]` (bit 15) instead of the product's true sign bit, bit 31.
The widths still matched (1 + 32 = 33), so lint could not see it.
The counts were recorded with the first version of the lane testbench. Its tests map one-to-one onto the current Tests 1–6, except that it also had a reset-mid-step check (1 extra step in Test 6).

| Test | Mismatches with the bug | After the fix |
|---|---|---|
| 1. Hand-computed h | 2 / 2 | 0 |
| 2. Directed h | 25 / 36 | 0 |
| 3. Golden ch 0, h vs golden | 3,600 / 3,600 (first failure k = 0) | 0 |
| 3. Golden ch 1–7, h vs golden | 25,200 / 25,200 | 0 |
| 3. yp == C·h (all channels) | **0 / 28,800** | 0 |
| 4. Saturation h | 276 / 537 | 0 |
| 5. Random h | 4,960 / 10,000 | 0 |
| 6. Handshake h | 53 / 54 | 0 |
| Protocol violations | 0 | 0 |

**Analysis:**
- **The bug is broader than the classic risk-2 symptom ("wrong only when Āh < 0").** Any product whose bit 15 differs from bit 31 is extended wrongly. Test 1 case a has Āh > 0 and still fails, because B̄x = −2²⁴ = `0xFF00_0000` has bit 15 = 0.
- **Test 5 fails 4,960 / 10,000 ≈ ½.** For random products, bit 15 matches the sign bit about half the time. The failure rate the arithmetic predicts is the rate measured.
- **The 11 Test 2 cases that passed have |product| < 2¹⁵**, where bit 15 equals the sign bit and the bug cannot show. The `product +33000` case was built for exactly this bug and caught it.
- **`yp == C·h` passed 28,800 / 28,800 while every other golden check failed.** That localised the fault to the h path (accumulator or rounding) before any waveform was opened: pass 2 and the output path were correct.
- **Zero protocol violations and latency 56 throughout:** the bug was datapath-only, and the FSM timing was right on the first attempt.

