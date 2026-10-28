"""
features.py

Turns a raw TDR window (an array of amplitude samples) into a small set of
hand-picked numbers a classical ML model (Random Forest / SVM) can use.

This is the "classical ML" path we settled on as the primary approach: with
a small, self-collected real dataset, a few good features + a simple model
generalizes better than a CNN, which needs more data to avoid overfitting.
The CNN in train_cnn.py is kept as the stretch option if these features
turn out not to separate soft faults from noise well enough on real data.

Each feature has a physical meaning, which also makes it much easier to
explain in a review if asked "why these features":

  peak_amp       size of the biggest reflection in the window (how strong)
  peak_pos       where in the window it happens (proxy for fault location)
  peak_sign      +1 or -1 (open-like vs short-like reflection)
  rise_time      how many samples the edge takes to develop (sharp vs soft)
  energy         total signal energy in the window (background vs event)
  baseline_std   noise level estimated from the start of the window,
                 used to tell "real reflection" apart from "just noise"
"""

import numpy as np


def extract_features(window):
    """window: 1D array of amplitude samples. Returns a 1D feature vector."""
    window = np.asarray(window, dtype=np.float64)
    n = len(window)

    baseline = window[: max(5, n // 10)]
    baseline_std = float(np.std(baseline))

    idx_peak = int(np.argmax(np.abs(window)))
    peak_amp = float(window[idx_peak])
    peak_pos = idx_peak / n  # normalized 0..1, robust to window length changes
    peak_sign = 1.0 if peak_amp >= 0 else -1.0

    # rise time: samples between 10% and 90% of the peak value, searched
    # in a small region around the peak
    lo = max(0, idx_peak - 20)
    hi = min(n, idx_peak + 5)
    segment = window[lo:hi]
    target = abs(peak_amp)
    if target < 1e-6 or len(segment) < 2:
        rise_time = 0.0
    else:
        norm = np.abs(segment) / target
        above_10 = np.argmax(norm >= 0.1) if np.any(norm >= 0.1) else 0
        above_90 = np.argmax(norm >= 0.9) if np.any(norm >= 0.9) else len(segment) - 1
        rise_time = float(max(above_90 - above_10, 0))

    energy = float(np.sum(window ** 2) / n)

    return np.array([
        peak_amp,
        peak_pos,
        peak_sign,
        rise_time,
        energy,
        baseline_std,
    ], dtype=np.float64)


FEATURE_NAMES = ["peak_amp", "peak_pos", "peak_sign", "rise_time", "energy", "baseline_std"]


def extract_features_batch(X):
    """X: (n_samples, window_len) raw windows -> (n_samples, n_features)."""
    return np.stack([extract_features(row) for row in X])
