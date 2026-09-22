"""
ENGINEER:       Josh Rawlinson - joshrawlinson05@outlook.com
DATE:           11.09.26
DESCRIPTION:
        Test functions to validate S4 SSM model
"""

import pytest
import numpy as np

from model.ssm_model.s4_recurrence import s4_recurrence_float, s4_recurrence_fixed, quantise, dequantise
from model.ssm_model.s4_analysis import sweep_worst_case

@pytest.fixture
def ssm_params():
        """
        Builds the set of SSM coefficients shared by every test
        """
        num_channels = 8
        dt = 0.1
        tau = np.logspace(np.log10(2 * dt), np.log10(100 * dt), num_channels)

        A = -1.0 / tau
        B = np.ones(num_channels)
        C = np.random.default_rng(seed=0).normal(0, 0.5, num_channels)
        D = 0.0

        A_bar = np.exp(dt * A)
        B_bar = ((A_bar - 1.0) / A) * B

        return {
                "num_channels": num_channels,
                "dt"          : dt,
                "A_bar"       : A_bar,
                "B_bar"       : B_bar,
                "C_bar"       : C,
                "D_bar"       : D
        }


def test_impulse_response_decays(ssm_params):
        """
        Validate that impulse response tends
        to zero instead of towards infinity
        """
        x_seq = np.zeros(50)
        x_seq[0] = 1.0
        y = s4_recurrence_float(x_seq,
                                ssm_params["A_bar"], ssm_params["B_bar"],
                                ssm_params["C_bar"], ssm_params["D_bar"])
        assert np.all(np.isfinite(y))
        assert y[0] != 0.0
        assert abs(y[-1]) < abs(y[0])


def test_dc_input_matches_steady_state(ssm_params):
        """
        Input a simple step input and then determine if
        the SSM correctly decays it towards 0 and use
        the closed-form model to use as a comparitive
        """
        num_timesteps = 500
        x_seq = np.ones(num_timesteps)

        A_bar, B_bar, C_bar, D_bar = (ssm_params["A_bar"], ssm_params["B_bar"],
                                      ssm_params["C_bar"], ssm_params["D_bar"])
        y = s4_recurrence_float(x_seq, A_bar, B_bar, C_bar, D_bar)
        h_ss = B_bar / (1.0 - A_bar)        # closed-form steady state
        y_ss = np.sum(C_bar * h_ss) + D_bar # Steady state output `y = Cx + Du`

        remaining_fraction = np.max(A_bar) ** num_timesteps
        assert abs(y[-1] - y_ss) < (remaining_fraction * abs(y_ss) * 10) # 10x margin


def test_realistic_input(ssm_params):
        """
        Uses a user-defined approximate of the nominal amplitude, spike amplitude
        and probability of a spike to determine the size & precision of each
        variable within the SSM to determine bit allocation for hardware accelerator
        """
        rng = np.random.default_rng(seed=1)
        num_timesteps = 500
        nominal_amplitude = 0.5
        spike_amplitude = 0.95
        spike_probability = 0.05

        x_seq = nominal_amplitude * rng.standard_normal(num_timesteps)
        spike_mask = rng.random(num_timesteps) < spike_probability
        x_seq[spike_mask] = spike_amplitude

        y, h_trace = s4_recurrence_float(x_seq, ssm_params["A_bar"], ssm_params["B_bar"],
                                         ssm_params["C_bar"], ssm_params["D_bar"],
                                         return_state_trace=True)

        assert np.all(np.isfinite(y))

        # Calculate and store max/min values of h to determine hardware bit structure
        max_h_per_channel = np.max(np.abs(h_trace), axis=0)
        max_y = np.max(np.abs(y))
        np.savetxt("model/tests/max_h_per_channel.csv", max_h_per_channel, delimiter=",")
        with open("model/tests/max_y.txt", "w") as f:
                f.write(str(max_y))


def test_worst_case_percentiles(ssm_params):
        """
        Performs a Monte Carlo sweep random test inputs and calculates the 99.9th percentile
        of the results - used to help determine necessary bit allocation
        """
        max_h_runs, max_y_runs = sweep_worst_case(ssm_params, duration_s=3600, dt=0.01)

        h_p999 = np.percentile(max_h_runs, 99.9, axis=0)
        h_p977 = np.percentile(max_h_runs, 97.7, axis=0)
        y_p999 = np.percentile(max_y_runs, 99.9)
        y_p977 = np.percentile(max_y_runs, 97.7)

        np.savetxt("model/tests/h_p999_per_channel.csv", h_p999, delimiter=",")
        np.savetxt("model/tests/h_p977_per_channel.csv", h_p977, delimiter=",")
        with open("model/tests/y_p999.txt", "w") as f:
                f.write(str(y_p999))
        with open("model/tests/y_p977.txt", "w") as f:
                f.write(str(y_p977))


def test_fixed_match_float(ssm_params):
        """
        Test the fixed recurrence against the floating
        """
        FRAC_BITS = 13
        TOTAL_BITS = 16
        FULL_SCALE = 2 ** (TOTAL_BITS - FRAC_BITS -1)
        THRESHOLD = 0.02 * FULL_SCALE
        round_mode = "round" # 'truncate' or 'round'

        rng = np.random.default_rng(seed=2)
        x_seq = 0.5 * rng.standard_normal(3600)
        spike_mask = rng.random(3600) < 0.05
        x_seq[spike_mask] = 0.95

        y_float = s4_recurrence_float(x_seq, ssm_params["A_bar"], ssm_params["B_bar"],
                                      ssm_params["C_bar"], ssm_params["D_bar"])

        x_seq_q = quantise(x_seq, FRAC_BITS, TOTAL_BITS)
        A_bar_q = quantise(ssm_params["A_bar"], FRAC_BITS, TOTAL_BITS)
        B_bar_q = quantise(ssm_params["B_bar"], FRAC_BITS, TOTAL_BITS)
        C_bar_q = quantise(ssm_params["C_bar"], FRAC_BITS, TOTAL_BITS)
        D_bar_q = quantise(np.array(ssm_params["D_bar"]), FRAC_BITS, TOTAL_BITS)

        y_seq_q, _ = s4_recurrence_fixed(x_seq_q, A_bar_q, B_bar_q, C_bar_q, D_bar_q,
                                         coeff_frac_bits=FRAC_BITS, state_frac_bits=FRAC_BITS,
                                         total_bits=TOTAL_BITS, round_mode=round_mode)
        y_fixed_dequant = dequantise(y_seq_q, FRAC_BITS)

        error = np.abs(y_float - y_fixed_dequant)

        # Save max error as a floating point string
        np.savetxt(f"model/tests/fix_{round_mode}_error.txt", [error.max()], fmt="%.6f")

        # Save quantized vectors as integers
        np.savetxt("model/vectors/x_seq_q.txt", x_seq_q.astype(np.uint16), fmt="%d")
        np.savetxt("model/vectors/y_seq_q.txt", y_seq_q.astype(np.uint16), fmt="%d")

        assert error.max() < THRESHOLD, f"max error {error.max():.4f} exceeds {THRESHOLD:.4f} (2% of full-scale)"
        assert error.max() < THRESHOLD, f"max error{error.max():.4f} exceeds {THRESHOLD:.4f} (2% of full-scale)"
