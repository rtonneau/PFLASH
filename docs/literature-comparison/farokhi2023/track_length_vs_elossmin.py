"""Primary/secondary track-length statistics vs. PrimaryKiller eLossMin threshold,
500 keV electron, 3 dissolved-O2 levels (0, 2.5, 5 %).

Usage: python track_length_vs_elossmin.py

Unlike track_length_vs_energy_loss.py (per-event scatter against each event's own
*actual* energy loss, 90 MeV proton, fixed box size), this script sweeps the
*configured* `/primaryKiller/eLossMin` threshold itself (1, 2.5, 5, 7.5, 10 % of the
500 keV beam energy) and reads each run's own pre-computed summary straight from
Manifest.json's `trackLengths` block (mean +/- SEM over events) -- no raw
TrackLengths.csv re-aggregation needed, since each eLossMin/O2 combination is already
one dump's worth of events.

Data: <G4_DATA_OUTPUT>/261010_Farokhi2023_electron_elossmin_tracklen/o2_{0,2p5,5}pct/
elossmin_{1,2p5,5,7p5,10}pct (250 events/run, 8 threads; macros
build/macro/beam_farokhi_electron_o2_*pct_elossmin_*pct.in). The 21 % O2 level and the
10 % eLossMin point at 5 % O2 are not included here: 21 % was not part of this sweep's
macro set, and 5pct/elossmin_10pct was still running (no Manifest.json yet) when this
script was written -- rerun to pick it up once it finishes.

Writes, next to this script:
  - track_length_vs_elossmin.png: 4 panels (primary length, secondary length all-gen,
    secondary count all-gen, secondary count first-gen) vs. eLossMin, one line per O2
    level, error bars = SEM over events.
  - track_length_vs_elossmin.md: the same data as a table.

Caveat: eLossMin truncates the primary once its *cumulative* energy loss reaches the
threshold, so a larger eLossMin lets the primary travel further and lose more energy
before being killed -- this is expected to show up as a monotonic increase in primary
length and secondary yield with eLossMin, not a physics result about O2 itself.

Result: the 3 O2 curves in every panel are bit-identical (visually a single line) --
confirming, at these track-length quantities, the project's known convention that the
physical/pre-chemical stage is bit-identical across O2 levels at fixed RNG seed and
beam settings (see CLAUDE.md's physical_stage_counts.md precedent), since O2 only acts
in the chemistry stage, downstream of TrackLengths.csv. This is not a bug in the sweep;
it means this script's O2 axis is a no-op for these quantities and the real signal here
is purely the eLossMin dependence.
"""

import json
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

G4_DATA_OUTPUT = Path(os.environ.get("G4_DATA_OUTPUT", r"D:\DATA\Geant4"))
SWEEP_DIR = G4_DATA_OUTPUT / "261010_Farokhi2023_electron_elossmin_tracklen"

O2_LEVELS = [("0 %", "o2_0pct"), ("2.5 %", "o2_2p5pct"), ("5 %", "o2_5pct")]
O2_COLORS = {"0 %": "tab:blue", "2.5 %": "tab:green", "5 %": "tab:red"}
O2_MARKERS = {"0 %": "o", "2.5 %": "s", "5 %": "^"}

ELOSSMIN_LEVELS = [(1.0, "elossmin_1pct"), (2.5, "elossmin_2p5pct"), (5.0, "elossmin_5pct"),
                   (7.5, "elossmin_7p5pct"), (10.0, "elossmin_10pct")]


def load_sweep():
    """Return {o2_label: [(elossmin_pct, trackLengths_dict), ...]} for finished runs only."""
    data = {}
    for o2_label, o2_dir in O2_LEVELS:
        points = []
        for elossmin_pct, elossmin_dir in ELOSSMIN_LEVELS:
            manifest_path = SWEEP_DIR / o2_dir / elossmin_dir / "Manifest.json"
            if not manifest_path.exists():
                continue
            with open(manifest_path, encoding="utf-8") as f:
                manifest = json.load(f)
            points.append((elossmin_pct, manifest["trackLengths"]))
        data[o2_label] = points
    return data


