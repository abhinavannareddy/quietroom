# Rebuild, test, benchmark and re-run the whole evaluation on Windows (PowerShell).
# Needs: cmake, a C++17 compiler on PATH (MSVC from a Developer prompt, or MinGW), and python
# with python\requirements.txt installed.
#
#   scripts\reproduce.ps1
#   scripts\reproduce.ps1 -Generator Ninja
param(
    [string]$BuildDir = "build",
    [string]$Generator = ""
)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$configure = @("-S", ".", "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Release")
if ($Generator) { $configure += @("-G", $Generator) }
cmake @configure
cmake --build $BuildDir --config Release --parallel
ctest --test-dir $BuildDir -C Release --output-on-failure

$cli = @("$BuildDir\quietroom_cli.exe", "$BuildDir\Release\quietroom_cli.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
$bench = @("$BuildDir\bench\quietroom_bench.exe", "$BuildDir\bench\Release\quietroom_bench.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cli -or -not $bench) { throw "could not find the built CLI or benchmark in $BuildDir" }

New-Item -ItemType Directory -Force docs\results | Out-Null
& $bench --benchmark_out=docs/results/bench.json --benchmark_out_format=json

$cache = Get-Content "$BuildDir\CMakeCache.txt"
$compiler = ($cache | Where-Object { $_ -like "CMAKE_CXX_COMPILER:*" } | Select-Object -First 1) -replace "^[^=]*=", ""
$flags = ($cache | Where-Object { $_ -like "CMAKE_CXX_FLAGS_RELEASE:*" } | Select-Object -First 1) -replace "^[^=]*=", ""
python python\evaluate.py --cli $cli --compiler $compiler --flags "$flags -std=c++17"
python python\plot_pipeline.py

Write-Host ""
Write-Host "Done. Tables: docs\results\results.md   Plots: docs\images\   Benchmark JSON: docs\results\bench.json"
