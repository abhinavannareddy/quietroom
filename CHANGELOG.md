# Changelog

All notable changes are listed here. The format follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

## [0.1.0]

First complete version.

### Added
- `quietroom::Pipeline`: 160-sample hops at 16 kHz, 50% overlap-add with a sqrt-Hann window,
  512-point radix-2 FFT, VAD, noise estimation and spectral subtraction. `process_frame()` does
  not allocate (checked by a test that replaces `operator new`).
- `quietroom_cli in.wav out.wav`: prints per-frame timing and VAD statistics; optional per-frame
  VAD CSV and JSON stats.
- `Config` struct for the tunable parameters; float or double samples at compile time.
- Google Benchmark target reporting mean, p50, p99, max, deadline misses and real-time factor.
- `python/evaluate.py`: mixes LibriSpeech clips with white, pink, hum and babble noise at 0, 5
  and 10 dB, and produces the results tables and plots.
- CI on Linux and macOS (Release and Debug), an ASan/UBSan job and a lint job.
