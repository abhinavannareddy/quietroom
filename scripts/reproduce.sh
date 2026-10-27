#!/usr/bin/env bash
# Rebuild, test, benchmark and re-run the whole evaluation. Works on Linux, macOS and
# Git Bash / MSYS2 on Windows. Needs: cmake, a C++17 compiler, python3 with python/requirements.txt.
#
#   scripts/reproduce.sh            # uses ./build
#   BUILD_DIR=out scripts/reproduce.sh
#
# Results are written to docs/results/ and docs/images/. Timings depend on the machine, so the
# numbers you get will differ a little from the ones committed in the README.
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build}"
PYTHON="${PYTHON:-python3}"
command -v "$PYTHON" >/dev/null || PYTHON=python

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

CLI=""
for candidate in "$BUILD_DIR/quietroom_cli" "$BUILD_DIR/quietroom_cli.exe" "$BUILD_DIR/Release/quietroom_cli.exe"; do
  [ -x "$candidate" ] && CLI="$candidate" && break
done
BENCH=""
for candidate in "$BUILD_DIR/bench/quietroom_bench" "$BUILD_DIR/bench/quietroom_bench.exe" "$BUILD_DIR/bench/Release/quietroom_bench.exe"; do
  [ -x "$candidate" ] && BENCH="$candidate" && break
done
[ -n "$CLI" ] && [ -n "$BENCH" ] || { echo "could not find the built CLI or benchmark in $BUILD_DIR" >&2; exit 1; }

mkdir -p docs/results
"$BENCH" --benchmark_out=docs/results/bench.json --benchmark_out_format=json

COMPILER="$(grep -m1 '^CMAKE_CXX_COMPILER:' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)"
FLAGS="$(grep -m1 '^CMAKE_CXX_FLAGS_RELEASE:' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)"
"$PYTHON" python/evaluate.py --cli "$CLI" --compiler "$COMPILER" --flags "$FLAGS -std=c++17"
"$PYTHON" python/plot_pipeline.py

echo
echo "Done. Tables: docs/results/results.md   Plots: docs/images/   Benchmark JSON: docs/results/bench.json"
