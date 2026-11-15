#!/usr/bin/env python
"""Plots of the dose-rate study (PNG), from analyze.py output.

Usage:
    python plot.py --data <dir with gvalues_*.csv> [--out <dir>] [--dpi 150] [--species ...]
                   [--norm cum|final] [--ncols 4]

g_vs_time_<norm>.png: G(t), one panel per species, one line per dose rate, log t, SE band;
extrapolated rows (steady-state stop) drawn dashed.
g_vs_rate_<norm>.png: G at beam-off (1 Gy) vs dose rate, log x, error bars.
"""
import argparse
import csv
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

DISPLAY = {"e_aq^-1": "e$_{aq}^-$", "°OH^0": "·OH", "H^0": "H", "H_2^0": "H$_2$",
           "H2O2^0": "H$_2$O$_2$", "H3O^1": "H$_3$O$^+$", "OH^-1": "OH$^-$"}


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def num(text):
    return float(text) if text not in ("", "nan") else float("nan")


def panels(n, ncols, width=3.6, height=3.0):
    nrows = max(1, math.ceil(n / ncols))
    fig, axes = plt.subplots(nrows, ncols, figsize=(width * ncols, height * nrows), squeeze=False)
    flat = list(axes.flat)
    for ax in flat[n:]:
        ax.set_visible(False)
    return fig, flat


def plot_g_vs_time(rows, species, path, norm="cum", dpi=150, ncols=4):
    fig, axes = panels(len(species), ncols)
    rates = sorted({num(r["rate_Gy_s"]) for r in rows})
    colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    for ax, s in zip(axes, species):
        for i, rate in enumerate(rates):
            sel = sorted((r for r in rows if r["species"] == s and num(r["rate_Gy_s"]) == rate),
                         key=lambda r: num(r["time_ns"]))
            t = [num(r["time_ns"]) for r in sel]
            g = [num(r["G_" + norm]) for r in sel]
            e = [num(r["SE_" + norm]) for r in sel]
            x = [r["extrapolated"] == "1" for r in sel]
            c = colors[i % len(colors)]
            meas = [j for j in range(len(sel)) if not x[j]]
            ax.plot([t[j] for j in meas], [g[j] for j in meas], color=c, label="%g Gy/s" % rate)
            extra = [j for j in range(len(sel)) if x[j]]
            if extra:
                j0 = [extra[0] - 1] if extra[0] > 0 else []
                ax.plot([t[j] for j in j0 + extra], [g[j] for j in j0 + extra], color=c, ls="--")
            lo = [a - b if not math.isnan(b) else a for a, b in zip(g, e)]
            hi = [a + b if not math.isnan(b) else a for a, b in zip(g, e)]
            ax.fill_between(t, lo, hi, color=c, alpha=0.2, lw=0)
        ax.set_xscale("log")
        ax.set_title(DISPLAY.get(s, s))
        ax.set_xlabel("time (ns)")
        ax.set_ylabel("G (molecules / 100 eV)")
        ax.grid(alpha=0.3)
    if species:
        axes[0].legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(path, dpi=dpi)
    plt.close(fig)


def plot_g_vs_rate(rows, species, path, norm="cum", dpi=150, ncols=4):
    fig, axes = panels(len(species), ncols)
    for ax, s in zip(axes, species):
        sel = sorted((r for r in rows if r["species"] == s), key=lambda r: num(r["rate_Gy_s"]))
        ax.errorbar([num(r["rate_Gy_s"]) for r in sel], [num(r["G_" + norm]) for r in sel],
                    yerr=[0 if math.isnan(num(r["SE_" + norm])) else num(r["SE_" + norm]) for r in sel],
                    fmt="o-", capsize=3)
        ax.set_xscale("log")
        ax.set_title(DISPLAY.get(s, s))
        ax.set_xlabel("dose rate (Gy/s)")
        ax.set_ylabel("G at beam-off (molecules / 100 eV)")
        ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(path, dpi=dpi)
    plt.close(fig)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", required=True)
    parser.add_argument("--out")
    parser.add_argument("--dpi", type=int, default=150)
    parser.add_argument("--species", nargs="*")
    parser.add_argument("--norm", choices=("cum", "final"), default="cum")
    parser.add_argument("--ncols", type=int, default=4)
    args = parser.parse_args(argv)
    out = args.out or args.data
    os.makedirs(out, exist_ok=True)
    time_rows = read_rows(os.path.join(args.data, "gvalues_time.csv"))
    off_rows = read_rows(os.path.join(args.data, "gvalues_beamoff.csv"))
    species = args.species or list(dict.fromkeys(r["species"] for r in off_rows))
    plot_g_vs_time(time_rows, species, os.path.join(out, "g_vs_time_%s.png" % args.norm), args.norm, args.dpi, args.ncols)
    plot_g_vs_rate(off_rows, species, os.path.join(out, "g_vs_rate_%s.png" % args.norm), args.norm, args.dpi, args.ncols)
    print("wrote g_vs_time_%s.png and g_vs_rate_%s.png to %s" % (args.norm, args.norm, out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
