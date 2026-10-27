"""Illustrative figures for the README, all computed from real runs of quietroom_cli.

    python python/make_figures.py --cli build/quietroom_cli[.exe]

Writes docs/images/{hero,vad_timeline,subtraction_stage,latency_budget,tradeoff}.png.
The spectrum plot recomputes the textbook formulas in numpy for illustration and overlays the
spectrum of what the C++ pipeline actually produced, so the two can be compared by eye.
"""
import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

import evaluate as ev

SR, HOP, WIN, NFFT = ev.SR, ev.HOP, 320, 512
CLIP = Path("data/speech/3000-15664-0000.wav")
BLUE, ORANGE, GREEN, PINK = (ev.COLORS[k] for k in ("white", "pink", "hum50", "babble"))
INK, MUTED, GRID = ev.INK, ev.MUTED, ev.GRID
OUT = Path("docs/images")
INIT_FRAMES = 20  # the pipeline's start-up noise-only frames


def noisy_example(kind: str, snr: int, babble: np.ndarray):
    clean, _ = sf.read(CLIP, dtype="float32")
    noise = ev.make_noise(kind, len(clean), babble, np.random.default_rng(ev.SEED))
    return clean, np.clip(ev.mix(clean, noise, snr), -1, 1)


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT / name, dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"wrote {OUT / name}")


def hero(cli: Path, tmp: Path, babble: np.ndarray) -> None:
    _, noisy = noisy_example("pink", 5, babble)
    out, _, _ = ev.run_cli(cli, noisy, tmp)
    t = np.arange(len(noisy)) / SR
    fig = plt.figure(figsize=(12, 4.2))
    fig.text(0.01, 0.97, "QuietRoom", fontsize=24, fontweight="bold", color=INK, va="top")
    fig.text(0.01, 0.885, "Noise out, speech in: 10 ms frames, no heap allocation, a few microseconds each.",
             fontsize=11.5, color=MUTED, va="top")
    gs = fig.add_gridspec(2, 1, top=0.78, bottom=0.1, hspace=0.12)
    top = fig.add_subplot(gs[0])
    bottom = fig.add_subplot(gs[1], sharex=top, sharey=top)
    top.plot(t, noisy, color=ORANGE, linewidth=0.5)
    bottom.plot(t, out[: len(t)], color=BLUE, linewidth=0.5)
    top.set_ylabel("input", color=INK)
    bottom.set_ylabel("output", color=INK)
    top.text(0.995, 0.88, "speech + pink noise at 5 dB SNR", transform=top.transAxes, ha="right", va="top",
             fontsize=9, color=MUTED)
    bottom.text(0.995, 0.88, "after QuietRoom", transform=bottom.transAxes, ha="right", va="top", fontsize=9,
                color=MUTED)
    plt.setp(top.get_xticklabels(), visible=False)
    bottom.set_xlabel("time (s)", color=INK)
    for ax in (top, bottom):
        ev.style_axes(ax)
    save(fig, "hero.png")


def vad_timeline(cli: Path, tmp: Path, babble: np.ndarray) -> None:
    clean, noisy = noisy_example("white", 5, babble)
    _, vad, _ = ev.run_cli(cli, noisy, tmp)
    labels = ev.reference_labels(clean)
    k = min(len(labels), len(vad))
    t = np.arange(len(noisy)) / SR
    th = (np.arange(k) + 0.5) * HOP / SR

    fig, (ax, strip) = plt.subplots(2, 1, figsize=(11, 3.8), sharex=True,
                                    gridspec_kw={"height_ratios": [3, 1], "hspace": 0.08})
    ax.plot(t, noisy, color=MUTED, linewidth=0.4, label="noisy input (white noise, 5 dB SNR)")
    ax.fill_between(th, -1, 1, where=vad[:k].astype(bool), step="mid", color=BLUE, alpha=0.18,
                    label="VAD says speech (incl. 80 ms hangover)")
    ax.set_ylim(-1, 1)
    ax.set_ylabel("amplitude", color=INK)
    ax.legend(frameon=False, fontsize=8, loc="upper right", ncol=2)
    strip.fill_between(th, 0.55, 1, where=labels[:k], step="mid", color=INK, alpha=0.75, linewidth=0)
    strip.fill_between(th, 0, 0.45, where=vad[:k].astype(bool), step="mid", color=BLUE, linewidth=0)
    strip.set_yticks([0.22, 0.78], ["VAD", "reference"], fontsize=8)
    strip.set_ylim(0, 1)
    strip.set_xlabel("time (s)", color=INK)
    for a in (ax, strip):
        ev.style_axes(a)
    strip.yaxis.grid(False)
    save(fig, "vad_timeline.png")


