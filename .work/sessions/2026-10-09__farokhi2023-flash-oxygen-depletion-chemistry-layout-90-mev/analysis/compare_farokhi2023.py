"""Compare dnachem-min's Farokhi2023 G-value curves with Farokhi et al. 2023.

Usage: python compare_farokhi2023.py   (no arguments; run directories are a
small hardcoded list below, following the precedent of
../../2026-10-06__homogeneous-chemistry-prechemical-extension/analysis/compare_gvalues.py)

Reads the already-produced `Species_nt_species.csv` ntuple (one row per
species/time/run, columns speciesID, number, nEvent, speciesName, time_ns,
sumG, sumG2 -- the same convention `compare_gvalues.py` already parses) from
tickets 5 (90 MeV proton, Farokhi2023 chemistry, 10x10x10 um^3 box, 0/3/7/21 %
O2, 20 events each) and 6 (500 keV electron, Farokhi2023 chemistry, default
500 um box, PrimaryKiller eLossMin 5 keV, 0/2.5/5/21 % O2, 50 events each) of
this session. `sumG` is `ScoreSpecies`'s per-event G-value
(`n_mol / (eventEdep_eV) * 100`, src/scoring/ScoreSpecies.cc) summed over
events, so `sumG / nEvent` is the mean G-value per event at that time --
exactly what this script plots, without needing Manifest.json's
totalEnergyDeposit_eV separately (that would give a ratio-of-sums G, which
differs from the mean-of-per-event-G the paper and `ScoreSpecies` itself use
whenever per-event energy deposit varies, as it does for stochastic proton
tracks).

Produces two PNGs next to this script:
  - farokhi2023_proton_gvalues.png: one panel per species the paper tracks in
    Fig. 2 (OH, H3O+, H, e_aq, H2, H2O2, HO2, O2-), dnachem-min's G(t) curves
    for all four irradiated O2 levels (0/3/7/21 %), with the 21 % curve also
    overlaid with hand-digitized reference points read off Fig. 2's 21 % O2
    panel (the same case used for the Fig. 3 TRAX-CHEM comparison).
  - farokhi2023_electron_gvalues.png: one panel per species the paper tracks
    in Fig. 5 (e_aq, O2-, H, HO2), dnachem-min's G(t) curves for all four
    irradiated O2 levels (0/2.5/5/21 %), each overlaid with hand-digitized
    reference points read off the matching O2-level curve of Fig. 5's four
    panels.

Paper: Farokhi et al., Radiat. Phys. Chem. 212 (2023) 111184,
docs/literature/Farokhi2023_Effects_of_the_Oxygen_depletion_in_FLASH_irradiation_investigated_through_geant4_DNA_toolkit.pdf.
The REFERENCE_* data literals below were read by eye off Fig. 2 (21 % O2
panel) and Fig. 5 (all 4 panels, solid "Geant4-DNA" curves -- the paper's own
Geant4-DNA simulation, not the dashed TRAX-CHEM curves plotted alongside it)
at a handful of time points per curve; they are approximate, hand-digitized
values, not extracted with a digitizing tool, and are only meant to show
whether dnachem-min's curves land in the right place and have the right
shape.

Caveats -- this comparison is indicative only, not a reproduction:
  - dnachem-min models a single homogeneous water box (10x10x10 um^3 for the
    proton runs, 500 um default for the electron runs), not the paper's
    nested sensitive-volume geometry.
  - dnachem-min's PhysicsList uses G4EmDNAPhysics_option2; the paper does not
    state which G4EmDNAPhysics option (if any beyond "Geant4-DNA") it used,
    so the physics settings are not confirmed to match exactly.
  - The reference points are hand-digitized off rendered PDF pages (read
    visually, not with a digitizing tool), so they carry a visible reading
    error (of order 0.1-0.2 G-value units) on top of whatever the paper's own
    figures already carry.
"""

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ---------------------------------------------------------------------------
# dnachem-min run directories (already produced by tickets 5 and 6; this
# script only reads them, it never re-runs the macros).

REPO_ROOT = Path(__file__).resolve().parents[4]
SESSION_SCRATCH = (
    REPO_ROOT
    / ".scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev"
)

# O2 level (%) -> run directory name, in the fixed plot order (0 %, low %,
# mid %, 21 %) shared with the paper's own 4-colour legend (blue/green/orange/
# magenta for 0/low/mid/21 %).
PROTON_RUNS = {
    0: "ticket05-proton-0pct-smallbox",
    3: "ticket05-proton-3pct-smallbox",
    7: "ticket05-proton-7pct-smallbox",
    21: "ticket05-proton-21pct-smallbox",
}
ELECTRON_RUNS = {
    0: "ticket06-electron-0pct-fix",
    2.5: "ticket06-electron-2p5pct-fix",
    5: "ticket06-electron-5pct-fix",
    21: "ticket06-electron-21pct-fix",
}
O2_COLORS = {0: "tab:blue", 3: "tab:green", 2.5: "tab:green", 7: "tab:orange", 5: "tab:orange", 21: "magenta"}

