"""
ENGINEER:       Josh Rawlinson - joshrawlinson05@outlook.com
DATE:           09.09.26
DESCRIPTION:
    A small reference S4 model of the quantised single channel
    recurrence. This will provide as a test model to pass through
    the State-Space Model Accelerator
"""

import numpy as np

from s4_tests import test_impulse_response_decays
from s4_recurrence import s4_recurrence_float, s4_recurrence_fixed


# --- Define Initial State Vectors
num_channels: int = 8   # Number of parallel salar recurrences
num_timesteps = 50      # Number of timesteps
time = np.zeros(num_timesteps)
for i in range(0,num_timesteps):
    time[i] = i * 0.1
dt = 0.1                # Amount of time between each step

tau = np.logspace(np.log10(2*dt),   # A decay rate for each channel
                  np.log10(100*dt), # 2 to 100 samples of memory
                  num_channels)

A = -1.0 / tau
B = np.ones(num_channels)
C = np.random.default_rng(seed=0).normal(0, 0.5, num_channels)
D = 0

# --- Descretise State Space Model
A_bar = np.exp(dt * A)              # elementwise exp
B_bar = ((A_bar - 1.0) / A) * B     # Exact ZOH
C_bar = C
D_bar = D

print(f"A_bar:\n{A_bar}\n\nB_bar:\n{B_bar}\n\nC:\n{C}\n\nD:\n{D}")

# --- Calculate recurrence
x_seq = np.zeros(num_timesteps)
x_seq[0] = 1.0                  # Impulse at t=0
y_seq = s4_recurrence_float(x_seq, A_bar, B_bar, C_bar, D_bar)
print(f"\ny_seq: \n{y_seq}")


# --- Test/Validation
test_impulse_response_decays(y=y_seq)
