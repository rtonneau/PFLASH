"""Compare dnachem-min species yields at ~100 ns with Table 3 of Tonneau et al.

Usage: python -I compare_gvalues.py <output-dir>

<output-dir> holds Species_nt_species.csv from a run of macro/beam_tonneau2025.in
(Tonneau2025 Chemistry, hand-over time 100 ns, so the particle-based stage
records the species at 100 ns). Mean G per event = sumG / nEvent, in
molecules/100 eV.

Paper values: Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, Table 3,
"G values at 100 ns (GCDR) for LET of 1.17 keV/um" [molecules/100 eV]. They are
the Boscolo et al. 2020 values; H3O+ is set equal to e_aq (charge conservation).
The comparison is indicative only: the simulation uses 10 keV electrons, whose
LET is higher than 1.17 keV/um.
"""
import csv
import sys
from pathlib import Path

# (label, species name in the CSV, paper G-value)
SPECIES = [
    ("e_aq", "e_aq^-1", 2.49),
    ("H3Op", "H3O^1", 2.49),
    ("OH", "°OH^0", 2.62),
    ("H", "H^0", 0.59),
    ("H2", "H_2^0", 0.31),
    ("H2O2", "H2O2^0", 0.54),
]
TARGET_NS = 100.0


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1]) / "Species_nt_species.csv"
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            c = next(csv.reader([line]))
            rows.append((c[3], float(c[4]), float(c[5]), int(c[2])))
    times = sorted({r[1] for r in rows})
    t = min(times, key=lambda x: abs(x - TARGET_NS))
    print(f"Species yields at {t:g} ns (target {TARGET_NS:g} ns), {path}")
    print(f"{'species':<6} {'G_sim':>8} {'G_paper':>8} {'ratio':>7}")
    for label, name, gpaper in SPECIES:
        hit = [r for r in rows if r[0] == name and r[1] == t]
        if not hit:
            print(f"{label:<6} missing in CSV ({name})")
            continue
        g = hit[0][2] / hit[0][3]
        flag = "  DEVIATES" if not 0.5 <= g / gpaper <= 2.0 else ""
        print(f"{label:<6} {g:8.3f} {gpaper:8.2f} {g / gpaper:7.2f}{flag}")
    print("Indicative only: 10 keV electron vs the paper's 1.17 keV/um LET column.")


if __name__ == "__main__":
    main()