SPECIES_CSV_NAME = "Species_nt_species.csv"

# Display label -> exact speciesName string used in Species_nt_species.csv /
# Species.Txt (project molecule-table names; "\xb0" is the degree sign used
# for radicals).
SPECIES_CSV_LABEL = {
    "e_aq": "e_aq^-1",
    "OH": "°OH^0",
    "H": "H^0",
    "H3Op": "H3O^1",
    "H2": "H_2^0",
    "H2O2": "H2O2^0",
    "O2m": "O_2^-1",
    "HO2": "HO_2°^0",
}


def load_gvalues(run_dir):
    """Return {csv speciesName: [(time_s, mean G per event), ...]} sorted by time."""
    path = run_dir / SPECIES_CSV_NAME
    series = {}
    with open(path, newline="", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            row = next(csv.reader([line]))
            name = row[3]
            time_ns = float(row[4])
            n_event = int(row[2])
            sum_g = float(row[5])
            series.setdefault(name, []).append((time_ns * 1e-9, sum_g / n_event))
    for pts in series.values():
        pts.sort()
    return series


# ---------------------------------------------------------------------------
# Hand-digitized reference points (molecules/100 eV vs. time in seconds).
# Read by eye off rendered pages of the Farokhi2023 PDF -- approximate, not
# extracted with a digitizing tool. See module docstring for the figures used.

# Fig. 2, PO2 = 21 % panel (90 MeV protons).
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

# Fig. 5, solid "Geant4-DNA" curves (0.5 MeV electrons), one entry per
# species panel, keyed by O2 level (%) matching ELECTRON_RUNS.
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


def plot_proton(out_path):
    species_list = ["OH", "H3Op", "H", "e_aq", "H2", "H2O2", "HO2", "O2m"]
    runs = {o2: load_gvalues(SESSION_SCRATCH / name) for o2, name in PROTON_RUNS.items()}

    fig, axes = plt.subplots(2, 4, figsize=(16, 7), sharex=True)
    for ax, label in zip(axes.flat, species_list):
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in PROTON_RUNS:
            pts = runs[o2].get(csv_name, [])
            if not pts:
                continue
            t, g = zip(*pts)
            ax.plot(t, g, color=O2_COLORS[o2], label=f"dnachem-min {o2}% O2")
        ref = REFERENCE_PROTON_21PCT.get(label)
        if ref:
            t, g = zip(*ref)
            ax.scatter(t, g, color="black", marker="x", zorder=5,
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
    fig.suptitle("dnachem-min vs. Farokhi et al. 2023 Fig. 2/3 -- 90 MeV proton, Farokhi2023 chemistry")
    fig.tight_layout(rect=(0, 0.05, 1, 0.96))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_electron(out_path):
    species_list = ["e_aq", "O2m", "H", "HO2"]
    runs = {o2: load_gvalues(SESSION_SCRATCH / name) for o2, name in ELECTRON_RUNS.items()}

    fig, axes = plt.subplots(1, 4, figsize=(18, 4.5))
    for ax, label in zip(axes, species_list):
        csv_name = SPECIES_CSV_LABEL[label]
        for o2 in ELECTRON_RUNS:
            pts = runs[o2].get(csv_name, [])
            if not pts:
                continue
            t, g = zip(*pts)
            ax.plot(t, g, color=O2_COLORS[o2], label=f"dnachem-min {o2}% O2")
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
    # de-duplicate (each panel repeats the "dnachem-min 0% O2" style labels)
    seen = {}
    for h, l in zip(handles, labels):
        seen.setdefault(l, h)
    fig.legend(seen.values(), seen.keys(), loc="lower center", ncol=4, bbox_to_anchor=(0.5, -0.12), fontsize=8)
    fig.suptitle("dnachem-min vs. Farokhi et al. 2023 Fig. 5 -- 500 keV electron, Farokhi2023 chemistry")
    fig.tight_layout(rect=(0, 0.18, 1, 0.92))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def main():
    out_dir = Path(__file__).resolve().parent
    proton_png = out_dir / "farokhi2023_proton_gvalues.png"
    electron_png = out_dir / "farokhi2023_electron_gvalues.png"
    plot_proton(proton_png)
    plot_electron(electron_png)
    print(f"Wrote {proton_png}")
    print(f"Wrote {electron_png}")


if __name__ == "__main__":
    main()
