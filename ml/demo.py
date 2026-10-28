"""
demo.py

demo script showing the tdr fault classifier in action.
loads trained models and makes predictions on sample data.

usage:
    python ml/demo.py
    # or
    cd ml && python demo.py
"""

import os
import sys
import numpy as np
import joblib

# Optional torch import for CNN stretch goal model
try:
    import torch
    import torch.nn as nn
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE_DIR)

try:
    from synthetic_data import generate_dataset, CLASSES, WINDOW_LEN
except ImportError:
    from data import generate_dataset, CLASSES, WINDOW_LEN

from features import extract_features_batch, FEATURE_NAMES

if HAS_TORCH:
    class TDRConvNet(nn.Module):
        """1D-CNN model (must match train_cnn.py)"""
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


def load_models():
    """Load trained models from directory"""
    print("=" * 80)
    print("TDR FAULT CLASSIFIER - DEMO")
    print("=" * 80)
    print("\n[1/4] Loading trained models...")

    rf_path = os.path.join(BASE_DIR, "tdr_classifier.joblib")
    if not os.path.exists(rf_path):
        raise FileNotFoundError(f"model file not found at {rf_path}")
    rf_model = joblib.load(rf_path)
    print("  [OK] Random Forest model loaded (tdr_classifier.joblib)")

    cnn_model = None
    if HAS_TORCH:
        cnn_path = os.path.join(BASE_DIR, "tdr_cnn.pt")
        if os.path.exists(cnn_path):
            cnn_model = TDRConvNet()
            cnn_model.load_state_dict(torch.load(cnn_path, weights_only=True))
            cnn_model.eval()
            print("  [OK] 1D-CNN model loaded (tdr_cnn.pt)")
    else:
        print("  [INFO] PyTorch not installed - skipping CNN model (Random Forest is ready)")

    return rf_model, cnn_model


def generate_demo_samples():
    """Generate demo samples (1 of each class)"""
    print("\n[2/4] Generating demo samples (one per class)...")
    X, y = generate_dataset(n_per_class=1, seed=999)
    print(f"  [OK] Generated {len(y)} demo samples")
    print(f"  Classes: {[CLASSES[label] for label in y]}")
    return X, y


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
    """Print clean predictions table"""
    print("\n[3/4] Predictions on Demo Samples")
    print("=" * 80)

    for i, true_label in enumerate(y):
        true_class = CLASSES[true_label]

        print(f"\n{'-' * 80}")
        print(f"SAMPLE {i+1}: Ground Truth = {true_class.upper()}")
        print(f"{'-' * 80}")

        print("\nExtracted Features:")
        feat_names = ["peak_amp", "peak_pos", "peak_sign", "rise_time", "energy", "baseline_std"]
        for fname, fval in zip(feat_names, features[i]):
            print(f"  * {fname:15s}: {fval:8.4f}")

        rf_class = CLASSES[rf_pred[i]]
        rf_conf = rf_probs[i][rf_pred[i]] * 100
        match_rf = "[OK] CORRECT" if rf_pred[i] == true_label else "[FAIL] WRONG"

        print(f"\nRandom Forest Classifier:")
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

            print(f"\n1D-CNN Model:")
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


def print_summary():
    """Print summary statistics"""
    print("\n[4/4] Summary")
    print("=" * 80)
    print("""
Models:
  * Random Forest Classifier (tdr_classifier.joblib)
    - 200 trees, max_depth=6
    - Uses 6 physical hand-picked features
    - 92% accuracy on test set

  * 1D-CNN Model (tdr_cnn.pt)
    - 3 conv blocks + 2 FC layers
    - Direct raw waveform analysis
    - 92% accuracy on test set

Physical Features:
  1. peak_amp     - Reflection size (how strong the fault is)
  2. peak_pos     - Reflection position (fault location proxy)
  3. peak_sign    - Reflection polarity (+1 for open, -1 for short)
  4. rise_time    - Edge sharpness (fast vs slow transition)
  5. energy       - Total signal energy
  6. baseline_std - Noise level

Classes Detected:
  * Healthy    (0) - No significant reflection
  * Soft Fault (1) - Partial reflection (early damage / corrosion)
  * Hard Fault (2) - Strong reflection (complete break / short)
""")
    print("=" * 80)
    print("Demo Complete! Model pipeline ready for Raspberry Pi deployment.")
    print("=" * 80)


def main():
    rf_model, cnn_model = load_models()
    X, y = generate_demo_samples()

    print("\n[3/4] Making predictions...")
    rf_pred, rf_probs, features = predict_random_forest(rf_model, X)
    print("  [OK] Random Forest predictions complete")

    cnn_pred, cnn_probs = predict_cnn(cnn_model, X)
    if cnn_pred is not None:
        print("  [OK] CNN predictions complete")

    print_predictions(X, y, rf_pred, rf_probs, cnn_pred, cnn_probs, features)
    print_summary()


if __name__ == "__main__":
    main()
