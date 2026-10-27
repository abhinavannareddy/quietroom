# Design notes

## Data flow

![pipeline](images/pipeline.png)

`Pipeline::process_frame(in, out)` takes one 160-sample hop and writes one 160-sample hop. All
state lives in the `Pipeline` object, and every buffer is a `std::array` sized at compile time,
so the constructor is the only place that allocates (and it doesn't even need to: `sizeof(Pipeline)` is 15,424 bytes in the
float build, so it can live on the stack).

| File | Stage |
|---|---|
| `framing.hpp/.cpp` | `OverlapAdd`: windowing the previous + current hop, and overlap-add on the way out |
| `fft.hpp/.cpp` | 512-point radix-2 FFT, twiddles and bit-reversal table built once |
| `vad.hpp/.cpp` | frame features (energy, zero-crossing rate, spectral flatness) and the decision with hangover |
| `noise_estimator.hpp/.cpp` | noise power spectrum |
| `spectral_sub.hpp/.cpp` | per-bin gain with floor and temporal smoothing |
| `pipeline.hpp/.cpp` | glues the stages together |
| `wav.hpp/.cpp` | 16 kHz mono WAV in/out (used by the CLI, not by the frame path) |

## Why the output is one hop late

The synthesis half of an overlap-add can only be completed once the next window has arrived, so
the output for hop *k* is available when hop *k+1* has been processed. `process_frame` therefore
returns the input delayed by exactly one hop (160 samples). Counting from when a sample arrives to
when it can be played, that is 10 to 20 ms, so the worst case is the 20 ms analysis window. The CLI
drops the first output hop and flushes one extra zero hop so the output WAV lines up with the input.

## Things that are deliberate

- **Complex FFT for real input.** A real-input FFT would be about twice as fast. The measured
  cost per frame is a small fraction of the budget, so the simpler code stays.
- **Gain applied to a mirrored spectrum.** The 512-point transform is complex, so the gain for
  bin `k` is also applied to bin `512-k`, which keeps the inverse transform real.
- **The VAD sees the current hop, the spectrum covers two.** Energy and zero-crossing rate are
  computed on the newest 10 ms (low latency), flatness on the 20 ms window power spectrum.
- **The first `noise_init_frames` frames are never speech.** The noise estimate needs a starting
  point; the PRD says to assume the first ~200 ms are noise. A clip that starts with speech will
  therefore have that speech partly treated as noise. Test clips start with silence.
- **`bypass` still runs the VAD and noise estimator.** It only skips the gain, so the reconstruction
  test exercises the real FFT / window / overlap-add path.

## Tests

| Test file | What it protects |
|---|---|
| `test_framing.cpp` | windows satisfy `w[n]^2 + w[n+160]^2 = 1`; passthrough reproduces the input one hop late |
| `test_fft.cpp` | round trip under 1e-5, a tone lands in the right bin, agreement with a naive DFT |
| `test_vad.cpp` | feature values on known signals, burst detection, hangover length, floor adaptation |
| `test_noise_and_subtraction.cpp` | convergence, freezing during speech, gain formula, smoothing, config effect |
| `test_pipeline.cpp` | bypass reconstruction, SNR improvement on a synthetic clip, config changes output |
| `test_no_alloc.cpp` | `process_frame()` performs zero heap allocations (own executable: it replaces `operator new`) |
| `test_wav.cpp` | round trip, clipping, malformed and lying headers |
| `test_cli_regression.cpp` | runs the real CLI on `data/regression/noisy_white_5db.wav` and checks a loose SNR-improvement floor and the exit code on a missing file |
