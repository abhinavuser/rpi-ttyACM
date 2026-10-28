"""
demo_standalone.py

Standalone demo script for TDR fault classification.
Does not require external modules - all helper functions included.

Usage:
    python ml/demo_standalone.py
    # or
    cd ml && python demo_standalone.py
"""

import os
import sys
import numpy as np
import joblib

try:
    import torch
    import torch.nn as nn
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

WINDOW_LEN = 200
CLASSES = ["healthy", "soft_fault", "hard_fault"]
FEATURE_NAMES = ["peak_amp", "peak_pos", "peak_sign", "rise_time", "energy", "baseline_std"]


def extract_features(window):
    """Extract 6 physical features from raw window"""
    window = np.asarray(window, dtype=np.float64)
    n = len(window)

    baseline = window[: max(5, n // 10)]
    baseline_std = float(np.std(baseline))

    idx_peak = int(np.argmax(np.abs(window)))
    peak_amp = float(window[idx_peak])
    peak_pos = idx_peak / n
    peak_sign = 1.0 if peak_amp >= 0 else -1.0

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


def extract_features_batch(X):
    """Extract features from multiple windows"""
    return np.stack([extract_features(row) for row in X])


if HAS_TORCH:
    class TDRConvNet(nn.Module):
        """1D-CNN model for TDR classification"""
        def __init__(self, window_len=WINDOW_LEN, n_classes=3):
            super().__init__()
            self.features = nn.Sequential(
                nn.Conv1d(1, 16, kernel_size=7, padding=3),
                nn.ReLU(),
                nn.MaxPool1d(2),
                nn.Conv1d(16, 32, kernel_size=5, padding=2),
                nn.ReLU(),
                nn.MaxPool1d(2),
                nn.Conv1d(32, 32, kernel_size=3, padding=1),
                nn.ReLU(),
                nn.AdaptiveAvgPool1d(1),
            )
            self.classifier = nn.Sequential(
                nn.Flatten(),
                nn.Linear(32, 32),
                nn.ReLU(),
                nn.Dropout(0.2),
                nn.Linear(32, n_classes),
            )

        def forward(self, x):
            x = x.unsqueeze(1)
            x = self.features(x)
            return self.classifier(x)


def generate_demo_samples():
    """Generate one sample per class for demo"""
    rng = np.random.default_rng(999)
    samples = []

    for label in range(3):
        baseline_noise_std = rng.uniform(0.015, 0.06)
        signal = rng.normal(0, baseline_noise_std, size=WINDOW_LEN)
        drift = rng.uniform(-0.01, 0.01) * np.arange(WINDOW_LEN) / WINDOW_LEN
        signal += drift

        if label == 0:
            residual = rng.uniform(-0.10, 0.10)
            step_pos = rng.integers(20, WINDOW_LEN - 20)
            t = np.arange(WINDOW_LEN)
            edge = 1.0 / (1.0 + np.exp(-(t - step_pos) / 3))
            signal += residual * edge
        elif label == 1:
            sign = rng.choice([-1.0, 1.0])
            amplitude = sign * rng.uniform(0.08, 0.55)
            step_pos = rng.integers(20, WINDOW_LEN - 20)
            rise = rng.uniform(2, 6)
            t = np.arange(WINDOW_LEN)
            edge = 1.0 / (1.0 + np.exp(-(t - step_pos) / max(rise, 1e-6)))
            signal += amplitude * edge
        else:
            sign = rng.choice([-1.0, 1.0])
            amplitude = sign * rng.uniform(0.45, 1.0)
            step_pos = rng.integers(20, WINDOW_LEN - 20)
            rise = rng.uniform(1, 3)
            t = np.arange(WINDOW_LEN)
            edge = 1.0 / (1.0 + np.exp(-(t - step_pos) / max(rise, 1e-6)))
            signal += amplitude * edge

        samples.append(signal.astype(np.float32))

    return np.array(samples), np.array([0, 1, 2])


def load_models():
    """Load both trained models"""
    print("=" * 80)
    print("TDR FAULT CLASSIFIER - STANDALONE DEMO")
    print("=" * 80)
    print("\n[1/4] Loading trained models...")

    rf_path = os.path.join(BASE_DIR, "tdr_classifier.joblib")
    if not os.path.exists(rf_path):
        raise FileNotFoundError(f"Random Forest model not found at {rf_path}")
    rf_model = joblib.load(rf_path)
    print("  [OK] Random Forest model loaded")

    cnn_model = None
    if HAS_TORCH:
        cnn_path = os.path.join(BASE_DIR, "tdr_cnn.pt")
        if os.path.exists(cnn_path):
            cnn_model = TDRConvNet()
            cnn_model.load_state_dict(torch.load(cnn_path, weights_only=True))
            cnn_model.eval()
            print("  [OK] 1D-CNN model loaded")
    else:
        print("  [INFO] PyTorch not installed - using Random Forest model")

    return rf_model, cnn_model


def predict_random_forest(model, X_raw):
    """Make predictions using Random Forest"""
    X_feat = extract_features_batch(X_raw)
    predictions = model.predict(X_feat)
    probabilities = model.predict_proba(X_feat)
    return predictions, probabilities, X_feat


def predict_cnn(model, X_raw):
    """Make predictions using 1D-CNN"""
    if model is None or not HAS_TORCH:
        return None, None
    X_tensor = torch.tensor(X_raw, dtype=torch.float32)
    with torch.no_grad():
        logits = model(X_tensor)
        probs = torch.softmax(logits, dim=1).numpy()
        predictions = logits.argmax(dim=1).numpy()
    return predictions, probs


def print_predictions(X, y, rf_pred, rf_probs, cnn_pred, cnn_probs, features):
    """Pretty-print predictions"""
    print("\n[3/4] Predictions on Demo Samples")
    print("=" * 80)

    for i, true_label in enumerate(y):
        true_class = CLASSES[true_label]

        print(f"\n{'-' * 80}")
        print(f"SAMPLE {i+1}: Ground Truth = {true_class.upper()}")
        print(f"{'-' * 80}")

        print("\nExtracted Features:")
        for fname, fval in zip(FEATURE_NAMES, features[i]):
            print(f"  * {fname:15s}: {fval:8.4f}")

        rf_class = CLASSES[rf_pred[i]]
        rf_conf = rf_probs[i][rf_pred[i]] * 100
        match_rf = "[OK] CORRECT" if rf_pred[i] == true_label else "[FAIL] WRONG"

        print(f"\nRandom Forest:")
        print(f"  Prediction: {rf_class}")
        print(f"  Confidence: {rf_conf:.1f}%")
        print(f"  Status:     {match_rf}")
        print(f"  Class Probabilities:")
        for cls_idx, cls_name in enumerate(CLASSES):
            prob = rf_probs[i][cls_idx] * 100
            bar = "#" * int(prob / 5) + "." * (20 - int(prob / 5))
            print(f"    {cls_name:12s} {prob:5.1f}% [{bar}]")

        if cnn_pred is not None and cnn_probs is not None:
            cnn_class = CLASSES[cnn_pred[i]]
            cnn_conf = cnn_probs[i][cnn_pred[i]] * 100
            match_cnn = "[OK] CORRECT" if cnn_pred[i] == true_label else "[FAIL] WRONG"

            print(f"\n1D-CNN:")
            print(f"  Prediction: {cnn_class}")
            print(f"  Confidence: {cnn_conf:.1f}%")
            print(f"  Status:     {match_cnn}")
            print(f"  Class Probabilities:")
            for cls_idx, cls_name in enumerate(CLASSES):
                prob = cnn_probs[i][cls_idx] * 100
                bar = "#" * int(prob / 5) + "." * (20 - int(prob / 5))
                print(f"    {cls_name:12s} {prob:5.1f}% [{bar}]")

            agreement = "[OK] AGREE" if rf_pred[i] == cnn_pred[i] else "[!] DISAGREE"
            print(f"\n  Model Agreement: {agreement}")


def main():
    rf_model, cnn_model = load_models()
    print("\n[2/4] Generating demo samples (one per class)...")
    X, y = generate_demo_samples()
    print(f"  [OK] Generated {len(y)} demo samples")
    print(f"  Classes: {[CLASSES[label] for label in y]}")

    print("\n[3/4] Making predictions...")
    rf_pred, rf_probs, features = predict_random_forest(rf_model, X)
    print("  [OK] Random Forest predictions complete")

    cnn_pred, cnn_probs = predict_cnn(cnn_model, X)
    if cnn_pred is not None:
        print("  [OK] CNN predictions complete")

    print_predictions(X, y, rf_pred, rf_probs, cnn_pred, cnn_probs, features)


if __name__ == "__main__":
    main()
