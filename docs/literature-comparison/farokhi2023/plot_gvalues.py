"""Farokhi2023 G-value time evolution vs. Farokhi et al. 2023, with a
statistical-adequacy check on the underlying Monte Carlo runs.

Paper: Farokhi et al., Radiat. Phys. Chem. 212 (2023) 111184,
docs/literature/Farokhi2023_Effects_of_the_Oxygen_depletion_in_FLASH_irradiation_investigated_through_geant4_DNA_toolkit.pdf.

Usage: python plot_gvalues.py
Reads the production dnachem-min runs from <G4_DATA_OUTPUT>/<RUN_DIR_NAME>
(environment variable G4_DATA_OUTPUT, falling back to D:\\DATA\\Geant4 if
unset -- machine-specific, see README.md):

- 261009_Farokhi2023_proton/o2_{0,3,7,21}pct   -- 90 MeV proton, Farokhi2023
  chemistry, 10x10x10 um^3 box, 16 events each, 8 threads, /scheduler/endTime
  1 us, 61 log-spaced recording times (10/decade, 1 ps to 1 us).
- 261009_Farokhi2023_electron/o2_{0,2p5,5,21}pct -- 500 keV electron,
  Farokhi2023 chemistry, default 500 um box, PrimaryKiller eLossMin 5 keV,
  56 events each, 8 threads, same end time and recording grid.
- 261010_Farokhi2023_proton_20um_tracklen/o2_{0,3,7,21}pct -- same 90 MeV
  proton setup, but a 20x20x20 um^3 box (/chem/env/halfBox 10 um) and 50
  events each, 8 threads, run after the TrackLengths scorer landed (also
  used by track_length_vs_energy_loss.py). Supersedes the earlier
  261010_Farokhi2023_proton_20um/ (24 events, no TrackLengths output).
- 261010_Farokhi2023_proton_40um_tracklen/o2_{0,3,7,21}pct -- same setup, a
  40x40x40 um^3 box (/chem/env/halfBox 20 um), 50 events each, 8 threads.

Writes, next to this script:
  - farokhi2023_proton_gvalues.png / farokhi2023_electron_gvalues.png:
    mean G(t) per species (shaded band = +-1 standard error of the mean,
    SEM), one panel per species, all O2 levels overlaid, with hand-digitized
    reference points from the paper's Fig. 2 (21 % O2 panel) / Fig. 5 (all 4
    panels) for comparison.
  - farokhi2023_proton_relerr.png / farokhi2023_electron_relerr.png:
    relative statistical uncertainty (SEM / mean, %) vs. time for the same
    species/O2-level combinations -- the "is there enough statistics" check.
  - farokhi2023_proton_boxsize_gvalues.png: the same proton species panels,
    10x10x10 um^3 (16 events, solid), 20x20x20 um^3 (50 events, dashed) and
    40x40x40 um^3 (50 events, dotted) overlaid per O2 level, to see whether
    the diffusion boundary at 10/20 um half-box is biasing the result.
  - statistics_summary.md: a plain-text table of the worst (highest) relative
    SEM observed per species/O2-level/particle, and the fraction of the 61
    recorded times where it exceeds 25 %.

Mean and SEM per (run, species, time) come directly from the ntuple
dnachem-min already writes (Species_nt_species.csv: nEvent, sumG, sumG2,
where sumG/sumG2 are the sum/sum-of-squares of each EVENT's own G-value,
n_mol / eventEdep_eV * 100, src/scoring/ScoreSpecies.cc:164) -- no re-running
of the simulation, no re-derivation of G from a run-level ratio:

    mean = sumG / n
    sample variance = (sumG2 - n * mean^2) / (n - 1)   (Bessel-corrected)
    SEM = sqrt(sample variance / n)

Caveats -- this comparison is indicative only, not a reproduction (same as
the earlier session-local check this directory supersedes for Farokhi2023):
  - dnachem-min models a single homogeneous water box (10x10x10 um^3 for the
    proton runs, 500 um default for the electron runs), not the paper's
    nested sensitive-volume geometry.
  - dnachem-min's PhysicsList uses G4EmDNAPhysics_option2; the paper does not
    state which G4EmDNAPhysics option (if any beyond "Geant4-DNA") it used.
  - The reference points are hand-digitized off rendered PDF pages (read
    visually, not with a digitizing tool), carrying a reading error of order
    0.1-0.2 G-value units on top of whatever the paper's own figures carry.
  - Event counts (16 proton, 56 electron) are modest; see
    statistics_summary.md and the *_relerr.png plots for exactly where that
    does and does not matter.
"""

