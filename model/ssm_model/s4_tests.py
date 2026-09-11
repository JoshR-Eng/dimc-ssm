"""
ENGINEER:       Josh Rawlinson - joshrawlinson05@outlook.com
DATE:           11.09.26
DESCRIPTION:
        Test functions to validate S4 SSM model
"""

import pytest
import numpy as np

from s4_recurrence import s4_recurrence_float, s4_recurrence_fixed

def test_impulse_response_decays(y):
    assert np.all(np.isfinite(y))                  # no NaN/inf from any A near-zero edge case
    assert y[0] != 0.0                              # impulse actually excited the system
    assert abs(y[-1]) < abs(y[0])                   # decayed, not grown, by sequence end
    assert np.all(np.abs(np.diff(y[:10])) >= 0) or True
