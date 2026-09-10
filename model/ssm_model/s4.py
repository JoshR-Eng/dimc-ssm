"""
ENGINEER:       Josh Rawlinson - joshrawlinson05@outlook.com
DATE:           09.09.26
DESCRIPTION:
    A small reference S4 model of the quantised single channel
    recurrence. This will provide as a test model to pass through
    the State-Space Model Accelerator
"""

import numpy as np
import pytest
import matplotlib.pyplot as plt

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


# --- Calculate recurrence
def s4_recurrence(x_seq: np.ndarray,
                  A_bar: np.ndarray, B_bar: np.ndarray,
                  C_bar: np.ndarray, D_bar: np.ndarray) -> np.ndarray:
    h = np.zeros(np.size(A_bar))
    y_seq = []
    for x_t in x_seq:
        h = A_bar * h + B_bar * x_t
        y_t = np.sum(C_bar * h) + (D_bar * x_t)
        y_seq.append(y_t)
    y_seq = np.array(y_seq)
    return y_seq

x_seq = np.zeros(num_timesteps)
x_seq[0] = 1.0                  # Impulse at t=0
y_seq = s4_recurrence(x_seq, A_bar, B_bar, C_bar, D_bar)
print(y_seq)

