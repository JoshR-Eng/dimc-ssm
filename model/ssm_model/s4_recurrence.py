"""
ENGINEER:       Josh Rawlinson - joshrawlinson05@outlook.com
DATE:           09.09.26
DESCRIPTION:
    Recurrence Logic for S4 SSM model
"""
import numpy as np


# ==============================================================================
# Floating Recurrence
#       All variables are floating points, enabling high precision
#       capable of being the comparable baseline to determine how
#       different precisions will affect accuracy
# ==============================================================================

def s4_recurrence_float(x_seq: np.ndarray,
                        A_bar: np.ndarray, B_bar: np.ndarray,
                        C_bar: np.ndarray, D_bar: np.ndarray,
                        return_state_trace: bool = False) -> np.ndarray:
    """
    Basic recurrent function with no contraint to bit length
    and no quantisation
    """
    h = np.zeros(np.size(A_bar))
    y_seq = []
    h_trace = [] if return_state_trace else None
    for x_t in x_seq:
        h = A_bar * h + B_bar * x_t
        y_t = np.sum(C_bar * h) + (D_bar * x_t)
        y_seq.append(y_t)
        if return_state_trace:
            h_trace.append(h.copy())
    y_seq = np.array(y_seq)
    if return_state_trace:
        return y_seq, np.array(h_trace)
    return y_seq




# ==============================================================================
# Fixed Recurrence
#       Variables are fixed to given bit lengths
#       this enables validation and modelling of models
#       that could run on the SSM hardware accelerator
# ==============================================================================

# --- Quantisation & Dequantisation functions
def quantise(x, frac_bits, total_bits):
    scale = 1 << frac_bits
    q_min, q_max = -(1 << (total_bits -1)), (1 << (total_bits -1)) - 1
    return np.clip(np.round(x * scale), q_min, q_max).astype(np.int64)

def dequantise(x_q, frac_bits):
    """
    Quantisation is lossy - hence, this quantisation function
    only changes the data from integer to floating-point format to enable
    accurate comparison with floating model but its not capable of
    returning the information loss when originally quantised
    """
    return x_q.astype(np.float64) / (1 << frac_bits)



# --- Rescale function to shorten long bit lengths to a fixed allocation
def _rescale(x_wide, shift, total_bits, round_mode="truncate"):
    """
    Bring a wide intermediate (extra bits from multiple & addition)
    back down to the working format then saturate
    """
    if round_mode == "truncate":
        x_shifted = x_wide >> shift             # arithmetic shift
    elif round_mode == "round":
        half = 1 << (shift -1)
        x_shifted = (x_wide + half) >> shift    # round-half-up
    else:
        raise ValueError(f"Unknown round_mode: {round_mode}")
    q_min = -(1 << (total_bits - 1))
    q_max =  (1 << (total_bits - 1)) - 1
    return np.clip(x_shifted, q_min, q_max)


# --- Fixed Recurrence Function
def s4_recurrence_fixed(x_seq, A_bar_q, B_bar_q, C_q, D_q,
                        coeff_frac_bits, state_frac_bits, total_bits):
    """
    S4 Recurrence function with contained bit sizes
    and quantisation
    """
    h_q = np.zeros_like(A_bar_q, dtype=np.int64)
    y_seq = []
    for x_t_q in x_seq:
        # Multiply: int*int -> double width int
        Ah = A_bar_q * h_q
        Bx = B_bar_q * x_t_q

        # Re-scale back down to state_frac_bits and saturate to total_bits
        # This Implements 'truncation-after-multiply'
        h_raw = (Ah + Bx) >> coeff_frac_bits
        h_q = np.clip(h_raw, -(1 << (total_bits - 1)), (1 << (total_bits - 1)) - 1)

        # Reduce across channels:
        # use wider (int64) accumulator before any final rounding - models adder tree's gaurd bits
        y_acc = np.sum((C_q * h_q).astype(np.int64)) + D_q * x_t_q
        y_seq.append(y_acc)
    return np.array(y_seq)

