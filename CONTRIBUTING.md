# Contributing

Thanks for looking. A few things that keep the project honest:

**Build and test**

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Warnings are errors on QuietRoom's own targets (`-DQUIETROOM_WERROR=OFF` to turn that off while
experimenting). To run the sanitizers locally on Linux or macOS:
`cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DQUIETROOM_SANITIZE=ON`.

**Rules**

- Write the test first. A change to the DSP needs a test that fails without it.
- `Pipeline::process_frame()` must stay allocation-free and lock-free. `tests/test_no_alloc.cpp`
  enforces the first part; please do not weaken it.
- Never loosen or delete a failing test to get green. If a test is wrong, say why in the commit.
- Format with `clang-format` (config in `.clang-format`) before committing.
- Any number in the README or docs must come from a real run. `scripts/reproduce.sh` (or
  `scripts/reproduce.ps1` on Windows) regenerates all of them. If a change moves the results,
  re-run it and commit the new `docs/results/` and `docs/images/` together with the change.
- CI never fails on absolute timings, and your change should not add a check that does.

**Commits**: one logical change each, with a plain description of what changed and why, for
example "Add overlap-add framing with reconstruction test".

**Data**: only small clips with a clear open license, and add them to `data/LICENSES.md`.