def plot_panels(out_path, data):
    fig, axes = plt.subplots(2, 2, figsize=(11, 8.5))
    panels = [
        (axes[0][0], "primaryLength_nm", 1.0 / 1000.0, "Primary track length (um)"),
        (axes[0][1], "secondaryAll_nm", 1.0 / 1000.0, "Secondary track length, all gen. (um)"),
        (axes[1][0], "nSecondaryAll", 1.0, "Secondary electron count, all gen."),
        (axes[1][1], "nSecondaryFirstGen", 1.0, "Secondary electron count, 1st gen."),
    ]
    for ax, key, scale, ylabel in panels:
        for o2_label, points in data.items():
            if not points:
                continue
            x = [p[0] for p in points]
            y = [p[1][key]["mean"] * scale for p in points]
            yerr = [p[1][key]["sem"] * scale for p in points]
            ax.errorbar(x, y, yerr=yerr, color=O2_COLORS[o2_label], marker=O2_MARKERS[o2_label],
                        capsize=3, label=f"O2 {o2_label}")
        ax.set_xlabel("PrimaryKiller eLossMin (% of 500 keV beam energy)")
        ax.set_ylabel(ylabel)
    axes[0][0].legend(loc="best", fontsize=9)
    fig.suptitle(
        "dnachem-min -- 500 keV electron, Farokhi2023, TrackLengths summary vs. "
        "PrimaryKiller eLossMin, 250 events/point, mean +/- SEM")
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def write_summary(out_path, data):
    lines = [
        "# Track-length statistics vs. PrimaryKiller eLossMin (500 keV electron, Farokhi2023)",
        "",
        "Mean +/- SEM over 250 events per point, from each run's Manifest.json `trackLengths`"
        " block. See the script docstring for caveats (eLossMin sweep is a truncation-threshold"
        " check, not a dose-effect result).",
        "",
        "| O2 | eLossMin (%) | Primary length (um) | Secondary length, all gen. (um) |"
        " Secondary count, all gen. | Secondary count, 1st gen. |",
        "|---|---|---|---|---|---|",
    ]
    for o2_label, points in data.items():
        for elossmin_pct, tl in points:
            pl = tl["primaryLength_nm"]
            sa = tl["secondaryAll_nm"]
            na = tl["nSecondaryAll"]
            nf = tl["nSecondaryFirstGen"]
            lines.append(
                f"| {o2_label} | {elossmin_pct} | "
                f"{pl['mean'] / 1000.0:.4f} +/- {pl['sem'] / 1000.0:.4f} | "
                f"{sa['mean'] / 1000.0:.2f} +/- {sa['sem'] / 1000.0:.2f} | "
                f"{na['mean']:.1f} +/- {na['sem']:.1f} | "
                f"{nf['mean']:.1f} +/- {nf['sem']:.1f} |"
            )
    lines += [
        "",
        "All O2 rows at a given eLossMin are bit-identical -- expected, see the script"
        " docstring (O2 only acts in the chemistry stage, downstream of these physical-stage"
        " track-length quantities).",
        "",
    ]
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    out_dir = Path(__file__).resolve().parent
    data = load_sweep()

    n_points = sum(len(points) for points in data.values())
    print(f"Loaded {n_points} finished run(s) out of {len(O2_LEVELS) * len(ELOSSMIN_LEVELS)}.")

    plot_panels(out_dir / "track_length_vs_elossmin.png", data)
    write_summary(out_dir / "track_length_vs_elossmin.md", data)

    print(f"Wrote {out_dir / 'track_length_vs_elossmin.png'}")
    print(f"Wrote {out_dir / 'track_length_vs_elossmin.md'}")


if __name__ == "__main__":
    main()
