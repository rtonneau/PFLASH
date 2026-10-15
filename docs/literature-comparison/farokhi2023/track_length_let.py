"""Path length and LET PROXIES for the primary track, from the existing
PreChemical_run<R>_event<E>.txt dumps -- no new C++ instrumentation, no
re-running the simulation.

Source and method: each H2O line in a PreChemical file is written directly by
Geant4-DNA's G4DNAChemistryManager::CreateWaterMolecule for every ionisation
(modification code 0), excitation (1) or dissociative-attachment (2) event:
ParentID, "H2O", "<modification>:<level>", Energy (eV), then the 3D position
(nm) of the interacting track at that moment. The Energy column is a DISCRETE
shell/level constant from G4DNAWaterIonisationStructure/ExcitationStructure
(confirmed against the proton-beam data: exactly the water ionisation shells
10.79, 13.39, 16.05, 32.30, and the rare 539.00 eV oxygen K-shell) -- NOT the
actual stochastic energy the primary lost in that step (which would also
include any secondary electron's kinetic energy).

Filtering to ParentID == 1 (the primary track; confirmed on the proton data:
Z position increases monotonically along the 90 MeV beam direction, i.e. an
essentially straight track) and taking the H2O rows in file order gives the
sequence of positions where the primary itself ionised/excited/attached.
Summing consecutive Euclidean distances between them gives a PATH-LENGTH
ESTIMATE; summing the Energy column over the same rows gives an ENERGY-PROXY
(energy channelled directly into water ionisation/excitation by the primary).
Dividing the two gives an LET-PROXY (keV/um).

IMPORTANT -- these are proxies, not the standard physical quantities:
  - Path length skips the geometric wiggle from steps between logged points
    that do not produce a chemistry-stage species (elastic scattering,
    vibrational excitation, sub-threshold attachment attempts). Negligible
    bias for the proton (path already close to straight); likely a real
    underestimate for the electron, whose steps are dominated by elastic
    scattering (e.g. ~160680 elastic vs. ~2489 ionisation events of all
    electrons combined in one proton-beam run -- see physical_stage_counts.md)
    -- so the proton/electron path-length ratio carries an asymmetric bias.
  - The energy-proxy excludes kinetic energy handed to secondary electrons at
    the same step, so it under-counts true dE/dx: a lower bound, not the NIST
    PSTAR/ESTAR stopping power.
  - A true instrumented LET (actual per-step kinetic-energy loss and step
    length of the primary, via new C++ scoring code and re-running the
    simulations) remains available as future work if this proxy needs
    validating further.

As with physical_stage_counts.py, all O2-level directories within a batch are
bit-identical (same fixed RNG seed, O2 is chemistry-stage-only) -- this
script uses one representative O2 directory per beam/box configuration, not
pooled (see physical-stage-deterministic-same-seed memory /
physical_stage_counts.py's docstring for the full explanation).

Usage: python track_length_let.py
Writes, next to this script: track_length_let_boxsize.png (10 vs 20 um
proton), track_length_let_proton_vs_electron.png (proton vs electron), and
track_length_let.md (full table + caveats).
"""

import math
import os
import statistics
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

G4_DATA_OUTPUT = Path(os.environ.get("G4_DATA_OUTPUT", r"D:\DATA\Geant4"))

RUNS = {
    "proton_10um": (G4_DATA_OUTPUT / "261009_Farokhi2023_proton" / "o2_0pct", "90 MeV proton, 10 um box"),
    "proton_20um": (G4_DATA_OUTPUT / "261010_Farokhi2023_proton_20um" / "o2_0pct", "90 MeV proton, 20 um box"),
    "electron": (G4_DATA_OUTPUT / "261010_Farokhi2023_electron_300ev" / "o2_0pct", "500 keV electron"),
}


def per_event_path_and_energy(path):
    """Parse one PreChemical_*.txt. Return (path_length_nm, energy_eV, n_interactions)
    for the primary track (ParentID == 1, H2O rows only), in file order."""
    positions = []
    energy_sum = 0.0
    with open(path, encoding="utf-8") as f:
        for i, line in enumerate(f):
            if i < 3 or not line.strip():
                continue
            parts = line.split()
            parent_id = parts[0]
            mol = parts[1]
            if parent_id != "1" or mol != "H2O":
                continue
            energy_sum += float(parts[3])
            positions.append((float(parts[4]), float(parts[5]), float(parts[6])))

    path_length = 0.0
    for (x0, y0, z0), (x1, y1, z1) in zip(positions, positions[1:]):
        path_length += math.sqrt((x1 - x0) ** 2 + (y1 - y0) ** 2 + (z1 - z0) ** 2)

    return path_length, energy_sum, len(positions)


def load_run(run_dir):
    """Return a list of (path_length_nm, energy_eV, n_interactions, let_keV_per_um), one per event."""
    files = sorted(run_dir.glob("PreChemical_run*_event*.txt"))
    rows = []
    for f in files:
        length_nm, energy_eV, n = per_event_path_and_energy(f)
        let_keV_per_um = (energy_eV * 1e-3) / (length_nm * 1e-3) if length_nm > 0 else float("nan")
        rows.append((length_nm, energy_eV, n, let_keV_per_um))
    return rows


