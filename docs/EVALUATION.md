# Evaluation method

`python/evaluate.py` runs the real `quietroom_cli` on every combination of 6 speech clips,
4 noise types and 3 input SNRs (72 runs) and computes everything from the output files. The
tables are in `docs/results/results.md`, the raw per-run rows in `docs/results/results.csv`.

## Inputs

- **Speech**: six LibriSpeech utterances, 2 from each of 3 speakers (see `data/LICENSES.md`).
  Each has 0.5 s of digital silence in front so the noise estimate can start up.
- **Noise**: white, pink (1/f power), 50 Hz hum with four harmonics, and babble (six overlapped
  utterances). White, pink and hum are generated from a fixed seed. Babble comes from a different set
  of utterances than the test speech.
- **Mixing**: noise is scaled so that `10 log10(E_clean / E_noise)` over the whole file equals
  the requested SNR (0, 5 or 10 dB). The mix is clipped to the 16-bit range, as the WAV file is.

## Metrics

| Metric | Definition |
|---|---|
| SNR improvement | output SNR minus input SNR, both against the clean clip, whole file |
| Noise reduction | `10 log10(E_in / E_out)` over frames labelled non-speech |
| Speech-region SNR | `10 log10(E[clean] / E[out - clean])` over frames labelled speech, input and output |
| VAD accuracy, confusion matrix | CLI per-frame decision against the labels below |
| False-silence rate | `FN / (TP + FN)`: speech frames the VAD called non-speech |

"Speech distortion" in the PRD is covered by the speech-region SNR. It includes the residual
noise in those frames, so it overstates pure distortion, but spectral subtraction is not linear
and the speech component of the output cannot be separated exactly.

## Reference labels

A 10 ms frame is labelled speech when its energy in the clean clip is within 35 dB of the loudest
frame. This is a convenient proxy, not a human annotation: it calls quiet fricatives and breaths
speech or not depending on the threshold. VAD numbers are only meaningful relative to this
definition.

## Tuning without fooling ourselves

`python/tune.py` sweeps over-subtraction, spectral floor and gain smoothing on the first clip of
each speaker and then scores the winner on the other three clips, which the sweep never saw. The
defaults in `Config` come from that run. The objective is SNR improvement, which rewards
aggressive subtraction. It does not measure musical noise, and nobody has done listening tests on
these settings, so the defaults lean toward noise removal more than a listener might like.

## What is not measured

- STOI/PESQ (optional in the PRD, not done).
- Non-stationary noise (horns, keyboard), which is a stated non-goal.
- Timing on other machines. CI uploads benchmark JSON from the runners as an artifact so it can
  be compared, but CI never fails on it.
