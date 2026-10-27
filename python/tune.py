"""Parameter sweep for the spectral-subtraction defaults, with a held-out check.

Tunes on the first clip of each speaker and reports the winner on the other clips, which the
sweep never saw. Objective: mean SNR improvement at 5 dB input over all four noise types. The
speech-region SNR gain is printed next to it so over-aggressive settings are visible, and it only
breaks exact ties; it is not a penalty term.

    python python/tune.py --cli build/quietroom_cli[.exe]
"""
import argparse
import itertools
from pathlib import Path

import numpy as np
import soundfile as sf

import evaluate as ev

GRID = {
    "--over-subtraction": (1.5, 2.0, 2.5, 3.0, 4.0),
    "--floor": (0.02, 0.05, 0.1),
    "--smoothing": (0.3, 0.6, 0.8),
}


def score(cli: Path, clips, babble, tmp: Path, extra) -> tuple[float, float]:
    imp, gain = [], []
    for clean in clips:
        for nk in ev.NOISES:
            rng = np.random.default_rng(ev.SEED)  # same noise realisation for every setting
            r = ev.evaluate_one(cli, clean, nk, 5, babble, rng, tmp, extra)
            imp.append(r["snr_improvement_db"])
            gain.append(r["speech_snr_out_db"] - r["speech_snr_in_db"])
    return float(np.mean(imp)), float(np.mean(gain))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", required=True, type=Path)
    ap.add_argument("--data", type=Path, default=Path("data"))
    args = ap.parse_args()
    babble, _ = sf.read(args.data / "noise" / "babble.wav", dtype="float32")
    paths = sorted((args.data / "speech").glob("*.wav"))
    clips = [sf.read(p, dtype="float32")[0] for p in paths]
    tune, held = clips[0::2], clips[1::2]

    import tempfile
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        results = []
        for combo in itertools.product(*GRID.values()):
            extra = [x for k, v in zip(GRID, combo) for x in (k, str(v))]
            imp, gain = score(args.cli, tune, babble, tmp, extra)
            results.append((imp, gain, combo))
            print(f"{dict(zip(GRID, combo))}  improvement {imp:5.2f} dB  speech-region gain {gain:5.2f} dB", flush=True)
        results.sort(reverse=True)
        print("\nTop 5 on tuning clips:")
        for imp, gain, combo in results[:5]:
            print(f"  {combo}  {imp:.2f} dB (speech gain {gain:.2f})")
        best = results[0][2]
        extra = [x for k, v in zip(GRID, best) for x in (k, str(v))]
        default_imp, default_gain = score(args.cli, held, babble, tmp, [])
        best_imp, best_gain = score(args.cli, held, babble, tmp, extra)
        print(f"\nHeld-out clips, defaults : {default_imp:.2f} dB (speech gain {default_gain:.2f})")
        print(f"Held-out clips, best {best}: {best_imp:.2f} dB (speech gain {best_gain:.2f})")


if __name__ == "__main__":
    main()
