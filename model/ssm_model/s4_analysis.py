"""
ENGINEER:       Josh Rawlinson - joshrawlinson05@outlook.com
DATE:           09.09.26
DESCRIPTION:
    A few test functions to enable more complex analysis
    of the S4 model
"""
import numpy as np
from model.ssm_model.s4_recurrence import s4_recurrence_float, s4_recurrence_fixed


def sweep_worst_case(ssm_params, duration_s, dt,
                     num_runs=500, nominal_amplitude=0.5,
                     spike_amplitude=0.95, spike_probability=0.05):
    num_timesteps = int(duration_s / dt)
    A_bar, B_bar = ssm_params["A_bar"], ssm_params["B_bar"]
    num_channels = A_bar.size

    rng = np.random.default_rng(seed=0)
    # shape (num_runs, num_timesteps) - one input sequence per run, generated all at once
    x_seq = nominal_amplitude * rng.standard_normal((num_runs, num_timesteps))
    spike_mask = rng.random((num_runs, num_timesteps)) < spike_probability
    x_seq[spike_mask] = spike_amplitude

    h = np.zeros((num_runs, num_channels))
    max_h = np.zeros((num_runs, num_channels))
    max_y = np.zeros(num_runs)

    for t in range(num_timesteps):
        x_t = x_seq[:, t][:, None]                         # shape (num_runs, 1) - broadcasts over channels
        h = A_bar * h + B_bar * x_t                        # (num_runs, num_channels), all runs at once
        y_t = np.sum(ssm_params["C_bar"] * h, axis=1) + ssm_params["D_bar"] * x_seq[:, t]

        max_h = np.maximum(max_h, np.abs(h))
        max_y = np.maximum(max_y, np.abs(y_t))

    return max_h, max_y   # shape (num_runs, num_channels), (num_runs,)
