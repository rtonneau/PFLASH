#!/usr/bin/env python
"""Plots of the dose-rate study (PNG).

Usage:
    python plot.py --data <dir with gvalues_*.csv> [--out <dir>] [--dpi 150] [--species A B ...]

Writes g_vs_rate.png (G at the endpoint vs dose rate, one panel per species, log x, error bars)
and g_vs_time.png (G(t), one panel per species, one line per dose rate, log t).
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


def _num(text):
    return float(text) if text not in ("", "nan") else float("nan")


def _panels(species, ncols, width, height):
    nrows = math.ceil(len(species) / ncols)
    fig, axes = plt.subplots(nrows, ncols, figsize=(width * ncols, height * nrows), squeeze=False)
    flat = list(axes.flat)
    for ax in flat[len(species):]:
        ax.set_visible(False)
    return fig, flat


def plot_g_vs_rate(rows, species, path, dpi=150, ncols=4, width=3.6, height=3.0):
    fig, axes = _panels(species, ncols, width, height)
    for ax, s in zip(axes, species):
        sel = sorted((r for r in rows if r["species"] == s), key=lambda r: _num(r["rate_Gy_s"]))
        ax.errorbar([_num(r["rate_Gy_s"]) for r in sel], [_num(r["G_per_100eV"]) for r in sel],
                    yerr=[_num(r["SE"]) for r in sel], fmt="o-", capsize=3)
        ax.set_xscale("log")
        ax.set_title(DISPLAY.get(s, s))
        ax.set_xlabel("dose rate (Gy/s)")
        ax.set_ylabel("G (molecules / 100 eV)")
        ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(path, dpi=dpi)
    plt.close(fig)


def plot_g_vs_time(rows, species, path, dpi=150, ncols=4, width=3.6, height=3.0):
    fig, axes = _panels(species, ncols, width, height)
    rates = sorted({_num(r["rate_Gy_s"]) for r in rows})
    for ax, s in zip(axes, species):
        for rate in rates:
            sel = sorted((r for r in rows if r["species"] == s and _num(r["rate_Gy_s"]) == rate),
                         key=lambda r: _num(r["time_ns"]))
            ax.plot([_num(r["time_ns"]) for r in sel], [_num(r["G_per_100eV"]) for r in sel],
                    label="%g Gy/s" % rate)
        ax.set_xscale("log")
        ax.set_title(DISPLAY.get(s, s))
        ax.set_xlabel("time (ns)")
        ax.set_ylabel("G (molecules / 100 eV)")
        ax.grid(alpha=0.3)
    axes[0].legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(path, dpi=dpi)
    plt.close(fig)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", required=True, help="directory with gvalues_time.csv and gvalues_1ms.csv")
    parser.add_argument("--out", help="output directory (default: --data)")
    parser.add_argument("--dpi", type=int, default=150)
    parser.add_argument("--species", nargs="*", help="species to plot (default: all in the files)")
    parser.add_argument("--ncols", type=int, default=4)
    args = parser.parse_args(argv)
    out = args.out or args.data
    os.makedirs(out, exist_ok=True)
    time_rows = read_rows(os.path.join(args.data, "gvalues_time.csv"))
    end_rows = read_rows(os.path.join(args.data, "gvalues_1ms.csv"))
    species = args.species or list(dict.fromkeys(r["species"] for r in end_rows))
    plot_g_vs_rate(end_rows, species, os.path.join(out, "g_vs_rate.png"), args.dpi, args.ncols)
    plot_g_vs_time(time_rows, species, os.path.join(out, "g_vs_time.png"), args.dpi, args.ncols)
    print("wrote g_vs_rate.png and g_vs_time.png to " + out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
