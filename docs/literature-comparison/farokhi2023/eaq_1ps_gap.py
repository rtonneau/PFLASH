"""e_aq G(t) at 0 % O2, 500 keV e-: why dnachem-min was ~3.4 % below Farokhi at 1 ps.

Sources (all in the repo, no simulation re-run):
  - Ritchie1994  : column IRT_syn of ../simulation_vs_literature_comparison.csv (e_aq, O2_pct 0)
  - Meesungnoen  : data/meesungnoen_eaq.csv (this macro: beam_farokhi_electron_o2_0pct_meesungnoen.in)
  - stock        : data/stock_scavenger_eaq.csv (unmodified Geant4 11.4.1 scavenger example)
  - Farokhi      : hand-digitized Fig. 5, column Farokhi of the comparison CSV (+-0.1-0.2 reading error)

To rebuild data/meesungnoen_eaq.csv from a sim output directory:
  python eaq_1ps_gap.py --from-ntuple <dir>/Species_nt_species.csv

Writes eaq_1ps_gap.png and eaq_1ps_gap.md next to itself.
"""
import argparse
import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
COMPARISON = os.path.join(HERE, "..", "simulation_vs_literature_comparison.csv")
MEESUNGNOEN = os.path.join(HERE, "data", "meesungnoen_eaq.csv")
STOCK = os.path.join(HERE, "data", "stock_scavenger_eaq.csv")
T_MAX = 1.0e-9
FINDING = """## Finding

The 3.3 % gap to the stock Geant4-DNA `scavenger` example (and 4 % to Farokhi) was mostly the electron solvation subtype:
dnachem-min's Farokhi macros set `Ritchie1994`, the example sets `Meesungnoen2002`. With `Meesungnoen2002`
(`macro/beam_farokhi_electron_o2_0pct_meesungnoen.in`, 250 events, eLossMin 12.5 keV) dnachem-min is 0.6 % below the stock run,
inside the 1 % target, and 1.5 % below the digitized Farokhi value, inside its reading error. The remainder is consistent with
the B1A1 autoionisation probability (0.55 stock vs 0.50 option1), see `dissociation_channels_comparison.md`; it was not isolated.
Boscolo (4.49) comes from a different code and is not a target.
"""


def from_ntuple(path):
    """e_aq mean G and SEM per recorded time (ns -> s) from a Species_nt_species.csv."""
    rows = []
    for line in open(path):
        if line.startswith("#") or "e_aq" not in line:
            continue
        f = line.split(",")
        n, t_ns, s, s2 = int(f[2]), float(f[4]), float(f[5]), float(f[6])
        mean = s / n
        var = (s2 - n * mean * mean) / (n - 1)
        rows.append((t_ns * 1e-9, mean, math.sqrt(var / n), n))
    rows.sort()
    return rows


def read_csv(path, tcol, gcol, ecol=None):
    out = []
    for r in csv.DictReader(open(path)):
        t = float(r[tcol])
        out.append((t, float(r[gcol]), float(r[ecol]) if ecol else float("nan")))
    return out


def comparison_column(col):
    out = []
    for r in csv.DictReader(open(COMPARISON)):
        if r["species"] == "e_aq" and float(r["O2_pct"]) == 0.0 and r[col] != "":
            t = float(r["time_s"])
            if t <= T_MAX * 1.01:
                out.append((t, float(r[col]), float("nan")))
    return out


def at_1ps(series):
    return min(series, key=lambda x: abs(math.log10(x[0]) + 12))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from-ntuple")
    args = ap.parse_args()
    if args.from_ntuple:
        with open(MEESUNGNOEN, "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["time_s", "G_eaq_per_100eV", "SEM", "n_events"])
            for t, g, e, n in from_ntuple(args.from_ntuple):
                w.writerow([f"{t:.6e}", f"{g:.6f}", f"{e:.6f}", n])

    series = {
        "dnachem-min, Ritchie1994": comparison_column("IRT_syn"),
        "dnachem-min, Meesungnoen2002": read_csv(MEESUNGNOEN, "time_s", "G_eaq_per_100eV", "SEM"),
        "stock scavenger (Meesungnoen2002)": read_csv(STOCK, "time_s", "G_eaq_per_100eV", "SEM"),
        "Farokhi 2023 (digitized)": comparison_column("Farokhi"),
    }
    stock_1ps = at_1ps(series["stock scavenger (Meesungnoen2002)"])[1]

    fig, ax = plt.subplots(figsize=(7, 4.5))
    for name, s in series.items():
        s = [x for x in s if x[0] <= T_MAX * 1.01]
        ax.plot([x[0] for x in s], [x[1] for x in s], label=name,
                linestyle=":" if "Farokhi" in name else "-")
    ax.set_xscale("log")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("G(e_aq) (molecules / 100 eV)")
    ax.set_title("e_aq, 500 keV e-, 0 % O2")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(os.path.join(HERE, "eaq_1ps_gap.png"), dpi=150)

    lines = ["# e_aq G at 1 ps, 500 keV e-, 0 % O2", "",
             "| Source | G(1 ps) | SEM | vs stock |", "|---|---|---|---|"]
    for name, s in series.items():
        t, g, e = at_1ps(s)
        sem = "" if math.isnan(e) else f"{e:.3f}"
        lines.append(f"| {name} | {g:.3f} | {sem} | {100 * (g - stock_1ps) / stock_1ps:+.1f} % |")
        print(f"{name:40s} G(1 ps)={g:.3f} {sem} ({100 * (g - stock_1ps) / stock_1ps:+.1f} % vs stock)")
    lines += ["", "Farokhi's digitized value carries a reading error of about 0.1-0.2 G units (about 3.5 %).",
              "![e_aq](eaq_1ps_gap.png)", "", FINDING]
    open(os.path.join(HERE, "eaq_1ps_gap.md"), "w").write("\n".join(lines))


if __name__ == "__main__":
    main()
