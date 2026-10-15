"""Physical/pre-chemical stage interaction counts for the Farokhi2023 proton
and electron production runs: water ionisation, water excitation,
dissociative electron attachment, and secondary-electron (solvation) counts.

Source: PreChemical_run<R>_event<E>.txt, the per-event pre-chemical-stage
dump that Geant4-DNA's G4DNAChemistryManager/G4PhysChemIO writes directly
from the physics models (G4DNABornIonisationModel, G4DNAMillerGreenExcitationModel,
G4DNAMeltonAttachmentModel, G4DNAOneStepThermalizationModel, ...), staged
per-event by EventAction and moved into the dump directory by
RunAccumulatorMessenger at /run/dumpDataAndReset(ToDir) -- i.e. exactly the
files "dumped at the end of the physical phase / start of the pre-chemical
phase" that get moved at dump time.

File format (one line per physical interaction that creates a chemistry-stage
species): "<ParentID> H2O <modif>:<level> <energy_eV> ... <pos>" for an
ionised/excited/dissociatively-attached water molecule, or
"<ParentID> e_aq -1 <energy_eV> ... <pos>" for a secondary electron that
reached thermalisation/solvation. <modif> is Geant4's own
ElectronicModification enum (source/processes/electromagnetic/dna/utils/include/
G4DNAChemistryManager.hh): 0 = eIonizedMolecule, 1 = eExcitedMolecule,
2 = eDissociativeAttachment. Each e_aq line is one secondary electron reaching
the chemistry stage (a direct proxy for "secondary electron production": every
secondary that does not itself re-ionise/re-excite further ends up as exactly
one such line once it thermalises).

These are physics-stage quantities: independent of the chemistry model and
the O2 scavenger concentration (O2 only acts as a chemical-stage bulk
reaction, added after this file is written). IMPORTANT: every production
macro in this family uses the same fixed RNG seed (sim.cc's kDefaultSeed =
12345) regardless of O2 level, and per-event seeding in Geant4's MT run
manager is assigned deterministically from that single master seed before
any O2-dependent (chemistry-stage) RNG draw happens -- so the PreChemical
files, and every physics-stage aggregate derived from them, are *bit-
identical* across the 4 O2 levels of a given batch (verified by diff: proton
PreChemical_run0_event*.txt and PhysicsInteractions.Txt match byte-for-byte
across o2_{0,3,7,21}pct; same for electron). Pooling across O2 levels would
therefore count the same 16 (or 56/300) physical events repeatedly and
understate the true SEM -- this script uses exactly ONE representative O2
directory per beam/box/eLossMin configuration and reports the per-O2-level
table purely as a determinism check (every row must match the representative
row). For the same reason, the 56-event and 300-event electron batches are
NOT pooled either: they share the same seed, so the 56-event batch's events
are a byte-identical prefix of the 300-event batch's (verified) -- the
300-event batch alone is the larger, non-redundant sample. Runs with
different box size or PrimaryKiller eLossMin would not be poolable anyway,
since both directly change which/how many physical interactions are
reachable (box size bounds secondary travel in the one-box geometry;
eLossMin truncates the *primary* track once it has deposited the threshold,
stopping its own further ionisations/excitations early without affecting
already-created secondaries).

Usage: python physical_stage_counts.py
Writes, next to this script: physical_stage_counts.png (grouped bar chart,
proton vs. electron, mean count per event +-1 SEM) and
physical_stage_counts.md (the full table, pooled and per-O2-level).
"""

import math
import os
import statistics
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

G4_DATA_OUTPUT = Path(os.environ.get("G4_DATA_OUTPUT", r"D:\DATA\Geant4"))

PROTON_BASE = G4_DATA_OUTPUT / "261009_Farokhi2023_proton"
PROTON_RUNS = {0: "o2_0pct", 3: "o2_3pct", 7: "o2_7pct", 21: "o2_21pct"}
PROTON_REPRESENTATIVE_O2 = 0  # all 4 are bit-identical (same seed, O2 is chemistry-only)

