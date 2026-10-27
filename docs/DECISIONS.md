# Design decisions

Running log of choices where the PRD was open or ambiguous. Newest at the bottom.

## Tooling

- Host is Windows 11 with no system compiler. Toolchain installed with `pip install --user`:
  CMake, Ninja and `ziglang` (Zig 0.16 ships clang 21.1 as `zig c++`). No admin rights needed.
  Linux and macOS are covered by CI; Windows is built locally.
- GoogleTest 1.15.2 and Google Benchmark 1.9.1 are fetched by CMake from release tarballs.
- Warnings (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`) are errors on our targets only,
  never on fetched dependencies.

## FFT (milestone 3)

Hand-written iterative radix-2, 512 points, complex in/out, twiddles and bit-reversal table built
in the constructor. Chosen over pocketfft/KissFFT because the size is fixed, it needs no
dependency, it is ~60 lines, and allocation-free by construction. A real-input FFT would be
roughly twice as fast but the measured cost is far below the 1 ms budget (see the benchmark
section of the README), so the simpler complex transform stays.

## Framing and delay (milestone 2)

`process_frame` takes one 160-sample hop and returns one 160-sample hop. The synthesis half of the
overlap-add needs the next window, so the output is the input delayed by one hop (160 samples).
A sample is therefore available 10-20 ms after it arrived, worst case 20 ms, which is the 20 ms
algorithmic latency the PRD states. The CLI drops the first hop of output and flushes one
zero-padded frame at the end so the output WAV has the same length and alignment as the input.

## Open questions in the PRD, and what was chosen

| PRD question | Decision |
|---|---|
| Which FFT? | Hand-written radix-2 (milestone 3 above). |
| Which machine is the reference? | The development laptop: AMD Ryzen 7 7730U, Windows 11, clang 21.1.0 via Zig 0.16, `-O3`. Recorded in `docs/results/machine.json` and next to every timing table. |
| Is 6 dB at 5 dB input the right target? | Kept as written and reported per noise type. It is met for white, pink and hum and missed for babble (2.83 dB). The target was not lowered after seeing the numbers. |
| STOI / PESQ in the first release? | No. Left as a next step; it needs an extra dependency and the PRD lists it as optional. |

## Other decisions

- **Reference VAD labels** are derived from the clean clip (frame energy within 35 dB of the
  loudest frame) because LibriSpeech is not annotated. This is a proxy and is called out wherever
  the VAD numbers appear.
- **Speech distortion** is reported as the speech-region SNR before and after processing. It
  includes residual noise, so it overstates pure distortion; the output of a non-linear spectral
  subtraction cannot be split exactly into speech and noise parts.
- **First 20 frames are never speech** in `Pipeline`, so the noise estimate always has a start.
- **Defaults** (alpha 3.0, floor 0.02, smoothing 0.3) come from `python/tune.py`: a grid search on
  three clips, scored on three held-out clips. Held-out mean SNR improvement over the four noise
  types at 5 dB input went from 4.60 dB (previous defaults: 2.0 / 0.1 / 0.6) to 5.37 dB. The
  objective is SNR only; musical noise is not measured and no listening test was done.
- **Library robustness** (added after review): NaN/Inf input samples become 0, `Config::clamped()`
  limits every parameter to a stable range, and a VAD floor that is stuck low (stream started
  with digital silence) climbs at 0.5 dB per frame after 3 s of unbroken "speech".
- **WAV reader limits**: files over 256 MiB are refused before allocation; float samples that are
  not finite become 0; WAVE_FORMAT_EXTENSIBLE is accepted only with a PCM or float sub-format.
- **CLI exit codes**: 0 success, 1 I/O or input error, 2 bad usage. The regression test checks
  code 1 for a missing input so it cannot pass because of a shell quoting error.
- **Dependencies** are pinned by SHA-256 in CMake and GitHub Actions by commit SHA.
- **Committed results** are from one run on one machine; benchmark JSON has the host name and
  local path scrubbed (`python/scrub_bench_json.py`).

## Tools and skills used

- Skills: `task-observer` (session protocol), `build-prd`, `dataviz` (chart conventions for the
  matplotlib figures).
- Agents: `agent-skills:code-reviewer` and `agent-skills:security-auditor` ran read-only reviews;
  their findings are fixed in the commits after "Add reproduce scripts...". `claude-mem` was
  searched for earlier context (none existed).
- Toolchain: CMake, Ninja and Zig (clang) from `pip --user`; GCC 13.3 in an Ubuntu Docker container
  for the Linux, ASan/UBSan and clang-tidy checks; clang-format 23.1.3 from `pip`.
- Not used: Context7 (GoogleTest, Benchmark and CMake FetchContent usage was standard and the
  builds verify it), Playwright / Chrome DevTools (there is no UI), the GitHub MCP server (it
  failed to connect in this session; `git` and `gh` were used instead).
