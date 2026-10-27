# QuietRoom

[![CI](https://github.com/abhinavannareddy/quietroom/actions/workflows/ci.yml/badge.svg)](https://github.com/abhinavannareddy/quietroom/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://en.cppreference.com/w/cpp/17)

If an audio callback takes longer than the audio it was handed, the next buffer arrives before
the last one is done and the sound breaks up. QuietRoom is a small C++17 library and command-line
tool that removes steady background noise from 16 kHz speech in 10 ms frames, and measures whether
it stays inside that deadline. A voice activity detector decides when only noise is present,
the noise spectrum is learned from those moments, and spectral subtraction takes it out. It does
not allocate memory while processing, and every number below comes from a run on one laptop.

## Results

All figures are from the committed runs in [`docs/results/`](docs/results/); the method is in
[`docs/EVALUATION.md`](docs/EVALUATION.md). Machine: AMD Ryzen 7 7730U, Windows 11, clang 21.1.0
(through Zig 0.16), `-O3 -DNDEBUG`, float samples.

**Time per frame** (`process_frame()`, one 10 ms hop, wall clock, monotonic timer)

| | CLI over the 72 evaluation runs (37,908 frames) | Google Benchmark, 5 repetitions (389,585 frames) |
|---|---|---|
| mean | 13.1 µs | 10.1 µs (median of repetitions) |
| p50 | 9.5 µs | 8.3 µs |
| p99 | 44.0 µs | 20.7 µs |
| max | 2,188 µs | 6,412 µs (worst repetition) |
| frames over 10 ms | 0 | 0 |
| frames over 2 ms | 1 | 16 |
| real-time factor | 0.0013 | about 0.001 |

**Noise removal: SNR improvement in dB** (mean over 6 LibriSpeech clips; output SNR minus input SNR)

| noise | 0 dB in | 5 dB in | 10 dB in |
|---|---|---|---|
| white | 7.15 | 7.39 | 7.67 |
| pink | 6.40 | 6.11 | 5.84 |
| 50 Hz hum | 8.21 | 8.42 | 7.51 |
| babble | 3.22 | 2.83 | 2.72 |

**Voice activity detection** against labels derived from the clean clips (see
[Limitations](#limitations); these are not human annotations)

| input SNR | accuracy | false-silence rate | false-speech rate |
|---|---|---|---|
| 0 dB | 66.8% | 33.6% | 32.6% |
| 5 dB | 82.5% | 11.8% | 25.5% |
| 10 dB | 86.0% | 5.9% | 25.3% |
| all runs | 78.4% | 17.1% | 27.8% |

**Against the PRD targets**

| target | result |
|---|---|
| mean under 1 ms per frame | met: about 0.01 ms |
| p99 under 2 ms | met: 21 to 80 µs across the repetitions and runs |
| no frame over 10 ms | met: 0 of 427,493 timed frames |
| frames over 2 ms: report, target 0 | **missed**: 17 of 427,493 (0.004%), the worst at 6.4 ms |
| real-time factor under 0.1 | met: about 0.001 |
| at least 6 dB SNR improvement at 5 dB input | met for white (7.39), pink (6.11) and hum (8.42); **missed for babble (2.83)** |
| VAD accuracy with a confusion matrix | reported above and in the figure below |
| zero allocations in `process_frame()` | met, enforced by a test |

The 2 ms misses are single frames that took far longer than the thousands around them, which
points at the operating system pre-empting the thread on a laptop that was also running other
work. I did not trace them further. Timings changed from run to run (the CLI runs recorded
maxima between 0.2 and 2.2 ms), so treat the maximum as a property of the machine at that moment,
not of the algorithm.

![Per-frame timing](docs/images/timing_histogram.png)

![SNR improvement](docs/images/snr_improvement.png)

![VAD confusion matrix](docs/images/vad_confusion.png)

Before and after, for one clip with pink noise at 5 dB. The output spectrogram still shows
isolated blobs between words: that is the musical noise discussed under Limitations.

![Spectrograms](docs/images/spectrograms.png)

![Waveforms](docs/images/waveforms.png)

## Quick start

You need CMake 3.16 or newer and a C++17 compiler. Google Test and Google Benchmark are
downloaded by CMake (pinned by hash) the first time you configure, so the first configure needs
network access.

**Linux and macOS**

```bash
git clone https://github.com/abhinavannareddy/quietroom.git
cd quietroom
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/quietroom_cli data/regression/noisy_white_5db.wav cleaned.wav
```

**Windows** (PowerShell, from a Visual Studio Developer prompt)

```powershell
git clone https://github.com/abhinavannareddy/quietroom.git
cd quietroom
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\quietroom_cli.exe data\regression\noisy_white_5db.wav cleaned.wav
```

With Ninja (`-G Ninja`) the executable is `build\quietroom_cli.exe` instead.

I built and tested on Windows with clang 21 through Zig and Ninja, and on Linux with GCC 13.3
(Release, Debug, ASan+UBSan and the double-precision build, in a clean checkout). The MSVC and
MinGW builds and macOS have not been run by me; the CI workflow covers macOS once it has run on
GitHub.

The CLI prints timing and VAD statistics:

```text
frames: 393 (3.93 s)
compute per frame: mean 10.5 us, p50 10.4 us, p99 11.2 us, max 40.6 us
deadline misses: 0 over 10 ms, 0 over 2 ms; real-time factor 0.00105
VAD: 161 speech frames, 232 non-speech frames
```

Options (`quietroom_cli --help` lists them): `--over-subtraction`, `--floor`, `--smoothing`,
`--hangover`, `--bypass`, `--vad-csv FILE`, `--json FILE`. Input must be 16 kHz mono, 16-bit PCM or
32-bit float WAV, at most 256 MiB.

**As a library**

```cpp
#include "quietroom/pipeline.hpp"

quietroom::Config config;          // defaults are tuned, see Config for every parameter
quietroom::Pipeline pipeline(config);

// per 10 ms hop (160 samples of quietroom::Sample, float by default):
pipeline.process_frame(in, out);   // out is the input delayed by one hop
```

Link against the `quietroom` CMake target. Configure with `-DQUIETROOM_DOUBLE=ON` for a double
build. Out-of-range `Config` values are clamped, and NaN or infinite input samples are treated as
silence.

## Reproduce every result

```bash
python -m pip install -r python/requirements.txt
scripts/reproduce.sh            # Linux, macOS, Git Bash
```

On Windows, `scripts\reproduce.ps1` does the same from PowerShell. I have not run that script
myself, because my shell was not allowed to run PowerShell scripts, so treat it as untested. The Bash
script rebuilds in Release, runs the tests, runs the benchmark into `docs/results/bench.json`,
then runs `python/evaluate.py`, which mixes the clips with noise, calls the CLI and rewrites
`docs/results/` and every PNG in `docs/images/`. Your timings will differ from mine.

## How it works

![Pipeline](docs/images/pipeline.png)

Each call takes one hop of 160 samples (10 ms at 16 kHz) and runs five steps. Defaults are in
`include/quietroom/config.hpp`.

**1. Window and transform.** The previous hop and the current hop are joined into 320 samples and
multiplied by a square-root Hann window, then zero-padded to 512 points for the FFT.

```text
w[n] = sqrt( 0.5 * (1 - cos(2*pi*n / 320)) ),   n = 0..319
```

The same window is applied again before the overlap-add. Because `w[n]^2 + w[n+160]^2 = 1`, a
signal that goes through unchanged comes back exactly, which a test checks to within 1e-5. The FFT
is a hand-written radix-2 transform with its twiddles built once; for a fixed 512 points it is
a 62-line source file, needs no dependency and cannot allocate.

**2. Voice activity detection.** Three features of the newest hop and the 20 ms spectrum:

```text
E  = 10*log10( mean(x^2) + 1e-10 )                       frame energy in dB
Z  = sign changes / 159                                   zero-crossing rate
F  = geometric mean(P[4..127]) / arithmetic mean(P[4..127])   spectral flatness
```

`E` is compared with a floor `f` that falls quickly and rises slowly: if `E < f` then
`f += 0.2 * (E - f)`, otherwise `f += 0.02 dB` per frame. A frame is speech if `E > f + 12 dB`, or
if `E > f + 6 dB` and it does not look like noise. "Looks like noise" means `F > 0.5` and `Z > 0.3`:
flat in frequency and busy in time. Without that rejection, loud broadband hiss would count as
speech. If the decision has been "speech" for 3 s without a break, the floor is almost
certainly too low (a stream that started with exact zeros does this), so it climbs at 0.5 dB per
frame until the decision settles.

**Hangover.** After the last speech frame the detector keeps saying "speech" for 8 more frames
(80 ms), so word endings are not clipped and the noise estimate does not learn from them.

**3. Noise estimate.** The first 20 frames (200 ms) are assumed to be noise and averaged. After
that the estimate `N` is updated only on frames the detector calls non-speech:

```text
N = 0.95 * N + 0.05 * P
```

During speech it is frozen. During the first 200 ms the detector's answer is ignored.

**4. Spectral subtraction.** For each of the 257 bins, with `P` the power of the noisy frame and
`N` the noise estimate:

```text
S      = max( P - alpha*N,  beta*P )          alpha = 3.0 (over-subtraction), beta = 0.02 (floor)
G_raw  = sqrt( S / P )                         amplitude gain, between sqrt(beta) and 1
G[t]   = s*G[t-1] + (1 - s)*G_raw              s = 0.3 (smoothing across time)
```

Subtracting more than the average noise (`alpha > 1`) removes more of it, because the noise power in
any one frame fluctuates around its mean; it also eats into speech. The floor stops bins going to
zero, which is what produces the isolated tones called musical noise. Smoothing the gain over time
reduces that further, at the cost of slower reaction. The gain is applied to the complex spectrum
(and mirrored onto the negative-frequency bins so the result stays real).

**5. Inverse FFT and overlap-add.** The inverse transform, the second window, and the sum with the
tail of the previous frame produce one finished hop.

## Latency budget

Two different numbers are easy to mix up here.

**Compute time per frame** is how long `process_frame()` runs. Above: about 10 µs typical, against
the 10,000 µs a hop is allowed. That leaves a margin of roughly a thousand, which is why occasional
operating-system hiccups on a laptop show up as rare outliers but not as a trend.

**Algorithmic latency** is how long a sample waits before it comes out, and it does not depend on
how fast the code is. The overlap-add needs the next window before a hop is finished, so the output
is the input delayed by exactly one hop (10 ms). Counted from the moment a sample arrives to the
moment it can be played, that is between 10 and 20 ms, the worst case being the full 20 ms analysis
window. This is fixed by the design; I stated it and tested the one-hop delay, but did not
benchmark it. The CLI removes the delay so the output file lines up with the input.

## Tests

39 tests, run by `ctest`:

- framing, FFT, VAD, noise estimator, spectral subtraction, the pipeline, WAV parsing, and a regression test
  that runs the real CLI on a fixed noisy clip;
- `test_no_alloc` replaces `operator new` and checks that 3 s of audio through `process_frame()`
  allocates nothing (it is its own executable because of the global replacement);
- hostile inputs: NaN and infinite samples, lying chunk sizes, wrong sample rate, unsupported WAV formats.

CI (`.github/workflows/ci.yml`) builds Linux and macOS in Release and Debug, runs an ASan+UBSan job
where UBSan aborts on a finding, builds the double-precision option, and checks formatting and
clang-tidy. It uploads the benchmark JSON as an artifact and never fails on timing.

## What went wrong along the way

The first defaults gave 5.9 dB on white noise, 4.7 on pink and 2.5 on babble at 5 dB input, just under
the target. A sweep over the three subtraction parameters on three clips, scored on the other
three, took the held-out average over the four noise types from 4.6 to 5.4 dB at the time, and became the
defaults. The cost is that a lower floor and lighter smoothing favour noise removal over listening
comfort, and I have not listened to the result (see Limitations).

Linux caught two mistakes in my allocation test that Windows did not. GCC deleted the
"does my counter work" check because a `new`/`delete` pair with an unused result may be removed; and
under AddressSanitizer I had replaced only some `operator new` variants, so memory from the
library's `nothrow` variant was freed by my `free()`. The test now replaces the whole family
and keeps the allocation observable.

A round-trip test found that my WAV writer scaled by 32767 while the reader divided by 32768, which
shifts values near full scale by more than one quantisation step. A review pass found that one NaN
sample would have poisoned every later frame (the smoothing filters are recursive and `0.3 * NaN`
is NaN), that the VAD floor would need an estimated 20 s to catch up after a stream that began with silence,
and that the UBSan half of the sanitizer job printed errors without failing. All three are fixed
and have tests.

## Limitations

**Musical noise.** Spectral subtraction leaves short isolated tones where the noise estimate was
wrong for a moment. The floor and the gain smoothing reduce this, but the defaults were chosen by
SNR, which does not measure it, and I have not done listening tests. The output spectrogram above
shows the residue.

**Non-stationary noise.** The noise estimate is a slow average, so it cannot follow noise that
changes quickly. Babble is the clear case: 2.8 dB improvement at 5 dB input, compared with 6 to 8 dB
for the steady noises. Horns and keyboard clatter are a stated non-goal.

**VAD errors.** The detector over-calls speech (25 to 33% of non-speech frames) partly by design,
since the 80 ms hangover extends every utterance, and partly because the reference labels are
strict. At 0 dB input it misses a third of speech frames (33.6%). A false "speech" freezes the noise
estimate; a false "silence" lets speech leak into it and be subtracted.

**Labels and data.** LibriSpeech has no speech/non-speech annotation, so the reference labels are
frames within 35 dB of the clean clip's loudest frame. Six clips from three speakers is a small test
set, and the babble comes from one speaker's other recordings. The DEMAND noise set and STOI/PESQ
are not used.

**Startup.** The first 200 ms must be noise. A stream that starts with speech will have it learned
as noise.

**Scope.** Mono 16 kHz WAV only, and no microphone capture.

## What I would do next

Track the noise floor continuously (minimum statistics) so it follows babble and other changing
noise; replace the subtraction rule with a decision-directed Wiener filter or a log-spectral
estimator to reduce musical noise; tune the VAD on annotated speech; add STOI/PESQ and a small
listening test; try a real-input FFT and SIMD, then measure on an ARM board, which is where
the latency budget is tight; run the MSVC, MinGW and macOS builds and publish their timings next to
this laptop's.

## Layout

```text
include/quietroom/   public headers (pipeline, config, wav, ...)
src/                 fft, framing, vad, noise_estimator, spectral_sub, pipeline, wav
cli/                 quietroom_cli
bench/               Google Benchmark targets
tests/               unit tests, allocation test, CLI regression test
python/              make_test_clips.py, evaluate.py, tune.py, plots
data/                small LibriSpeech-derived clips; see data/LICENSES.md
docs/                DESIGN.md, EVALUATION.md, DECISIONS.md, results/, images/
scripts/             reproduce.sh, reproduce.ps1
```

## License

MIT, see [LICENSE](LICENSE). The speech clips in `data/` are derived from LibriSpeech
(CC BY 4.0); see [data/LICENSES.md](data/LICENSES.md). Contributions: [CONTRIBUTING.md](CONTRIBUTING.md).
History: [CHANGELOG.md](CHANGELOG.md).
