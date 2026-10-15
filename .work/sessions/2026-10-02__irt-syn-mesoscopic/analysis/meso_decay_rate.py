"""Effective first-order decay rate of one species in SpeciesMeso.csv.

Usage: python meso_decay_rate.py <dump_dir> [species] [k_M^-1s^-1 conc_M]

Prints the species count at every mesoscopic record time and the effective
rate ln(c1/c2)/(t2-t1) between consecutive points. With bulk O2, e_aq should
decay at k[O2] = 1.74e10 * 2.73e-4 = 4.75e6 /s (defaults). This is the check
that exposed the stale late SpeciesMeso records fixed in baf4b29.
"""
import csv
import math
import sys
from pathlib import Path

dump = Path(sys.argv[1])
species = sys.argv[2] if len(sys.argv) > 2 else "e_aq^-1"
k, conc = (float(sys.argv[3]), float(sys.argv[4])) if len(sys.argv) > 4 else (1.74e10, 2.73e-4)

counts = {}
with open(dump / "SpeciesMeso.csv", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        if row["species"] == species:
            counts[float(row["time_ns"])] = counts.get(float(row["time_ns"]), 0) + int(row["count"])

points = sorted(counts.items())
print(f"t_ns       {species}")
for t, c in points:
    print(f"{t:10.3f} {c:9d}")
print("effective first-order rate (1/s) between consecutive points:")
for (t1, c1), (t2, c2) in zip(points, points[1:]):
    if c1 > 0 and c2 > 0 and t2 > t1:
        print(f"{t1:10.2f} -> {t2:10.2f} ns: {math.log(c1 / c2) / ((t2 - t1) * 1e-9):.3e}")
print(f"expected k*[S] = {k * conc:.3e}")
