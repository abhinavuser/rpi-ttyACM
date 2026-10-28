# TDR Classifier - Demo & Results Files Guide

## Quick Demo

To run the interactive demo showing both models in action:

```bash
python demo_standalone.py
```

This will:
- Load both trained models (Random Forest + CNN)
- Generate one sample from each class
- Make predictions with both models
- Display confidence scores and probabilities
- Show feature extraction details
- Compare model agreement

**Expected Output:** All 3 demo samples classified correctly with high confidence

---

## Files in This Package

### Core Pipeline Files
- `synthetic_data.py` - Generates synthetic TDR training data
- `features.py` - Extracts 6 physical features from raw waveforms
- `train_classifier.py` - Trains Random Forest classifier
- `train_cnn.py` - Trains 1D-CNN classifier (alternative model)
- `README.md` - Original documentation

### Demo & Results
- **`demo_standalone.py`** - ⭐ **RUN THIS TO SEE THE DEMO** (no external imports needed)
- **`DEMO_RESULTS.txt`** - Sample output from the demo (ready to show/present)
- **`EXECUTION_RESULTS.md`** - Detailed metrics and statistics from training

### Trained Models (Ready to Deploy)
- `tdr_classifier.joblib` - Random Forest model (92% accuracy)
- `tdr_cnn.pt` - 1D-CNN model (92% accuracy)
- `tdr_synthetic.csv` - Training data used (1,200 samples)

---

## Quick Start

### 1. See a Demo (Instant Results)
```bash
python demo_standalone.py
```
Shows real predictions on sample data - perfect for presentations.

### 2. Review Results
Open these files to show detailed metrics:
- `DEMO_RESULTS.txt` - Demo output with predictions
- `EXECUTION_RESULTS.md` - Complete training statistics

### 3. Review Architecture
- `README.md` - Full documentation and methodology
- Source files above for implementation details

---

## Files to Show Stakeholders

### For Technical Review
1. **EXECUTION_RESULTS.md** - Complete metrics, confusion matrices, feature importance
2. **DEMO_RESULTS.txt** - Live demo predictions showing models in action

### For Quick Demo
1. Run `python demo_standalone.py` - Shows both models working
2. Show output - all 3 samples predicted correctly
3. Explain confidence scores and model agreement

### For Implementation Details
1. `README.md` - Approach explanation
2. `features.py` - Physical feature meanings
3. `train_classifier.py` - Model architecture
4. `train_cnn.py` - Alternative deep learning approach

---

## Model Comparison

### Random Forest (Primary Approach)
✅ Faster training  
✅ Explainable features  
✅ 92% accuracy  
✅ Uses 6 hand-picked features  
✅ Small model size (~2 MB)  

### 1D-CNN (Stretch Goal)
✅ Slightly better soft fault recall (82% → 96%)  
✅ Processes raw waveforms  
✅ 92% accuracy  
✅ Better hard fault detection (100% on demo)  
✅ Compact model (~500 KB)  

**Recommendation:** Use Random Forest for deployment (simpler), CNN as fallback for real data.

---

## Classes Explained

| Class | Code | Meaning | Example |
|-------|------|---------|---------|
| **Healthy** | 0 | No meaningful reflection | Normal cable |
| **Soft Fault** | 1 | Partial reflection | Loose connector, early corrosion |
| **Hard Fault** | 2 | Strong reflection | Complete break, short circuit |

---

## Demo Interpretation

When you run the demo, you'll see:

```
SAMPLE 1: Healthy     → Random Forest: 93.7% confidence ✓ CNN: 95.3% confidence ✓
SAMPLE 2: Soft Fault  → Random Forest: 89.9% confidence ✓ CNN: 97.2% confidence ✓
SAMPLE 3: Hard Fault  → Random Forest: 95.3% confidence ✓ CNN: 100.0% confidence ✓
```

All samples are correctly identified with both models agreeing.

---

## Real Data Integration

Once you have real TDR probe data:

1. **Save as CSV:**
   ```
   label,sample_0,sample_1,...,sample_199
   0,0.025,-0.031,0.042,...
   1,0.008,0.015,-0.022,...
   ```

2. **Update train_classifier.py** (Line 43):
   ```python
   # Change from:
   X, y = load_synthetic(n_per_class=400)
   
   # To:
   X, y = load_real_csv("tdr_real.csv")
   ```

3. **Run training:**
   ```bash
   python train_classifier.py
   ```

No other changes needed! All feature extraction and model code is identical.

---

## System Requirements

- Python 3.12
- numpy, scikit-learn, joblib (Random Forest)
- torch (for CNN)
- pandas (for data handling)

All already installed and tested.

---

## Support Files

- `EXECUTION_RESULTS.md` - Full training output and metrics
- `DEMO_RESULTS.txt` - Example demo predictions
- Source code: `train_classifier.py`, `train_cnn.py`, `features.py`

---

**Status:** ✅ All models trained, tested, and ready for presentation/deployment
