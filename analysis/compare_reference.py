#!/usr/bin/env python
"""Compare G(t) of a new dump against a reference dump.

Usage: compare_reference.py <reference_dir> <new_dir> [--tol 0.10] [--plot out.png]

Each directory holds Species_nt_species.csv and Manifest.json (a dump of sim).
A dump whose Manifest.json has "mesoEnabled": true (older dumps: "chemistryModel") also has
SpeciesMeso.csv: Species_nt_species.csv stops at the hand-over time and the
mesoscopic output (time_ns,species,count, counts summed over events) covers
the later times.

G(t) is in molecules per 100 eV:
  - Species_nt_species.csv rows: sumG / nEvent (mean of the per-event G, the
    estimator used by validation/reference/*/README.md);
  - SpeciesMeso.csv rows: count / (totalEnergyDeposit_eV / 100) (pooled; the
    file has no per-event variance).
G is compared at the latest time <= 1 us in each dump (reference: 999.999 ns,
new model: 1000 ns), for e_aq, OH, H2O2 and H2.

Exit code: 1 when any of the four relative differences exceeds --tol, else 0
(2 on missing or unreadable input). Run in conda env GEANT4_py311.
"""
import argparse
import json
import os
import sys

import pandas as pd

SPECIES = {
    "e_aq": "e_aq^-1",
    "OH": "°OH^0",
    "H2O2": "H2O2^0",
    "H2": "H_2^0",
}
T_LIMIT_NS = 1000.0 * (1.0 + 1e-5)  # 1 us, with margin for 999.999 / 1000 rounding


def load_dump(path):
    """Return (manifest, {csvName: DataFrame time_ns, G}, wallTime_s, isNewModel)."""
    with open(os.path.join(path, "Manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    energy = float(man["totalEnergyDeposit_eV"])
    n_event = int(man["totalEvents"])
    if energy <= 0 or n_event <= 0:
        raise ValueError(f"{path}: no energy deposit or events in Manifest.json")
    sp = pd.read_csv(os.path.join(path, "Species_nt_species.csv"), comment="#",
                     header=None, encoding="utf-8",
                     names=["speciesID", "number", "nEvent", "speciesName",
                            "time", "sumG", "sumG2"])
    series = {}
    for name, grp in sp.groupby("speciesName"):
        g = grp.sort_values("time")
        series[name] = pd.DataFrame({"time_ns": g["time"].to_numpy(),
                                     "G": (g["sumG"] / g["nEvent"]).to_numpy()})
    # mesoEnabled (manifest of the selectable-mode build) is false for SBS and
    # IRT_syn-only dumps: no SpeciesMeso.csv. Older dumps have chemistryModel only.
    is_new = bool(man.get("mesoEnabled", "chemistryModel" in man))
    if is_new:
        meso_path = os.path.join(path, "SpeciesMeso.csv")
        if not os.path.exists(meso_path):
            raise ValueError(f"{path}: new-model dump without SpeciesMeso.csv")
        meso = pd.read_csv(meso_path, encoding="utf-8")
        hand_over = float(man.get("handOverTime_ns", 0.0))
        for name, grp in meso.groupby("species"):
            g = grp.sort_values("time_ns")
            g = g[g["time_ns"] > hand_over]
            part = pd.DataFrame({"time_ns": g["time_ns"].to_numpy(),
                                 "G": (g["count"] / (energy / 100.0)).to_numpy()})
            old = series.get(name)
            if old is not None:
                old = old[old["time_ns"] <= hand_over]
                part = pd.concat([old, part], ignore_index=True)
            series[name] = part
    wall = sum(float(r.get("wallTime_s", 0.0)) for r in man.get("runs", []))
    return man, series, wall, is_new


def g_at_1us(series, name):
    df = series[name]
    df = df[df["time_ns"] <= T_LIMIT_NS]
    if df.empty:
        raise ValueError(f"no time <= 1 us for {name}")
    row = df.iloc[-1]
    return float(row["time_ns"]), float(row["G"])


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("reference_dir")
    ap.add_argument("new_dir")
    ap.add_argument("--tol", type=float, default=0.10,
                    help="max relative difference |new-ref|/ref (default 0.10)")
    ap.add_argument("--plot", metavar="out.png", help="write a G(t) overlay plot")
    args = ap.parse_args()

    try:
        ref_man, ref, ref_wall, ref_new = load_dump(args.reference_dir)
        new_man, new, new_wall, new_new = load_dump(args.new_dir)
    except (OSError, ValueError, KeyError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    def label(man, is_new):
        model = man.get("chemistryModel", "SBS")
        extra = f", hand-over {man['handOverTime_ns']:g} ns" if is_new else ""
        return (f"{model}{extra}, {man['totalEvents']} events, "
                f"{man.get('runMode', '?')} x{man.get('threads', '?')}")

    print(f"reference: {args.reference_dir} ({label(ref_man, ref_new)})")
    print(f"new:       {args.new_dir} ({label(new_man, new_new)})")
    print(f"{'species':<6} {'t_ref[ns]':>10} {'t_new[ns]':>10} {'G_ref':>9} "
          f"{'G_new':>9} {'rel.diff':>9}")
    worst = 0.0
    failed = False
    for key, csv_name in SPECIES.items():
        try:
            t_ref, g_ref = g_at_1us(ref, csv_name)
            t_new, g_new = g_at_1us(new, csv_name)
        except (KeyError, ValueError) as e:
            print(f"error: {key}: {e}", file=sys.stderr)
            return 2
        rel = (g_new - g_ref) / g_ref if g_ref != 0 else float("inf")
        worst = max(worst, abs(rel))
        bad = abs(rel) > args.tol
        failed = failed or bad
        print(f"{key:<6} {t_ref:>10.4f} {t_new:>10.4f} {g_ref:>9.4f} {g_new:>9.4f} "
              f"{rel * 100:>+8.2f}%{'  FAIL' if bad else ''}")
    ratio = new_wall / ref_wall if ref_wall > 0 else float("nan")
    print(f"wall time: reference {ref_wall:.1f} s, new {new_wall:.1f} s, "
          f"ratio new/reference {ratio:.3f}")
    print(f"max |rel.diff| = {worst * 100:.2f}% (tol {args.tol * 100:.1f}%): "
          f"{'FAIL' if failed else 'PASS'}")

    if args.plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, axes = plt.subplots(2, 2, figsize=(10, 7), sharex=True)
        for ax, (key, csv_name) in zip(axes.flat, SPECIES.items()):
            for series, man, is_new, style, lab in (
                    (ref, ref_man, ref_new, "-", "reference"),
                    (new, new_man, new_new, "--", "new")):
                df = series[csv_name]
                df = df[df["time_ns"] > 0]
                ax.plot(df["time_ns"], df["G"], style, label=lab)
            ax.set_xscale("log")
            ax.set_title(key)
            ax.set_ylabel("G [/100 eV]")
            ax.grid(alpha=0.3)
        for ax in axes[-1]:
            ax.set_xlabel("time [ns]")
        axes[0][0].legend()
        fig.suptitle("G(t): reference vs new")
        fig.tight_layout()
        fig.savefig(args.plot, dpi=130)
        print(f"plot written: {args.plot}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