def frame_spectrum(x: np.ndarray, start: int, window: np.ndarray) -> np.ndarray:
    seg = np.zeros(NFFT)
    seg[:WIN] = x[start:start + WIN] * window
    return np.abs(np.fft.rfft(seg)) ** 2


def subtraction_stage(cli: Path, tmp: Path, babble: np.ndarray) -> None:
    clean, noisy = noisy_example("white", 5, babble)
    out, _, _ = ev.run_cli(cli, noisy, tmp)
    n = np.arange(WIN)
    w = np.sqrt(0.5 * (1 - np.cos(2 * np.pi * n / WIN)))
    energy = np.convolve(clean.astype(np.float64) ** 2, np.ones(WIN), mode="valid")
    start = int(np.argmax(energy)) // HOP * HOP  # loudest voiced stretch, aligned to a hop
    start = max(start, 40 * HOP)

    p_noisy = frame_spectrum(noisy, start, w)[: NFFT // 2 + 1]
    noise = np.mean([frame_spectrum(noisy, f * HOP, w)[: NFFT // 2 + 1] for f in range(INIT_FRAMES)], axis=0)
    cfg = dict(alpha=3.0, beta=0.02)  # Config defaults
    sub = np.maximum(p_noisy - cfg["alpha"] * noise, cfg["beta"] * p_noisy)
    gain = np.sqrt(sub / (p_noisy + 1e-20))
    p_out = frame_spectrum(out, start, w)[: NFFT // 2 + 1]  # what the C++ pipeline produced
    f = np.arange(NFFT // 2 + 1) * SR / NFFT / 1000
    db = lambda p: 10 * np.log10(p + 1e-12)  # noqa: E731

    fig, (a, b) = plt.subplots(1, 2, figsize=(12, 4), gridspec_kw={"width_ratios": [1.6, 1]})
    # Thick and translucent underneath, so where the output matches the input (strong speech
    # bins, gain close to 1) both lines stay visible.
    a.plot(f, db(p_noisy), color=ORANGE, linewidth=2.6, alpha=0.6, label="noisy frame, power P")
    a.plot(f, db(noise), color=MUTED, linewidth=1.4, linestyle="--", label="noise estimate N")
    a.plot(f, db(cfg["alpha"] * noise), color=MUTED, linewidth=1.4, label="3 x N (what gets subtracted)")
    a.plot(f, db(p_out), color=BLUE, linewidth=0.9, label="actual pipeline output")
    a.set_xlabel("frequency (kHz)", color=INK)
    a.set_ylabel("power (dB)", color=INK)
    a.set_title("One 20 ms frame of speech, before and after", loc="left", fontsize=11, color=INK)
    a.legend(frameon=False, fontsize=8, loc="upper right")
    b.plot(f, gain, color=GREEN, linewidth=1.0)
    b.axhline(np.sqrt(cfg["beta"]), color=MUTED, linestyle=":", linewidth=1)
    b.text(4.75, np.sqrt(cfg["beta"]) + 0.04, "floor: sqrt(beta)", ha="center", fontsize=8, color=MUTED)
    b.set_ylim(0, 1.05)
    b.set_xlabel("frequency (kHz)", color=INK)
    b.set_ylabel("gain G", color=INK)
    b.set_title("Gain from the formula, before time smoothing", loc="left", fontsize=11, color=INK)
    for ax in (a, b):
        ev.style_axes(ax)
    fig.tight_layout()
    save(fig, "subtraction_stage.png")


def latency_budget(bench_json: Path) -> None:
    data = json.loads(bench_json.read_text(encoding="utf-8"))["benchmarks"]
    median = next(b for b in data if b["name"].startswith("BM_ProcessFrame/") and b.get("aggregate_name") == "median")
    reps = [b for b in data if b["name"].startswith("BM_ProcessFrame/") and b.get("run_type") == "iteration"]
    rows = [
        ("process_frame(), typical (p50)", median["p50_us"], BLUE),
        ("process_frame(), p99", median["p99_us"], BLUE),
        ("process_frame(), worst frame seen", max(r["max_us"] for r in reps), BLUE),
        ("internal budget (2 ms)", 2000, MUTED),
        ("deadline: one hop (10 ms)", 10000, MUTED),
        ("algorithmic latency, worst case (20 ms)", 20000, ORANGE),
    ]
    fig, ax = plt.subplots(figsize=(10, 3.6))
    ys = np.arange(len(rows))[::-1]
    for y, (name, value, color) in zip(ys, rows):
        ax.barh(y, value, color=color, height=0.62)
        ax.text(value * 1.15, y, f"{value:,.0f} µs" if value < 1000 else f"{value / 1000:,.1f} ms", va="center",
                fontsize=9, color=INK)
    ax.set_yticks(ys, [r[0] for r in rows], fontsize=9)
    ax.set_xscale("log")
    ax.set_xlim(3, 120000)
    ax.set_xlabel("time (µs, log scale)", color=INK)
    ax.set_title("Compute time per frame vs. the budget it has to fit in", loc="left", fontsize=11, color=INK)
    ev.style_axes(ax)
    ax.yaxis.grid(False)
    ax.xaxis.grid(True, color=GRID, linewidth=0.6)
    save(fig, "latency_budget.png")


def tradeoff(cli: Path, tmp: Path, babble: np.ndarray, data_dir: Path) -> list:
    clips = [sf.read(p, dtype="float32")[0] for p in sorted((data_dir / "speech").glob("*.wav"))]
    alphas = [1.0, 1.5, 2.0, 3.0, 4.0, 6.0]
    imp, gain = [], []
    for a in alphas:
        imps, gains = [], []
        for clean in clips:
            for nk in ev.NOISES:
                rng = np.random.default_rng(ev.SEED)
                r = ev.evaluate_one(cli, clean, nk, 5, babble, rng, tmp, ["--over-subtraction", str(a)])
                imps.append(r["snr_improvement_db"])
                gains.append(r["speech_snr_out_db"] - r["speech_snr_in_db"])
        imp.append(float(np.mean(imps)))
        gain.append(float(np.mean(gains)))
    fig, ax = plt.subplots(figsize=(7.5, 3.8))
    ax.plot(alphas, imp, marker="o", color=BLUE, linewidth=1.6, label="SNR improvement, whole file")
    ax.plot(alphas, gain, marker="s", color=ORANGE, linewidth=1.6, label="SNR gain inside speech frames")
    ax.axvline(3.0, color=MUTED, linestyle=":", linewidth=1)
    ax.text(3.05, min(gain + imp) + 0.1, "default", fontsize=8, color=MUTED)
    ax.set_xlabel("over-subtraction factor alpha", color=INK)
    ax.set_ylabel("dB", color=INK)
    ax.set_title("Turning alpha up: 5 dB input, 4 noise types x 6 clips per point", loc="left", fontsize=11, color=INK)
    ax.legend(frameon=False, fontsize=9)
    ev.style_axes(ax)
    save(fig, "tradeoff.png")
    return list(zip(alphas, imp, gain))


def demo_pairs(cli: Path, tmp: Path, babble: np.ndarray) -> None:
    """Before/after WAV files to listen to: an easy case (pink) and the hardest one (babble)."""
    folder = Path("docs/demo")
    folder.mkdir(parents=True, exist_ok=True)
    for kind in ("pink", "babble"):
        _, noisy = noisy_example(kind, 5, babble)
        cleaned, _, _ = ev.run_cli(cli, noisy, tmp)
        sf.write(folder / f"{kind}_5db_noisy.wav", noisy, SR, subtype="PCM_16")
        sf.write(folder / f"{kind}_5db_cleaned.wav", np.clip(cleaned, -1, 1), SR, subtype="PCM_16")
        print(f"wrote {folder}/{kind}_5db_noisy.wav and _cleaned.wav")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", required=True, type=Path)
    ap.add_argument("--data", type=Path, default=Path("data"))
    ap.add_argument("--bench", type=Path, default=Path("docs/results/bench.json"))
    args = ap.parse_args()
    babble, _ = sf.read(args.data / "noise" / "babble.wav", dtype="float32")
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        hero(args.cli, tmp, babble)
        vad_timeline(args.cli, tmp, babble)
        subtraction_stage(args.cli, tmp, babble)
        latency_budget(args.bench)
        demo_pairs(args.cli, tmp, babble)
        for a, i, g in tradeoff(args.cli, tmp, babble, args.data):
            print(f"alpha {a:3.1f}: SNR improvement {i:5.2f} dB, speech-region gain {g:5.2f} dB")


if __name__ == "__main__":
    main()