# 300-event batch only: same fixed seed as the 56-event batch, so the 56-event
# batch's events are a byte-identical prefix of this one (verified) -- not a
# second independent sample.
ELECTRON_BASE = G4_DATA_OUTPUT / "261010_Farokhi2023_electron_300ev"
ELECTRON_RUNS = {0: "o2_0pct", 2.5: "o2_2p5pct", 5: "o2_5pct", 21: "o2_21pct"}
ELECTRON_REPRESENTATIVE_O2 = 0  # all 4 are bit-identical for the same reason


def count_prechem_file(path):
    """Return (n_ionisation, n_excitation, n_attachment, n_secondary_e) for one event file."""
    n_ion = n_exc = n_att = n_eaq = 0
    with open(path, encoding="utf-8") as f:
        for i, line in enumerate(f):
            if i < 3 or not line.strip():
                continue
            parts = line.split()
            mol = parts[1]
            if mol == "H2O":
                modif = parts[2].split(":")[0]
                if modif == "0":
                    n_ion += 1
                elif modif == "1":
                    n_exc += 1
                elif modif == "2":
                    n_att += 1
            elif mol == "e_aq":
                n_eaq += 1
    return n_ion, n_exc, n_att, n_eaq


def per_event_counts(run_dir):
    """Return a list of (n_ion, n_exc, n_att, n_eaq) tuples, one per event file."""
    files = sorted(run_dir.glob("PreChemical_run*_event*.txt"))
    return [count_prechem_file(f) for f in files]


def mean_sem(values):
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


COLUMNS = ["ionisation", "excitation", "attachment", "secondary_e"]


def summarize(rows_by_label):
    """rows_by_label: {label: [(ion, exc, att, eaq), ...]}. Returns {label: {col: (mean, sem, n)}}."""
    out = {}
    for label, rows in rows_by_label.items():
        stats = {}
        for idx, col in enumerate(COLUMNS):
            stats[col] = mean_sem([r[idx] for r in rows])
        out[label] = stats
    return out


def load_proton():
    per_o2 = {o2: per_event_counts(PROTON_BASE / name) for o2, name in PROTON_RUNS.items()}
    representative = per_o2[PROTON_REPRESENTATIVE_O2]
    return per_o2, representative


def load_electron():
    per_o2 = {o2: per_event_counts(ELECTRON_BASE / name) for o2, name in ELECTRON_RUNS.items()}
    representative = per_o2[ELECTRON_REPRESENTATIVE_O2]
    return per_o2, representative


def plot_bar(out_path, proton_stats, electron_stats):
    labels = ["ionisation", "excitation", "attachment", "secondary_e"]
    display = ["Water\nionisation", "Water\nexcitation", "Dissociative\nattachment",
               "Secondary e-\n(solvated)"]
    x = range(len(labels))
    width = 0.35
    fig, ax = plt.subplots(figsize=(8, 5))
    p_mean = [proton_stats[l][0] for l in labels]
    p_sem = [proton_stats[l][1] for l in labels]
    e_mean = [electron_stats[l][0] for l in labels]
    e_sem = [electron_stats[l][1] for l in labels]
    ax.bar([i - width / 2 for i in x], p_mean, width, yerr=p_sem, capsize=4,
           label="90 MeV proton (16 ev)", color="tab:orange")
    ax.bar([i + width / 2 for i in x], e_mean, width, yerr=e_sem, capsize=4,
           label="500 keV electron (300 ev)", color="tab:blue")
    ax.set_xticks(list(x))
    ax.set_xticklabels(display)
    ax.set_ylabel("Mean count per event")
    ax.set_yscale("log")
    ax.legend()
    ax.set_title("Physical/pre-chemical stage interaction counts per event\n"
                 "dnachem-min, Farokhi2023 chemistry, error bars = +-1 SEM")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def fmt(mean, sem, n):
    if n == 0:
        return "n/a"
    if math.isnan(sem):
        return f"{mean:.3f} (n={n})"
    return f"{mean:.3f} +/- {sem:.3f} (n={n})"


