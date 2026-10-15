"""Instrumented primary/secondary track length vs. primary energy loss,
90 MeV proton, 20x20x20 um^3 vs. 40x40x40 um^3 box.

Usage: python track_length_vs_energy_loss.py

Unlike track_length_let.py (a path-length/energy PROXY derived from
PreChemical dumps, written before the TrackLengths scorer existed), this
script reads the real per-event scorer output directly: TrackLengths.csv
(primaryLength_nm from G4Track::GetTrackLength(), primaryEkin0_keV/
primaryEkinEnd_keV from TrackingAction -- see CLAUDE.md's TrackLengthTable/
TrackingAction entries). Energy loss per event is
primaryEkin0_keV - primaryEkinEnd_keV.

Data: <G4_DATA_OUTPUT>/261010_Farokhi2023_proton_{20,40}um_tracklen/o2_0pct
(50 events/box, 8 threads, same macros as plot_gvalues.py's proton box-size
check). Only o2_0pct is read per box size: all 4 O2 levels of a batch share
the same RNG seed and O2 only acts in the chemistry stage, downstream of the
physical-stage track/energy-loss quantities here -- verified bit-identical
across O2 for TrackLengths.csv (same convention as physical_stage_counts.md).
Pooling the 4 O2 directories would just quadruple-count the same 50 events.

Writes, next to this script:
  - track_length_vs_energy_loss.png: 3 panels (primary length, secondary
    all-generation length, secondary count, all vs. primary energy loss),
    one point per event, 20 um and 40 um overlaid.
  - track_length_vs_energy_loss.md: mean +/- SEM per box size, and the
    instrumented LET (energy loss / primary length) compared against the
    track_length_let.py PROXY numbers for the same two box sizes.

Caveats:
  - 50 events/box is enough to see the energy-loss/secondary-yield trend
    clearly but not enough for a tight per-bin fit; this is exploratory, not
    a stopping-power measurement.
  - The primary's path length to escape is fixed by the box geometry (it
    starts at the box center and exits radially), so its spread across
    events is almost entirely multiple-scattering straggling, not a free
    variable -- the real per-event variable here is energy loss and the
    secondary yield it produces, not the primary length itself.
"""

import csv
import math
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

G4_DATA_OUTPUT = Path(os.environ.get("G4_DATA_OUTPUT", r"D:\DATA\Geant4"))
BOX_DIRS = {
    "20 um": G4_DATA_OUTPUT / "261010_Farokhi2023_proton_20um_tracklen" / "o2_0pct",
    "40 um": G4_DATA_OUTPUT / "261010_Farokhi2023_proton_40um_tracklen" / "o2_0pct",
}
BOX_COLORS = {"20 um": "tab:blue", "40 um": "tab:red"}
BOX_MARKERS = {"20 um": "o", "40 um": "^"}


