"""Draw the per-frame pipeline diagram to docs/images/pipeline.png.

    python python/plot_pipeline.py
"""
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

INK, MUTED = "#1f2328", "#59636e"
MAIN, SIDE, STATE = "#dbeafe", "#fef3c7", "#e5e7eb"
OUT = Path("docs/images/pipeline.png")


def box(ax, x, y, w, h, text, fill):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
                                facecolor=fill, edgecolor=MUTED, linewidth=1))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=9, color=INK)


def arrow(ax, x0, y0, x1, y1):
    ax.annotate("", xy=(x1, y1), xytext=(x0, y0),
                arrowprops=dict(arrowstyle="-|>", color=MUTED, linewidth=1.2))


def main() -> None:
    fig, ax = plt.subplots(figsize=(12, 4.6))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 4.6)
    ax.axis("off")

    w, h, y = 1.9, 1.0, 3.0
    xs = [0.2, 2.5, 5.7, 8.0, 10.1]
    box(ax, xs[0], y, w, h, "input hop\n160 samples (10 ms)", STATE)
    box(ax, xs[1], y, 2.5, h, "1. window prev + current hop\n(320 samples, sqrt-Hann)\n-> 512-point FFT", MAIN)
    box(ax, xs[2], y, 1.9, h, "4. spectral subtraction\ngain per bin", MAIN)
    box(ax, xs[3], y, 1.9, h, "5. IFFT, window,\noverlap-add", MAIN)
    box(ax, xs[4], y, 1.7, h, "output hop\n(input delayed\nby one hop)", STATE)
    for a, b in ((xs[0] + w, xs[1]), (xs[1] + 2.5, xs[2]), (xs[2] + 1.9, xs[3]), (xs[3] + 1.9, xs[4])):
        arrow(ax, a, y + h / 2, b, y + h / 2)

    box(ax, 2.5, 1.2, 2.5, 1.0, "2. VAD: energy vs adaptive\nfloor, ZCR, flatness,\nhangover", SIDE)
    box(ax, 5.7, 1.2, 1.9, 1.0, "3. noise estimate\n(non-speech frames)", SIDE)
    arrow(ax, 3.75, y, 3.75, 2.2)                    # spectrum -> VAD
    arrow(ax, 5.0, 1.7, 5.7, 1.7)                    # VAD -> noise estimate
    arrow(ax, 6.65, 2.2, 6.65, y)                    # noise estimate -> subtraction
    ax.text(3.85, 2.6, "power\nspectrum", fontsize=8, color=MUTED, va="center")
    ax.text(5.35, 1.82, "speech?", fontsize=8, color=MUTED, ha="center")

    ax.text(0.2, 0.75, "G = sqrt( max(P - alpha*N, beta*P) / P ),  then  G[t] = s*G[t-1] + (1-s)*G",
            fontsize=10, color=INK, family="monospace")
    ax.text(0.2, 0.25, "All buffers are preallocated: process_frame() does not allocate. "
            "Output is the input delayed by one hop; 10 to 20 ms from arrival to playable.", fontsize=9, color=MUTED)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT, dpi=150, bbox_inches="tight")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
