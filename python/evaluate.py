"""Mix clean speech with noise at 0/5/10 dB, run quietroom_cli, measure and plot.

    python python/evaluate.py --cli build/quietroom_cli[.exe]

Writes docs/results/{results.csv,results.md,machine.json} and the PNGs in docs/images/.
Every number comes from running the CLI on the mixed files; nothing is hard-coded.

Metric definitions (see docs/EVALUATION.md):
  SNR improvement   output SNR - input SNR over the whole file, clean signal as ground truth
  Noise reduction   energy drop (dB) over frames the reference labels as non-speech
  Speech-region SNR 10 log10(E[clean] / E[out - clean]) over reference speech frames, in vs out
  VAD               the CLI's per-frame decision against reference labels derived from the
                    clean clip: a 10 ms frame is speech if its energy is within 35 dB of the
                    loudest frame. These are not human annotations.
"""
import argparse
import csv
import json
import platform
import subprocess
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf

SR = 16000
HOP = 160
SNRS = (0, 5, 10)
NOISES = ("white", "pink", "hum50", "babble")
LABEL_RANGE_DB = 35.0
SEED = 1234

# Colour-blind-safe (Okabe-Ito) categorical colours, assigned in a fixed order.
COLORS = {"white": "#0072B2", "pink": "#E69F00", "hum50": "#009E73", "babble": "#CC79A7"}
INK, MUTED, GRID = "#1f2328", "#59636e", "#d8dee4"


def style_axes(ax) -> None:
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelsize=9)
    ax.yaxis.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)


