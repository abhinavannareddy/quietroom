"""Create the fixed noisy clip used by the CLI regression test (tests/test_cli_regression.cpp).

White noise at 5 dB SNR on data/speech/3000-15664-0000.wav, fixed seed, so the file is
reproducible. The test compares the CLI output against the clean clip.

    python python/make_regression_clip.py
"""
from pathlib import Path

import numpy as np
import soundfile as sf

import evaluate as ev

CLEAN = Path("data/speech/3000-15664-0000.wav")
OUT = Path("data/regression/noisy_white_5db.wav")


def main() -> None:
    clean, _ = sf.read(CLEAN, dtype="float32")
    noise = ev.make_noise("white", len(clean), np.zeros(1, np.float32), np.random.default_rng(ev.SEED))
    noisy = np.clip(ev.mix(clean, noise, 5.0), -1, 1)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    sf.write(OUT, noisy, ev.SR, subtype="PCM_16")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