import csv
import math
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ---------------------------------------------------------------------------
# Production run locations (machine-specific; see README.md).

G4_DATA_OUTPUT = Path(os.environ.get("G4_DATA_OUTPUT", r"D:\DATA\Geant4"))
PROTON_BASE = G4_DATA_OUTPUT / "261009_Farokhi2023_proton"
ELECTRON_BASE = G4_DATA_OUTPUT / "261009_Farokhi2023_electron"
PROTON_20UM_BASE = G4_DATA_OUTPUT / "261010_Farokhi2023_proton_20um_tracklen"
PROTON_40UM_BASE = G4_DATA_OUTPUT / "261010_Farokhi2023_proton_40um_tracklen"

PROTON_RUNS = {0: "o2_0pct", 3: "o2_3pct", 7: "o2_7pct", 21: "o2_21pct"}
ELECTRON_RUNS = {0: "o2_0pct", 2.5: "o2_2p5pct", 5: "o2_5pct", 21: "o2_21pct"}
PROTON_20UM_RUNS = PROTON_RUNS
PROTON_40UM_RUNS = PROTON_RUNS
O2_COLORS = {0: "tab:blue", 3: "tab:green", 2.5: "tab:green", 7: "tab:orange", 5: "tab:orange", 21: "magenta"}

SPECIES_CSV_NAME = "Species_nt_species.csv"

SPECIES_CSV_LABEL = {
    "e_aq": "e_aq^-1",
    "OH": "\u00b0OH^0",
    "H": "H^0",
    "H3Op": "H3O^1",
    "H2": "H_2^0",
    "H2O2": "H2O2^0",
    "O2m": "O_2^-1",
    "HO2": "HO_2\u00b0^0",
}

RELERR_THRESHOLD_PCT = 25.0


