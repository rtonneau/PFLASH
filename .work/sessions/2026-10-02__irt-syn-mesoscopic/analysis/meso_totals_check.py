"""Recorded vs true molecule totals at the end of the mesoscopic stage.

Usage: python meso_totals_check.py <dump_dir> <log_file>

The log must come from a run at /dnaLogger/verbose Debug: each event prints
"[TimeStepAction] Chemistry ends ... final total = N" (the true mesh total).
Sums those over events and compares with the SpeciesMeso.csv total at the
last record times. Equal at the end time means the late records are live,
not stale (the check used to verify baf4b29: 25796 = 25796 on 30 O2 events).
"""
import collections
import csv
import re
import sys
from pathlib import Path

dump, log = Path(sys.argv[1]), Path(sys.argv[2])

finals = [int(m) for m in re.findall(r"final total = (\d+)", log.read_text(encoding="utf-8", errors="replace"))]
print(f"true end totals: {len(finals)} events, sum {sum(finals)}")

totals = collections.defaultdict(int)
with open(dump / "SpeciesMeso.csv", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        totals[float(row["time_ns"])] += int(row["count"])
for t in sorted(totals)[-5:]:
    print(f"SpeciesMeso total at {t:10.3f} ns: {totals[t]}")