def make_noise(kind: str, n: int, babble: np.ndarray, rng: np.random.Generator) -> np.ndarray:
    if kind == "white":
        return rng.standard_normal(n).astype(np.float32)
    if kind == "pink":  # 1/f power: shape white noise in the frequency domain
        spec = np.fft.rfft(rng.standard_normal(n))
        spec[1:] /= np.sqrt(np.arange(1, len(spec)))
        spec[0] = 0
        return np.fft.irfft(spec, n).astype(np.float32)
    if kind == "hum50":  # mains hum: 50 Hz plus harmonics
        t = np.arange(n) / SR
        return sum(np.sin(2 * np.pi * 50 * h * t + rng.uniform(0, 6.28)) / h for h in (1, 2, 3, 4, 5)).astype(np.float32)
    if kind == "babble":
        start = int(rng.integers(0, max(1, len(babble) - n))) if len(babble) > n else 0
        seg = np.tile(babble, n // len(babble) + 1)[start:start + n]
        return seg.astype(np.float32)
    raise ValueError(kind)


def mix(clean: np.ndarray, noise: np.ndarray, snr_db: float) -> np.ndarray:
    gain = np.sqrt(np.sum(clean**2) / (np.sum(noise**2) * 10 ** (snr_db / 10)))
    return (clean + gain * noise).astype(np.float32)


def frames(x: np.ndarray) -> np.ndarray:
    n = len(x) // HOP
    return x[: n * HOP].reshape(n, HOP)


def reference_labels(clean: np.ndarray) -> np.ndarray:
    energy_db = 10 * np.log10(np.mean(frames(clean).astype(np.float64) ** 2, axis=1) + 1e-12)
    return energy_db > energy_db.max() - LABEL_RANGE_DB


def db_ratio(num: float, den: float) -> float:
    return float(10 * np.log10((num + 1e-20) / (den + 1e-20)))


def run_cli(cli: Path, noisy: np.ndarray, tmp: Path, extra=()) -> tuple[np.ndarray, np.ndarray, dict]:
    inp, out, vad, js = tmp / "in.wav", tmp / "out.wav", tmp / "vad.csv", tmp / "stats.json"
    sf.write(inp, np.clip(noisy, -1, 1), SR, subtype="PCM_16")
    subprocess.run([str(cli), str(inp), str(out), "--vad-csv", str(vad), "--json", str(js), *extra],
                   check=True, capture_output=True)
    y, _ = sf.read(out, dtype="float32")
    return y, np.loadtxt(vad, dtype=int), json.loads(js.read_text())


def evaluate_one(cli: Path, clean: np.ndarray, noise_kind: str, snr: int, babble: np.ndarray,
                 rng: np.random.Generator, tmp: Path, extra=()) -> dict:
    noisy = mix(clean, make_noise(noise_kind, len(clean), babble, rng), snr)
    noisy = np.clip(noisy, -1, 1)  # what the 16-bit file can actually hold
    y, vad, stats = run_cli(cli, noisy, tmp, extra)
    n = min(len(y), len(clean))
    c, x, y = clean[:n], noisy[:n], y[:n]
    labels = reference_labels(c)
    k = min(len(labels), len(vad))
    labels, vad = labels[:k], vad[:k].astype(bool)
    fc, fx, fy = frames(c)[:k], frames(x)[:k], frames(y)[:k]

    in_snr = db_ratio(np.sum(c**2), np.sum((x - c) ** 2))
    out_snr = db_ratio(np.sum(c**2), np.sum((y - c) ** 2))
    noise_only = ~labels
    speech = labels
    return {
        "noise": noise_kind, "input_snr_db": snr,
        "measured_input_snr_db": in_snr, "output_snr_db": out_snr, "snr_improvement_db": out_snr - in_snr,
        "noise_reduction_db": db_ratio(np.sum(fx[noise_only] ** 2), np.sum(fy[noise_only] ** 2)),
        "speech_snr_in_db": db_ratio(np.sum(fc[speech] ** 2), np.sum((fx[speech] - fc[speech]) ** 2)),
        "speech_snr_out_db": db_ratio(np.sum(fc[speech] ** 2), np.sum((fy[speech] - fc[speech]) ** 2)),
        "tp": int(np.sum(vad & labels)), "fp": int(np.sum(vad & ~labels)),
        "fn": int(np.sum(~vad & labels)), "tn": int(np.sum(~vad & ~labels)),
        "_frame_us": stats["frame_us"], "_noisy": x, "_out": y, "_clean": c,
    }


def machine_info(compiler: str, flags: str) -> dict:
    cpu = platform.processor()
    try:
        cpu = subprocess.run(["powershell", "-NoProfile", "-Command",
                              "(Get-CimInstance Win32_Processor).Name"], capture_output=True,
                             text=True, timeout=20).stdout.strip() or cpu
    except (OSError, subprocess.SubprocessError):
        pass
    return {"cpu": cpu, "os": platform.platform(), "logical_cpus": __import__("os").cpu_count(),
            "compiler": compiler, "build_flags": flags, "python": platform.python_version()}


def summarize(rows: list[dict], key: str) -> dict:
    return {(nk, s): float(np.mean([r[key] for r in rows if r["noise"] == nk and r["input_snr_db"] == s]))
            for nk in NOISES for s in SNRS}


def plot_snr(rows, path: Path) -> None:
    imp = summarize(rows, "snr_improvement_db")
    fig, ax = plt.subplots(figsize=(7.5, 4))
    width = 0.2
    for i, nk in enumerate(NOISES):
        xs = np.arange(len(SNRS)) + (i - 1.5) * width
        vals = [imp[(nk, s)] for s in SNRS]
        ax.bar(xs, vals, width * 0.9, color=COLORS[nk], label=nk)
    ax.axhline(6, color=MUTED, linestyle="--", linewidth=1, label="PRD target (6 dB at 5 dB input)")
    ax.axhline(0, color=MUTED, linewidth=0.8)
    ax.set_xticks(range(len(SNRS)), [f"{s} dB" for s in SNRS])
    ax.set_xlabel("Input SNR", color=INK)
    ax.set_ylabel("SNR improvement (dB)", color=INK)
    ax.set_title("SNR improvement by input SNR and noise type (mean of 6 clips)", color=INK, fontsize=11, loc="left")
    ax.legend(frameon=False, ncol=3, fontsize=9, loc="upper center", bbox_to_anchor=(0.5, -0.18))
    style_axes(ax)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_confusion(rows, path: Path) -> None:
    tp, fp, fn, tn = (sum(r[k] for r in rows) for k in ("tp", "fp", "fn", "tn"))
    m = np.array([[tn, fp], [fn, tp]], dtype=float)
    pct = m / m.sum(axis=1, keepdims=True) * 100
    fig, ax = plt.subplots(figsize=(4.6, 4))
    ax.imshow(pct, cmap="Blues", vmin=0, vmax=100)
    for i in range(2):
        for j in range(2):
            ax.text(j, i, f"{int(m[i, j]):,}\n{pct[i, j]:.1f}% of row", ha="center", va="center",
                    color="white" if pct[i, j] > 55 else INK, fontsize=10)
    ax.set_xticks([0, 1], ["non-speech", "speech"])
    ax.set_yticks([0, 1], ["non-speech", "speech"])
    ax.set_xlabel("VAD decision", color=INK)
    ax.set_ylabel("Reference label", color=INK)
    ax.set_title("VAD confusion matrix, all noise types and SNRs", color=INK, fontsize=10, loc="left")
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_timing(frame_us: np.ndarray, path: Path) -> None:
    fig, ax = plt.subplots(figsize=(7.5, 3.8))
    ax.hist(frame_us, bins=np.logspace(np.log10(max(frame_us.min(), 0.5)), np.log10(frame_us.max()), 70),
            color=COLORS["white"])
    ax.set_xscale("log")
    for v, name in ((np.percentile(frame_us, 50), "p50"), (np.percentile(frame_us, 99), "p99"), (frame_us.max(), "max")):
        ax.axvline(v, color=MUTED, linewidth=1, linestyle=":")
        ax.text(v, ax.get_ylim()[1] * 0.92, f" {name} {v:.0f} µs", fontsize=8, color=INK, rotation=90, va="top")
    ax.set_xlabel("process_frame() wall time per frame (µs, log scale)", color=INK)
    ax.set_ylabel("frames", color=INK)
    ax.set_title(f"Per-frame compute time, {len(frame_us):,} frames; the deadline is 10,000 µs",
                 color=INK, fontsize=11, loc="left")
    style_axes(ax)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_spectrograms(example: dict, path: Path) -> None:
    fig, axes = plt.subplots(1, 3, figsize=(12, 3.6), sharey=True)
    for ax, (name, sig) in zip(axes, (("clean", example["_clean"]), ("noisy (5 dB, pink)", example["_noisy"]),
                                      ("QuietRoom output", example["_out"]))):
        # Display only: 1e-6 noise keeps the digital-silence lead-in from plotting as -inf (blank).
        shown = sig + 1e-6 * np.random.default_rng(0).standard_normal(len(sig)).astype(np.float32)
        ax.specgram(shown, NFFT=512, Fs=SR, noverlap=256, cmap="magma", vmin=-130, vmax=-30)
        ax.set_title(name, color=INK, fontsize=10, loc="left")
        ax.set_xlabel("time (s)", color=INK)
        ax.tick_params(colors=MUTED, labelsize=8)
    axes[0].set_ylabel("frequency (Hz)", color=INK)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_waveforms(example: dict, path: Path) -> None:
    t = np.arange(len(example["_clean"])) / SR
    fig, axes = plt.subplots(3, 1, figsize=(10, 5.5), sharex=True, sharey=True)
    for ax, (name, sig, col) in zip(axes, (("clean", example["_clean"], MUTED),
                                           ("noisy (5 dB, pink)", example["_noisy"], COLORS["pink"]),
                                           ("QuietRoom output", example["_out"], COLORS["white"]))):
        ax.plot(t, sig, color=col, linewidth=0.5)
        ax.set_ylabel(name, color=INK, fontsize=9)
        style_axes(ax)
    axes[-1].set_xlabel("time (s)", color=INK)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def write_outputs(rows, machine, frame_us, results_dir: Path) -> None:
    results_dir.mkdir(parents=True, exist_ok=True)
    public = [{k: v for k, v in r.items() if not k.startswith("_")} for r in rows]
    with open(results_dir / "results.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(public[0]))
        w.writeheader()
        w.writerows({k: (round(v, 3) if isinstance(v, float) else v) for k, v in r.items()} for r in public)
    (results_dir / "machine.json").write_text(json.dumps(machine, indent=2) + "\n", encoding="utf-8")

    imp, red = summarize(public, "snr_improvement_db"), summarize(public, "noise_reduction_db")
    lines = ["# Evaluation results", "", "Generated by `python/evaluate.py`; do not edit by hand.", "",
             f"{len({r['noise'] for r in public}) * len(SNRS)} conditions x 6 clips = {len(public)} runs.", "",
             "## SNR improvement (dB), mean over clips", "",
             "| noise | " + " | ".join(f"{s} dB in" for s in SNRS) + " |", "|---|" + "---|" * len(SNRS)]
    lines += [f"| {nk} | " + " | ".join(f"{imp[(nk, s)]:.2f}" for s in SNRS) + " |" for nk in NOISES]
    lines += ["", "## Noise reduction in non-speech frames (dB), mean over clips", "",
              "| noise | " + " | ".join(f"{s} dB in" for s in SNRS) + " |", "|---|" + "---|" * len(SNRS)]
    lines += [f"| {nk} | " + " | ".join(f"{red[(nk, s)]:.2f}" for s in SNRS) + " |" for nk in NOISES]
    lines += ["", "## Speech-region SNR in -> out (dB), mean over clips", "",
              "| noise | " + " | ".join(f"{s} dB in" for s in SNRS) + " |", "|---|" + "---|" * len(SNRS)]
    for nk in NOISES:
        cells = []
        for s in SNRS:
            sel = [r for r in public if r["noise"] == nk and r["input_snr_db"] == s]
            cells.append(f"{np.mean([r['speech_snr_in_db'] for r in sel]):.1f} -> {np.mean([r['speech_snr_out_db'] for r in sel]):.1f}")
        lines.append(f"| {nk} | " + " | ".join(cells) + " |")
    lines += ["", "## VAD against reference labels", "",
              "| input SNR | accuracy | false-silence rate | false-speech rate | TP | FP | FN | TN |", "|---|---|---|---|---|---|---|---|"]
    for s in SNRS + ("all",):
        sel = [r for r in public if s == "all" or r["input_snr_db"] == s]
        tp, fp, fn, tn = (sum(r[k] for r in sel) for k in ("tp", "fp", "fn", "tn"))
        lines.append(f"| {s if s == 'all' else str(s) + ' dB'} | {(tp + tn) / (tp + fp + fn + tn):.1%} | "
                     f"{fn / max(tp + fn, 1):.1%} | {fp / max(fp + tn, 1):.1%} | {tp} | {fp} | {fn} | {tn} |")
    f_us = np.asarray(frame_us)
    lines += ["", "## Per-frame compute time (CLI, all runs)", "",
              f"{len(f_us):,} frames. mean {f_us.mean():.1f} µs, p50 {np.percentile(f_us, 50):.1f} µs, "
              f"p99 {np.percentile(f_us, 99):.1f} µs, max {f_us.max():.1f} µs; "
              f"{int((f_us > 2000).sum())} over 2 ms, {int((f_us > 10000).sum())} over 10 ms; "
              f"real-time factor {f_us.mean() / 10000:.5f}.", "",
              "## Machine", "", "```json", json.dumps(machine, indent=2), "```", ""]
    (results_dir / "results.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", required=True, type=Path)
    ap.add_argument("--data", type=Path, default=Path("data"))
    ap.add_argument("--results", type=Path, default=Path("docs/results"))
    ap.add_argument("--images", type=Path, default=Path("docs/images"))
    ap.add_argument("--compiler", default="unknown")
    ap.add_argument("--flags", default="unknown")
    args = ap.parse_args()

    babble, _ = sf.read(args.data / "noise" / "babble.wav", dtype="float32")
    clips = sorted((args.data / "speech").glob("*.wav"))
    if not clips:
        raise SystemExit(f"no clips in {args.data / 'speech'}; run python/make_test_clips.py first")

    rng = np.random.default_rng(SEED)
    rows, frame_us, example = [], [], None
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for clip in clips:
            clean, _ = sf.read(clip, dtype="float32")
            for nk in NOISES:
                for snr in SNRS:
                    r = evaluate_one(args.cli, clean, nk, snr, babble, rng, tmp)
                    rows.append(r)
                    frame_us += r["_frame_us"]
                    if example is None and nk == "pink" and snr == 5:
                        example = r
                    print(f"{clip.stem:18s} {nk:7s} {snr:2d} dB  improvement {r['snr_improvement_db']:6.2f} dB")

    args.images.mkdir(parents=True, exist_ok=True)
    plot_snr(rows, args.images / "snr_improvement.png")
    plot_confusion(rows, args.images / "vad_confusion.png")
    plot_timing(np.asarray(frame_us), args.images / "timing_histogram.png")
    plot_spectrograms(example, args.images / "spectrograms.png")
    plot_waveforms(example, args.images / "waveforms.png")
    write_outputs(rows, machine_info(args.compiler, args.flags), frame_us, args.results)
    print(f"wrote {args.results / 'results.md'}")


if __name__ == "__main__":
    main()
