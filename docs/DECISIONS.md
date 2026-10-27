# Design decisions

Running log of choices where the PRD was open or ambiguous. Newest at the bottom.

## Tooling

- Host is Windows 11 with no system compiler. Toolchain installed with `pip install --user`:
  CMake, Ninja and `ziglang` (Zig 0.16 ships clang 21.1 as `zig c++`). No admin rights needed.
  Linux and macOS are covered by CI; Windows is built locally.
- GoogleTest 1.15.2 and Google Benchmark 1.9.1 are fetched by CMake from release tarballs.
- Warnings (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`) are errors on our targets only,
  never on fetched dependencies.
