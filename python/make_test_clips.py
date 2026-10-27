"""Build the small speech test set in data/ from an extracted LibriSpeech dev-clean-2 folder.

LibriSpeech is CC BY 4.0 (https://www.openslr.org/31/). We keep 6 short utterances (2 each from
3 speakers) plus one babble-noise file made from *other* utterances of the same speakers, so
the babble never contains the test sentences.

    python python/make_test_clips.py --librispeech <dir containing LibriSpeech/dev-clean-2>

Output (16 kHz mono 16-bit WAV):
    data/speech/<utterance-id>.wav   0.5 s of digital silence + speech + 0.3 s of silence
    data/noise/babble.wav            12 s of overlapped speech
    data/clips.json                  which LibriSpeech utterances were used
"""
import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 16000
LEAD_S, TAIL_S = 0.5, 0.3  # silence so the noise estimate can initialise before speech starts
SPEAKERS_TO_USE = 3
CLIPS_PER_SPEAKER = 2
MIN_S, MAX_S = 3.0, 7.0
BABBLE_LAYERS = 6
BABBLE_SECONDS = 12


def utterances(root: Path):
    by_speaker = {}
    for flac in sorted(root.rglob("*.flac")):
        speaker = flac.parent.parent.name
        by_speaker.setdefault(speaker, []).append(flac)
    return by_speaker


def load(path: Path) -> np.ndarray:
    x, sr = sf.read(path, dtype="float32")
    if sr != SR or x.ndim != 1:
        raise ValueError(f"{path}: expected 16 kHz mono, got {sr} Hz shape {x.shape}")
    return x


def write16(path: Path, x: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(path, np.clip(x, -1, 1), SR, subtype="PCM_16")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--librispeech", required=True, type=Path)
    ap.add_argument("--out", type=Path, default=Path("data"))
    args = ap.parse_args()

    by_speaker = utterances(args.librispeech)
    speakers = sorted(by_speaker)[:SPEAKERS_TO_USE]
    if len(speakers) < SPEAKERS_TO_USE:
        raise SystemExit(f"need {SPEAKERS_TO_USE} speakers, found {len(speakers)}")

    used, spare = [], []
    for spk in speakers:
        picked = 0
        for path in by_speaker[spk]:
            x = load(path)
            if picked < CLIPS_PER_SPEAKER and MIN_S <= len(x) / SR <= MAX_S:
                x = x / np.max(np.abs(x)) * 0.7
                padded = np.concatenate([np.zeros(int(LEAD_S * SR), np.float32), x,
                                         np.zeros(int(TAIL_S * SR), np.float32)])
                write16(args.out / "speech" / f"{path.stem}.wav", padded)
                used.append(path.stem)
                picked += 1
            else:
                spare.append(path)

    rng = np.random.default_rng(0)
    babble = np.zeros(BABBLE_SECONDS * SR, np.float32)
    for path in spare[:BABBLE_LAYERS]:
        x = load(path)
        x = np.tile(x, BABBLE_SECONDS * SR // len(x) + 1)[: len(babble)]
        babble += np.roll(x, int(rng.integers(0, len(x)))) / np.max(np.abs(x))
    write16(args.out / "noise" / "babble.wav", babble / np.max(np.abs(babble)) * 0.7)

    manifest = {"source": "LibriSpeech dev-clean-2 (CC BY 4.0)", "speech": used,
                "babble_from": [p.stem for p in spare[:BABBLE_LAYERS]]}
    (args.out / "clips.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"wrote {len(used)} speech clips and babble.wav to {args.out}")


if __name__ == "__main__":
    main()