def write_markdown(out_path, proton_per_o2, proton_stats, electron_per_o2, electron_stats):
    lines = [
        "# Physical/pre-chemical stage interaction counts",
        "",
        "Mean count per event (molecules/events), +-1 SEM across events, from",
        "`PreChemical_run<R>_event<E>.txt`. Ionisation/excitation/attachment are H2O",
        "lines by modification code (0/1/2); secondary_e is the count of e_aq lines",
        "(solvated secondary electrons). See the script docstring for exact definitions.",
        "",
        "All 4 O2-level macros in a batch share the fixed RNG seed (12345), and O2 only",
        "acts in the chemistry stage (after these files are written), so these files are",
        "bit-identical across O2 levels -- confirmed below. One representative O2",
        "directory per beam is used for the counts (pooling across O2 would count the",
        "same events repeatedly and understate SEM); the electron run uses the 300-event",
        "batch, whose events are a seed-identical superset of the earlier 56-event batch.",
        "",
        "## Representative counts",
        "",
        "| Quantity | 90 MeV proton (10x10x10 um^3, 16 ev) | 500 keV electron "
        "(500 um box, eLossMin 5 keV, 300 ev) | Electron / proton ratio |",
        "|---|---|---|---|",
    ]
    for col, label in zip(COLUMNS, ["Water ionisation", "Water excitation",
                                     "Dissociative attachment", "Secondary e- (solvated)"]):
        pm, ps, pn = proton_stats[col]
        em, es, en = electron_stats[col]
        ratio = f"{em / pm:.2f}" if pm else "n/a"
        lines.append(f"| {label} | {fmt(pm, ps, pn)} | {fmt(em, es, en)} | {ratio} |")

    pi = proton_stats["ionisation"][0]
    pe = proton_stats["excitation"][0]
    ei = electron_stats["ionisation"][0]
    ee = electron_stats["excitation"][0]
    lines += [
        "",
        f"Ionisation/excitation ratio: proton {pi / pe:.3f}, electron {ei / ee:.3f}.",
        "",
        "## Per O2 level (determinism check -- every row below must equal the",
        "representative row above exactly, not just within SEM: physical stage does not",
        "see O2, and all O2 levels share the same seed)",
        "",
    ]
    for particle, per_o2 in [("Proton", proton_per_o2), ("Electron", electron_per_o2)]:
        lines.append(f"### {particle}")
        lines.append("")
        lines.append("| O2 level | Ionisation | Excitation | Attachment | Secondary e- |")
        lines.append("|---|---|---|---|---|")
        for o2 in sorted(per_o2):
            rows = per_o2[o2]
            stats = {col: mean_sem([r[idx] for r in rows]) for idx, col in enumerate(COLUMNS)}
            lines.append(
                f"| {o2}% | {fmt(*stats['ionisation'])} | {fmt(*stats['excitation'])} | "
                f"{fmt(*stats['attachment'])} | {fmt(*stats['secondary_e'])} |")
        lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    out_dir = Path(__file__).resolve().parent

    proton_per_o2, proton_rep = load_proton()
    electron_per_o2, electron_rep = load_electron()

    proton_stats = summarize({"rep": proton_rep})["rep"]
    electron_stats = summarize({"rep": electron_rep})["rep"]

    plot_bar(out_dir / "physical_stage_counts.png", proton_stats, electron_stats)
    write_markdown(out_dir / "physical_stage_counts.md",
                    proton_per_o2, proton_stats, electron_per_o2, electron_stats)

    print(f"Wrote {out_dir / 'physical_stage_counts.png'}")
    print(f"Wrote {out_dir / 'physical_stage_counts.md'}")


if __name__ == "__main__":
    main()
