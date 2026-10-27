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
