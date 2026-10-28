"""
train_classifier.py

Trains a Random Forest classifier on TDR windows using the hand-picked
features from features.py. Works on the synthetic data now, and on real
captures later without changing any of this code, only the data loading
line needs to point at a different CSV (see load_real_csv below).

Usage:
    python ml/train_classifier.py
"""

import os
import sys
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
import joblib

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE_DIR)

try:
    from synthetic_data import generate_dataset, CLASSES, WINDOW_LEN
except ImportError:
    from data import generate_dataset, CLASSES, WINDOW_LEN

from features import extract_features_batch, FEATURE_NAMES


def load_synthetic(n_per_class=400, seed=0):
    X_raw, y = generate_dataset(n_per_class=n_per_class, seed=seed)
    X_feat = extract_features_batch(X_raw)
    return X_feat, y


def load_real_csv(path):
    data = np.loadtxt(path, delimiter=",", skiprows=1)
    y = data[:, 0].astype(np.int64)
    X_raw = data[:, 1:]
    X_feat = extract_features_batch(X_raw)
    return X_feat, y


def main():
    print("Loading dataset...")
    X, y = load_synthetic(n_per_class=400)

    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.25, random_state=42, stratify=y
    )

    print("Training Random Forest classifier (200 trees)...")
    clf = RandomForestClassifier(
        n_estimators=200,
        max_depth=6,
        random_state=42,
    )
    clf.fit(X_train, y_train)

    y_pred = clf.predict(X_test)

    print("\nFeature importances:")
    for name, imp in sorted(zip(FEATURE_NAMES, clf.feature_importances_), key=lambda t: -t[1]):
        print(f"  {name:12s} {imp:.3f}")
    print()

    print("Classification report (test set):")
    print(classification_report(y_test, y_pred, target_names=CLASSES))

    print("Confusion matrix (rows=true, cols=predicted):")
    print(CLASSES)
    print(confusion_matrix(y_test, y_pred))

    out_path = os.path.join(BASE_DIR, "tdr_classifier.joblib")
    joblib.dump(clf, out_path)
    print(f"\nSaved trained model to {out_path}")


if __name__ == "__main__":
    main()