def load_series(run_dir):
    """Return {csv speciesName: [(time_s, mean_G, sem_G, nEvent), ...]} sorted by time."""
    path = run_dir / SPECIES_CSV_NAME
    series = {}
    with open(path, newline="", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            row = next(csv.reader([line]))
            name = row[3]
            time_s = float(row[4]) * 1e-9
            n = int(row[2])
            sum_g = float(row[5])
            sum_g2 = float(row[6])
            mean = sum_g / n
            if n > 1:
                var = (sum_g2 - n * mean * mean) / (n - 1)
                var = max(var, 0.0)
                sem = math.sqrt(var / n)
            else:
                sem = float("nan")
            series.setdefault(name, []).append((time_s, mean, sem, n))
    for pts in series.values():
        pts.sort()
    return series


# ---------------------------------------------------------------------------
# Hand-digitized reference points (molecules/100 eV vs. time in seconds).
# Read by eye off rendered pages of the Farokhi2023 PDF -- approximate, not
# extracted with a digitizing tool.

REFERENCE_PROTON_21PCT = {
    "OH": [(1e-12, 5.35), (1e-10, 4.95), (1e-9, 4.7), (1e-8, 3.9), (1e-7, 3.3), (1e-6, 3.15)],
    "H3Op": [(1e-12, 4.25), (1e-10, 4.15), (1e-9, 4.05), (1e-8, 3.85), (1e-7, 3.72), (1e-6, 3.68)],
    "H": [(1e-12, 0.65), (1e-10, 0.60), (1e-9, 0.57), (1e-8, 0.53), (1e-7, 0.35), (1e-6, 0.07)],
    "e_aq": [(1e-12, 4.3), (1e-10, 4.2), (1e-9, 4.0), (1e-8, 3.5), (1e-7, 1.5), (1e-6, 0.1)],
    "H2": [(1e-12, 0.1), (1e-10, 0.15), (1e-9, 0.2), (1e-8, 0.3), (1e-7, 0.4), (1e-6, 0.48)],
    "H2O2": [(1e-12, 0.05), (1e-10, 0.2), (1e-9, 0.35), (1e-8, 0.6), (1e-7, 0.68), (1e-6, 0.68)],
    "HO2": [(1e-9, 0.02), (1e-8, 0.08), (1e-7, 0.25), (1e-6, 0.4)],
    "O2m": [(1e-9, 0.03), (1e-8, 0.15), (1e-7, 1.3), (1e-6, 3.1)],
}

REFERENCE_ELECTRON = {
    "e_aq": {
        0: [(1e-12, 4.2), (1e-10, 4.1), (1e-9, 3.95), (1e-8, 3.55), (1e-7, 3.1), (1e-6, 2.85)],
        2.5: [(1e-12, 4.2), (1e-10, 4.1), (1e-9, 3.9), (1e-8, 3.5), (1e-7, 2.6), (1e-6, 1.65)],
        5: [(1e-12, 4.2), (1e-10, 4.1), (1e-9, 3.85), (1e-8, 3.4), (1e-7, 2.3), (1e-6, 1.0)],
        21: [(1e-12, 4.2), (1e-10, 4.05), (1e-9, 3.75), (1e-8, 2.9), (1e-7, 1.1), (1e-6, 0.1)],
    },
    "H": {
        0: [(1e-12, 0.635), (1e-10, 0.60), (1e-9, 0.555), (1e-8, 0.56), (1e-7, 0.60), (1e-6, 0.61)],
        2.5: [(1e-12, 0.635), (1e-10, 0.60), (1e-9, 0.555), (1e-8, 0.56), (1e-7, 0.58), (1e-6, 0.37)],
        5: [(1e-12, 0.635), (1e-10, 0.60), (1e-9, 0.555), (1e-8, 0.55), (1e-7, 0.52), (1e-6, 0.23)],
        21: [(1e-12, 0.635), (1e-10, 0.60), (1e-9, 0.555), (1e-8, 0.54), (1e-7, 0.33), (1e-6, 0.07)],
    },
    "O2m": {
        2.5: [(1e-9, 0.0), (1e-8, 0.05), (1e-7, 0.4), (1e-6, 1.25)],
        5: [(1e-9, 0.0), (1e-8, 0.08), (1e-7, 0.55), (1e-6, 2.05)],
        21: [(1e-9, 0.0), (1e-8, 0.15), (1e-7, 1.1), (1e-6, 2.95)],
    },
    "HO2": {
        2.5: [(1e-9, 0.0), (1e-8, 0.01), (1e-7, 0.08), (1e-6, 0.25)],
        5: [(1e-9, 0.0), (1e-8, 0.02), (1e-7, 0.12), (1e-6, 0.38)],
        21: [(1e-9, 0.0), (1e-8, 0.03), (1e-7, 0.2), (1e-6, 0.50)],
    },
}


def plot_gvalues_proton(out_path, runs):
    species_list = ["OH", "H3Op", "H", "e_aq", "H2", "H2O2", "HO2", "O2m"]
    fig, axes = plt.subplots(2, 4, figsize=(18, 7.5), sharex=True)
    for ax, label in zip(axes.flat, species_list):
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in PROTON_RUNS:
            pts = runs[o2].get(csv_name, [])
            if not pts:
                continue
            t = [p[0] for p in pts]
            mean = [p[1] for p in pts]
            sem = [p[2] if not math.isnan(p[2]) else 0.0 for p in pts]
            lo = [m - s for m, s in zip(mean, sem)]
            hi = [m + s for m, s in zip(mean, sem)]
            ax.plot(t, mean, color=O2_COLORS[o2], label=f"dnachem-min {o2}% O2")
            ax.fill_between(t, lo, hi, color=O2_COLORS[o2], alpha=0.25, linewidth=0)
        ref = REFERENCE_PROTON_21PCT.get(label)
        if ref:
            rt, rg = zip(*ref)
            ax.scatter(rt, rg, color="black", marker="x", zorder=5,
                       label="Farokhi 2023 Fig.2 21% O2 (digitized)")
        ax.set_xscale("log")
        ax.set_xlim(1e-12, 1e-6)
        ax.set_ylim(bottom=0)
        ax.set_title(label)
        ax.set_xlabel("Time (s)")
    axes[0, 0].set_ylabel("G-value (molecules/100 eV)")
    axes[1, 0].set_ylabel("G-value (molecules/100 eV)")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=5, bbox_to_anchor=(0.5, -0.02))
    fig.suptitle(
        "dnachem-min vs. Farokhi et al. 2023 Fig. 2/3 -- 90 MeV proton, 16 events/run, "
        "shaded = +-1 SEM")
    fig.tight_layout(rect=(0, 0.05, 1, 0.95))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_gvalues_electron(out_path, runs):
    species_list = ["e_aq", "O2m", "H", "HO2"]
    fig, axes = plt.subplots(1, 4, figsize=(18, 4.5))
    for ax, label in zip(axes, species_list):
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in ELECTRON_RUNS:
            pts = runs[o2].get(csv_name, [])
            if not pts:
                continue
            t = [p[0] for p in pts]
            mean = [p[1] for p in pts]
            sem = [p[2] if not math.isnan(p[2]) else 0.0 for p in pts]
            lo = [m - s for m, s in zip(mean, sem)]
            hi = [m + s for m, s in zip(mean, sem)]
            ax.plot(t, mean, color=O2_COLORS[o2], label=f"dnachem-min {o2}% O2")
            ax.fill_between(t, lo, hi, color=O2_COLORS[o2], alpha=0.25, linewidth=0)
            ref = REFERENCE_ELECTRON.get(label, {}).get(o2)
            if ref:
                rt, rg = zip(*ref)
                ax.scatter(rt, rg, color=O2_COLORS[o2], marker="x", zorder=5,
                           label=f"Farokhi 2023 Fig.5 {o2}% O2 (digitized)")
        ax.set_xscale("log")
        ax.set_xlim(1e-12, 1e-6)
        ax.set_ylim(bottom=0)
        ax.set_title(label)
        ax.set_xlabel("Time (s)")
    axes[0].set_ylabel("G-value (molecules/100 eV)")
    handles, labels = axes[0].get_legend_handles_labels()
    seen = {}
    for h, l in zip(handles, labels):
        seen.setdefault(l, h)
    fig.legend(seen.values(), seen.keys(), loc="lower center", ncol=4, bbox_to_anchor=(0.5, -0.14), fontsize=8)
    fig.suptitle(
        "dnachem-min vs. Farokhi et al. 2023 Fig. 5 -- 500 keV electron, 56 events/run, "
        "shaded = +-1 SEM")
    fig.tight_layout(rect=(0, 0.2, 1, 0.92))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_proton_boxsize_comparison(out_path, runs_10um, runs_20um, runs_40um):
    """10x10x10 um^3 (16 ev) vs. 20x20x20 um^3 (50 ev) vs. 40x40x40 um^3 (50 ev), per O2 level."""
    species_list = ["OH", "H3Op", "H", "e_aq", "H2", "H2O2", "HO2", "O2m"]
    box_series = [
        (runs_10um, "-", "10 um", 16),
        (runs_20um, "--", "20 um", 50),
        (runs_40um, ":", "40 um", 50),
    ]
    fig, axes = plt.subplots(2, 4, figsize=(18, 7.5), sharex=True)
    for ax, label in zip(axes.flat, species_list):
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in PROTON_RUNS:
            for runs, linestyle, box_label, n_ev in box_series:
                pts = runs[o2].get(csv_name, [])
                if not pts:
                    continue
                t = [p[0] for p in pts]
                mean = [p[1] for p in pts]
                sem = [p[2] if not math.isnan(p[2]) else 0.0 for p in pts]
                lo = [m - s for m, s in zip(mean, sem)]
                hi = [m + s for m, s in zip(mean, sem)]
                ax.plot(t, mean, color=O2_COLORS[o2], linestyle=linestyle,
                         label=f"{box_label}, {o2}% O2 ({n_ev} ev)")
                ax.fill_between(t, lo, hi, color=O2_COLORS[o2], alpha=0.2, linewidth=0)
        ax.set_xscale("log")
        ax.set_xlim(1e-12, 1e-6)
        ax.set_ylim(bottom=0)
        ax.set_title(label)
        ax.set_xlabel("Time (s)")
    axes[0, 0].set_ylabel("G-value (molecules/100 eV)")
    axes[1, 0].set_ylabel("G-value (molecules/100 eV)")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    seen = {}
    for h, l in zip(handles, labels):
        seen.setdefault(l, h)
    fig.legend(seen.values(), seen.keys(), loc="lower center", ncol=4, bbox_to_anchor=(0.5, -0.05), fontsize=8)
    fig.suptitle(
        "dnachem-min box-size check -- 90 MeV proton, 10x10x10 um^3 (solid, 16 ev) vs. "
        "20x20x20 um^3 (dashed, 50 ev) vs. 40x40x40 um^3 (dotted, 50 ev), shaded = +-1 SEM")
    fig.tight_layout(rect=(0, 0.08, 1, 0.95))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_relative_error(out_path, runs, species_list, run_levels, title):
    """SEM / mean (%) vs. time, one panel per species, all O2 levels overlaid."""
    n = len(species_list)
    fig, axes = plt.subplots(1, n, figsize=(4.5 * n, 4.2), sharey=True)
    if n == 1:
        axes = [axes]
    for ax, label in zip(axes, species_list):
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in run_levels:
            pts = runs[o2].get(csv_name, [])
            if not pts:
                continue
            t, relerr = [], []
            for time_s, mean, sem, _n in pts:
                if mean <= 0 or math.isnan(sem):
                    continue
                t.append(time_s)
                relerr.append(100.0 * sem / mean)
            if t:
                ax.plot(t, relerr, color=O2_COLORS[o2], marker=".", label=f"{o2}% O2")
        ax.axhline(RELERR_THRESHOLD_PCT, color="red", linestyle="--", linewidth=1,
                   label=f"{RELERR_THRESHOLD_PCT:.0f}% threshold" if label == species_list[0] else None)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlim(1e-12, 1e-6)
        ax.set_title(label)
        ax.set_xlabel("Time (s)")
    axes[0].set_ylabel("Relative SEM (%) = SEM(G) / mean(G) x 100")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=min(len(run_levels) + 1, 5),
               bbox_to_anchor=(0.5, -0.08))
    fig.suptitle(title)
    fig.tight_layout(rect=(0, 0.12, 1, 0.92))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def summarize_statistics(runs, species_list, run_levels, particle_name, n_events):
    """Return markdown lines: worst relative SEM and fraction of points over threshold."""
    lines = [
        f"### {particle_name} ({n_events} events/run)",
        "",
        "| Species | O2 level | Max relative SEM (%) | Fraction of 61 times > "
        f"{RELERR_THRESHOLD_PCT:.0f}% | Time of worst point |",
        "|---|---|---|---|---|",
    ]
    for label in species_list:
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in run_levels:
            pts = runs[o2].get(csv_name, [])
            worst_relerr = -1.0
            worst_t = None
            n_over = 0
            n_valid = 0
            for time_s, mean, sem, _n in pts:
                if mean <= 0 or math.isnan(sem):
                    continue
                n_valid += 1
                relerr = 100.0 * sem / mean
                if relerr > RELERR_THRESHOLD_PCT:
                    n_over += 1
                if relerr > worst_relerr:
                    worst_relerr = relerr
                    worst_t = time_s
            frac = f"{n_over}/{n_valid}" if n_valid else "n/a (always 0)"
            worst_str = f"{worst_relerr:.1f}" if worst_relerr >= 0 else "n/a"
            t_str = f"{worst_t:.2e} s" if worst_t is not None else "n/a"
            lines.append(f"| {label} | {o2}% | {worst_str} | {frac} | {t_str} |")
    lines.append("")
    return lines


