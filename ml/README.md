# TDR fault classifier, synthetic-to-real pipeline

## What is in here

- `synthetic_data.py`, generates fake but physically-reasonable TDR reflection
  windows for three classes: `healthy`, `soft_fault`, `hard_fault`. Run it
  directly to produce `tdr_synthetic.csv`.
- `features.py`, turns one raw window into six hand-picked numbers (peak size,
  peak position, polarity, rise time, energy, baseline noise). Each one has a
  physical meaning you can explain out loud.
- `train_classifier.py`, the primary model. A Random Forest trained on those
  six features. This was actually run in testing: **92% accuracy** on held-out
  synthetic data, with most confusion between `soft_fault` and its neighbours,
  which is expected, that is the genuinely hard boundary case.
- `train_cnn.py`, the stretch-goal model. A small 1D-CNN trained directly on
  the raw waveform, following the same idea as Scarpetta et al. (2021,
  *Sensors*). Needs `pip install torch`. Only reach for this if the six
  hand-picked features turn out not to separate soft faults from noise well
  enough once real data comes in, per our earlier discussion about why a CNN
  is not the default choice for a small, self-collected dataset.

## Why the accuracy is not close to 100%

An earlier version of the synthetic generator kept the three classes fully
separated in amplitude, and the classifier scored 100%. That is a sign the
fake data was too easy, not a sign the model is good, so the ranges were
overlapped on purpose (a very early soft fault genuinely looks similar to a
slightly noisy healthy cable). The 92% result is the more honest number to
quote, and matches the real physical difficulty of the problem.

## How to swap in real data once the probe is ready

Once the STM32 + Schmitt trigger probe is capturing real reflections:

1. Save each real capture as one row: `label, sample_0, sample_1, ..., sample_N`,
   using `0 = healthy`, `1 = soft_fault`, `2 = hard_fault`, exactly the format
   `synthetic_data.save_csv()` writes to `tdr_synthetic.csv`.
2. In `train_classifier.py`, replace the line

   ```python
   X, y = load_synthetic(n_per_class=400)
   ```

   with

   ```python
   X, y = load_real_csv("tdr_real.csv")
   ```

   Nothing else in the file needs to change, feature extraction, the model,
   and the evaluation code all stay exactly the same.
3. Expect the accuracy to drop compared to synthetic data. That is normal
   and worth saying plainly in the review rather than hiding it, real noise,
   real cable variation, and a much smaller dataset all make the problem
   genuinely harder than the synthetic version.

## Quick start

```bash
pip install numpy scikit-learn joblib
python3 synthetic_data.py       # writes tdr_synthetic.csv
python3 train_classifier.py     # trains, evaluates, saves tdr_classifier.joblib
```