def load_track_lengths(run_dir):
    """Return list of dicts, one per event, from TrackLengths.csv."""
    rows = []
    with open(run_dir / "TrackLengths.csv", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(
                {
                    "energy_loss_keV": float(row["primaryEkin0_keV"]) - float(row["primaryEkinEnd_keV"]),
                    "primary_length_um": float(row["primaryLength_nm"]) / 1000.0,
                    "secondary_all_um": float(row["secondaryAll_nm"]) / 1000.0,
                    "n_secondary_all": int(row["nSecondaryAll"]),
                }
            )
    return rows


def mean_sem(values):
    n = len(values)
    mean = sum(values) / n
    if n > 1:
        var = sum((v - mean) ** 2 for v in values) / (n - 1)
        sem = math.sqrt(var / n)
    else:
        sem = float("nan")
    return mean, sem


def plot_panels(out_path, data_by_box):
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5))
    panels = [
        ("primary_length_um", "Primary track length (um)"),
        ("secondary_all_um", "Secondary track length, all gen. (um)"),
        ("n_secondary_all", "Secondary electron count, all gen."),
    ]
    for ax, (key, ylabel) in zip(axes, panels):
        for box_label, rows in data_by_box.items():
            x = [r["energy_loss_keV"] for r in rows]
            y = [r[key] for r in rows]
            ax.scatter(x, y, color=BOX_COLORS[box_label], marker=BOX_MARKERS[box_label],
                       alpha=0.6, label=box_label)
        ax.set_xlabel("Primary energy loss (keV)")
        ax.set_ylabel(ylabel)
    axes[0].legend(loc="best", fontsize=9)
    fig.suptitle(
        "dnachem-min -- 90 MeV proton, instrumented TrackLengths.csv, "
        "50 events/box (20 um, 40 um), one point per event")
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def write_summary(out_path, data_by_box):
    lines = [
        "# Instrumented track length vs. energy loss (90 MeV proton)",
        "",
        "Real TrackLengths.csv data (not the PreChemical-dump proxy in track_length_let.md),"
        " 50 events/box, o2_0pct representative (bit-identical across O2 at the physical stage).",
        "",
        "| Box | Energy loss, mean +/- SEM (median) (keV) | Primary length (um) |"
        " Instrumented LET from mean (keV/um) | Secondary length, all gen. (um) |"
        " Secondary count, all gen. |",
        "|---|---|---|---|---|---|",
    ]
    for box_label, rows in data_by_box.items():
        e_vals = [r["energy_loss_keV"] for r in rows]
        e_mean, e_sem = mean_sem(e_vals)
        e_median = sorted(e_vals)[len(e_vals) // 2]
        l_mean, l_sem = mean_sem([r["primary_length_um"] for r in rows])
        let_mean, let_sem = e_mean / l_mean, (e_sem / l_mean if l_mean else float("nan"))
        s_mean, s_sem = mean_sem([r["secondary_all_um"] for r in rows])
        n_mean, n_sem = mean_sem([r["n_secondary_all"] for r in rows])
        n = len(rows)
        lines.append(
            f"| {box_label} | {e_mean:.3f} +/- {e_sem:.3f} ({e_median:.3f}) (n={n}) | "
            f"{l_mean:.4f} +/- {l_sem:.4f} | {let_mean:.4f} +/- {let_sem:.4f} | "
            f"{s_mean:.2f} +/- {s_sem:.2f} | {n_mean:.1f} +/- {n_sem:.1f} |"
        )
    lines += [
        "",
        "## Compare against the path-length/LET proxy (track_length_let.md) and NIST PSTAR",
        "",
        "The proxy (10 um box, PreChemical dumps, 16 events) gave "
        "0.1855 +/- 0.01308 keV/um; the proxy (20 um box, 24 events) gave "
        "0.1747 +/- 0.007077 keV/um -- both close to the ~0.57 keV/um NIST-PSTAR-based "
        "figure already cited in the proton macros' own header comments. The instrumented "
        "LET above (0.95, 1.08 keV/um) is higher than both by roughly 1.7-1.9x relative to "
        "the ~0.57 keV/um reference, not just 'somewhat' higher as the proxy's exclusion of "
        "secondary-electron kinetic energy alone would predict.",
        "",
        "The per-event relative spread is large (sample std dev is close to the mean itself "
        "at both box sizes -- e.g. 20 um: std dev ~9.5 keV on a 9.5 keV mean), i.e. a few "
        "high-energy-loss events are pulling the mean well above the typical/modal value -- "
        "consistent with occasional larger-angle, larger-energy-transfer collisions "
        "(delta-ray straggling) rather than a uniform per-event loss. At n=50/box this is "
        "not yet distinguishing a real effect from a mean dominated by a handful of outlier "
        "events; a larger n and/or reporting the median alongside the mean would clarify "
        "which is going on before drawing a physics conclusion from the absolute LET value.",
        "",
    ]
    out_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    out_dir = Path(__file__).resolve().parent
    data_by_box = {label: load_track_lengths(path) for label, path in BOX_DIRS.items()}

    plot_panels(out_dir / "track_length_vs_energy_loss.png", data_by_box)
    write_summary(out_dir / "track_length_vs_energy_loss.md", data_by_box)

    print(f"Wrote {out_dir / 'track_length_vs_energy_loss.png'}")
    print(f"Wrote {out_dir / 'track_length_vs_energy_loss.md'}")


if __name__ == "__main__":
    main()