def main():
    out_dir = Path(__file__).resolve().parent

    proton_runs = {o2: load_series(PROTON_BASE / name) for o2, name in PROTON_RUNS.items()}
    electron_runs = {o2: load_series(ELECTRON_BASE / name) for o2, name in ELECTRON_RUNS.items()}
    proton_20um_runs = {o2: load_series(PROTON_20UM_BASE / name) for o2, name in PROTON_20UM_RUNS.items()}
    proton_40um_runs = {o2: load_series(PROTON_40UM_BASE / name) for o2, name in PROTON_40UM_RUNS.items()}

    plot_gvalues_proton(out_dir / "farokhi2023_proton_gvalues.png", proton_runs)
    plot_gvalues_electron(out_dir / "farokhi2023_electron_gvalues.png", electron_runs)
    plot_proton_boxsize_comparison(
        out_dir / "farokhi2023_proton_boxsize_gvalues.png", proton_runs, proton_20um_runs, proton_40um_runs)

    proton_species = ["OH", "H3Op", "H", "e_aq", "H2", "H2O2", "HO2", "O2m"]
    electron_species = ["e_aq", "O2m", "H", "HO2"]
    plot_relative_error(
        out_dir / "farokhi2023_proton_relerr.png", proton_runs, proton_species, PROTON_RUNS,
        "Statistical adequacy -- 90 MeV proton, 16 events/run")
    plot_relative_error(
        out_dir / "farokhi2023_electron_relerr.png", electron_runs, electron_species, ELECTRON_RUNS,
        "Statistical adequacy -- 500 keV electron, 56 events/run")

    summary = ["# Statistics summary", ""]
    summary += summarize_statistics(proton_runs, proton_species, PROTON_RUNS, "Proton (90 MeV)", 16)
    summary += summarize_statistics(electron_runs, electron_species, ELECTRON_RUNS, "Electron (500 keV)", 56)
    (out_dir / "statistics_summary.md").write_text("\n".join(summary), encoding="utf-8")

    print(f"Wrote {out_dir / 'farokhi2023_proton_gvalues.png'}")
    print(f"Wrote {out_dir / 'farokhi2023_electron_gvalues.png'}")
    print(f"Wrote {out_dir / 'farokhi2023_proton_boxsize_gvalues.png'}")
    print(f"Wrote {out_dir / 'farokhi2023_proton_relerr.png'}")
    print(f"Wrote {out_dir / 'farokhi2023_electron_relerr.png'}")
    print(f"Wrote {out_dir / 'statistics_summary.md'}")


if __name__ == "__main__":
    main()