def mean_sem(values):
    values = [v for v in values if not math.isnan(v)]
    n = len(values)
    if n == 0:
        return float("nan"), float("nan"), 0
    mean = statistics.fmean(values)
    if n > 1:
        var = statistics.variance(values, xbar=mean)
        sem = math.sqrt(var / n)
    else:
        sem = float("nan")
    return mean, sem, n


QUANTITIES = ["length_um", "energy_keV", "let_keV_per_um"]
QUANTITY_LABELS = {
    "length_um": "Path length proxy\n(um)",
    "energy_keV": "Energy proxy\n(keV)",
    "let_keV_per_um": "LET proxy\n(keV/um)",
}


def summarize(rows):
    length_um = [r[0] * 1e-3 for r in rows]
    energy_keV = [r[1] * 1e-3 for r in rows]
    let_keV_per_um = [r[3] for r in rows]
    return {
        "length_um": mean_sem(length_um),
        "energy_keV": mean_sem(energy_keV),
        "let_keV_per_um": mean_sem(let_keV_per_um),
    }


def plot_bar(out_path, stats_by_label, title):
    labels = list(stats_by_label.keys())
    n = len(QUANTITIES)
    fig, axes = plt.subplots(1, n, figsize=(4.5 * n, 4.5))
    colors = ["tab:orange", "tab:red", "tab:blue"]
    for ax, qty in zip(axes, QUANTITIES):
        means = [stats_by_label[l][qty][0] for l in labels]
        sems = [stats_by_label[l][qty][1] for l in labels]
        ax.bar(labels, means, yerr=sems, capsize=4, color=colors[: len(labels)])
        ax.set_ylabel(QUANTITY_LABELS[qty])
        ax.tick_params(axis="x", rotation=15)
    fig.suptitle(title)
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def fmt(mean, sem, n):
    if n == 0:
        return "n/a"
    if math.isnan(sem):
        return f"{mean:.4g} (n={n})"
    return f"{mean:.4g} +/- {sem:.4g} (n={n})"


def write_markdown(out_path, stats_by_label, labels_desc):
    lines = [
        "# Path length / LET proxies (from existing PreChemical dumps)",
        "",
        "These are PROXIES, not the standard physical quantities -- see the script docstring",
        "(`track_length_let.py`) for the full derivation and caveats. Summary:",
        "",
        "- **Path length proxy**: sum of Euclidean distances between consecutive primary-track",
        "  (ParentID == 1) H2O interaction points logged in `PreChemical_*.txt`. Skips the",
        "  geometric wiggle from steps that don't log a chemistry-stage species (elastic",
        "  scattering, vibrational excitation) -- negligible bias for the proton (near-straight",
        "  track, verified), a likely real underestimate for the electron (scattering-dominated).",
        "- **Energy proxy**: sum of the discrete ionisation/excitation shell energies",
        "  (`G4DNAWaterIonisationStructure`/`ExcitationStructure`) for those same interactions --",
        "  excludes kinetic energy handed to secondary electrons, so it under-counts true dE/dx.",
        "- **LET proxy** = energy proxy / path length proxy (keV/um). Not the NIST PSTAR/ESTAR",
        "  stopping power.",
        "",
        "All O2-level directories within a batch are bit-identical (same seed, O2 is",
        "chemistry-stage only -- see `physical_stage_counts.md`); one representative O2 directory",
        "per beam/box configuration is used here, not pooled.",
        "",
        "## Results",
        "",
        "| Run | Path length (um) | Energy (keV) | LET (keV/um) |",
        "|---|---|---|---|",
    ]
    for label, desc in labels_desc.items():
        s = stats_by_label[label]
        lines.append(
            f"| {desc} | {fmt(*s['length_um'])} | {fmt(*s['energy_keV'])} | {fmt(*s['let_keV_per_um'])} |")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    out_dir = Path(__file__).resolve().parent

    rows_by_key = {}
    labels_desc = {}
    for key, (run_dir, desc) in RUNS.items():
        rows_by_key[key] = load_run(run_dir)
        labels_desc[key] = desc

    stats_by_key = {key: summarize(rows) for key, rows in rows_by_key.items()}

    boxsize_stats = {
        labels_desc["proton_10um"]: stats_by_key["proton_10um"],
        labels_desc["proton_20um"]: stats_by_key["proton_20um"],
    }
    plot_bar(out_dir / "track_length_let_boxsize.png", boxsize_stats,
             "Path length / energy / LET proxies -- proton box-size check\n"
             "(LET proxy should be roughly box-size-independent)")

    pe_stats = {
        labels_desc["proton_10um"]: stats_by_key["proton_10um"],
        labels_desc["electron"]: stats_by_key["electron"],
    }
    plot_bar(out_dir / "track_length_let_proton_vs_electron.png", pe_stats,
             "Path length / energy / LET proxies -- proton vs. electron")

    write_markdown(out_dir / "track_length_let.md", stats_by_key, labels_desc)

    print(f"Wrote {out_dir / 'track_length_let_boxsize.png'}")
    print(f"Wrote {out_dir / 'track_length_let_proton_vs_electron.png'}")
    print(f"Wrote {out_dir / 'track_length_let.md'}")


if __name__ == "__main__":
    main()
