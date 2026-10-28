"""
synthetic_data.py

Generates fake but physically-reasonable TDR (Time Domain Reflectometry)
reflection windows for three classes:

    0 = healthy      no meaningful reflection, just baseline noise
    1 = soft_fault    a small, partial reflection (corrosion, loose connector,
                       a cable that is starting to degrade)
    2 = hard_fault    a strong reflection (a clean open circuit or dead short)

Each example is one "window": a short slice of the reflected voltage signal
(what the STM32 would timestamp after the Schmitt trigger has cleaned it up),
sampled at WINDOW_LEN points.

Physics this tries to capture, kept deliberately simple:
  - Sending an incident pulse and reading nothing but noise back = healthy.
  - Hitting a fault produces a step in the reflected signal. Its SIGN tells
    you whether the impedance went up (positive reflection, open-like) or
    down (negative reflection, short-like). Its SIZE tells you how severe
    the fault is: an almost-complete break reflects nearly all the incident
    energy back (hard fault), a partial one only reflects a fraction of it
    (soft fault).
  - Real electronics + Schmitt trigger cleanup make the edge not instant,
    so it is smoothed with a short rise time rather than a single jump.
  - Real signals are noisy, so Gaussian noise is added.

This is a teaching / bootstrap dataset only. It is meant to get a working
pipeline (feature extraction + classifier) running before real captures
from the STM32 + Schmitt trigger probe are available. See the note at the
bottom of this file for how to swap in real data later.
"""

import numpy as np

WINDOW_LEN = 200      # samples per window
CLASSES = ["healthy", "soft_fault", "hard_fault"]


def _smoothed_step(window_len, step_pos, amplitude, rise_samples, rng):
    """A step function at step_pos with a finite rise time, using a sigmoid
    so the edge looks like a real (bandwidth-limited) reflection edge rather
    than a perfect digital jump."""
    t = np.arange(window_len)
    # sigmoid centered at step_pos, width controlled by rise_samples
    edge = 1.0 / (1.0 + np.exp(-(t - step_pos) / max(rise_samples, 1e-6)))
    return amplitude * edge


def generate_example(label, rng):
    """Generate one synthetic window for the given class label (0,1,2)."""
    baseline_noise_std = rng.uniform(0.015, 0.06)
    signal = rng.normal(0, baseline_noise_std, size=WINDOW_LEN)

    # slow linear drift, mimics cable attenuation / small DC offset
    drift = rng.uniform(-0.01, 0.01) * np.arange(WINDOW_LEN) / WINDOW_LEN
    signal += drift

    if label == 0:
        # healthy: no real reflection, maybe a tiny residual mismatch
        # (range overlaps the low end of soft_fault on purpose, a real
        # healthy cable and a very early soft fault are genuinely hard
        # to tell apart, and the model should be tested on that)
        residual = rng.uniform(-0.10, 0.10)
        step_pos = rng.integers(20, WINDOW_LEN - 20)
        signal += _smoothed_step(WINDOW_LEN, step_pos, residual, rise_samples=3, rng=rng)

    elif label == 1:
        # soft fault: partial reflection, moderate size, either polarity
        # (deliberately overlaps healthy at the low end and hard_fault
        # at the high end, this is not a clean-separation toy problem)
        sign = rng.choice([-1.0, 1.0])
        amplitude = sign * rng.uniform(0.08, 0.55)
        step_pos = rng.integers(20, WINDOW_LEN - 20)
        rise = rng.uniform(2, 6)  # softer faults often have a slightly slower edge
        signal += _smoothed_step(WINDOW_LEN, step_pos, amplitude, rise, rng)

    else:
        # hard fault: strong reflection, near +-1, sharper edge
        sign = rng.choice([-1.0, 1.0])
        amplitude = sign * rng.uniform(0.45, 1.0)
        step_pos = rng.integers(20, WINDOW_LEN - 20)
        rise = rng.uniform(1, 3)
        signal += _smoothed_step(WINDOW_LEN, step_pos, amplitude, rise, rng)

    return signal.astype(np.float32)


def generate_dataset(n_per_class=400, seed=0):
    """Returns X (n_samples, WINDOW_LEN) and y (n_samples,) with integer labels
    0=healthy, 1=soft_fault, 2=hard_fault."""
    rng = np.random.default_rng(seed)
    X, y = [], []
    for label in range(3):
        for _ in range(n_per_class):
            X.append(generate_example(label, rng))
            y.append(label)
    X = np.stack(X)
    y = np.array(y, dtype=np.int64)
    # shuffle
    idx = rng.permutation(len(y))
    return X[idx], y[idx]


def save_csv(path, X, y):
    """Save as one row per example: label, then WINDOW_LEN amplitude samples.
    This is the SAME format real captures should be saved in (see README)."""
    header = "label," + ",".join(f"t{i}" for i in range(X.shape[1]))
    data = np.column_stack([y.reshape(-1, 1), X])
    np.savetxt(path, data, delimiter=",", header=header, comments="", fmt="%.6f")


if __name__ == "__main__":
    X, y = generate_dataset(n_per_class=400)
    save_csv("tdr_synthetic.csv", X, y)
    print(f"Saved {len(y)} synthetic windows to tdr_synthetic.csv")
    print(f"Classes: {CLASSES}, counts: {np.bincount(y)}")
