"""
train_cnn.py

STRETCH GOAL, 1D-CNN classifier option.
Trains a small 1D-CNN over raw TDR waveforms directly in PyTorch.

Usage:
    python ml/train_cnn.py
"""

import os
import sys
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import TensorDataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE_DIR)

try:
    from synthetic_data import generate_dataset, CLASSES, WINDOW_LEN
except ImportError:
    from data import generate_dataset, CLASSES, WINDOW_LEN


class TDRConvNet(nn.Module):
    """Small 1D-CNN over the raw waveform. Three conv blocks, then a
    couple of fully connected layers down to 3 class scores."""

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


def main():
    print("Loading synthetic TDR dataset...")
    X, y = generate_dataset(n_per_class=400)
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.25, random_state=42, stratify=y
    )

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")

    train_ds = TensorDataset(
        torch.tensor(X_train, dtype=torch.float32),
        torch.tensor(y_train, dtype=torch.long),
    )
    test_ds = TensorDataset(
        torch.tensor(X_test, dtype=torch.float32),
        torch.tensor(y_test, dtype=torch.long),
    )
    train_loader = DataLoader(train_ds, batch_size=32, shuffle=True)
    test_loader = DataLoader(test_ds, batch_size=64)

    model = TDRConvNet().to(device)
    optim = torch.optim.Adam(model.parameters(), lr=1e-3)
    loss_fn = nn.CrossEntropyLoss()

    n_epochs = 25
    print("Training 1D-CNN...")
    for epoch in range(n_epochs):
        model.train()
        total_loss = 0.0
        for xb, yb in train_loader:
            xb, yb = xb.to(device), yb.to(device)
            optim.zero_grad()
            out = model(xb)
            loss = loss_fn(out, yb)
            loss.backward()
            optim.step()
            total_loss += loss.item() * xb.size(0)
        if (epoch + 1) % 5 == 0:
            print(f"epoch {epoch+1}/{n_epochs}  loss={total_loss/len(train_ds):.4f}")

    model.eval()
    all_preds, all_true = [], []
    with torch.no_grad():
        for xb, yb in test_loader:
            xb = xb.to(device)
            preds = model(xb).argmax(dim=1).cpu().numpy()
            all_preds.extend(preds)
            all_true.extend(yb.numpy())

    print("\nClassification report (test set):")
    print(classification_report(all_true, all_preds, target_names=CLASSES))
    print("Confusion matrix:")
    print(CLASSES)
    print(confusion_matrix(all_true, all_preds))

    out_path = os.path.join(BASE_DIR, "tdr_cnn.pt")
    torch.save(model.state_dict(), out_path)
    print(f"\nSaved trained model to {out_path}")


if __name__ == "__main__":
    main()
