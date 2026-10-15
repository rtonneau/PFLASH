"""Validate SpeciesMesoSpatial.h5 of a dump directory against SpeciesMeso.csv.

usage: python check_meso_spatial.py <dump dir> [--mt]

Checks (all runs/events/snapshots):
  structure: root attr 'species', per snapshot attrs time_ns/cellSize_nm,
             datasets counts (N x S) and position_nm (N x 3)
  cellSize_nm never decreases with time within an event
  every position (cell centre) lies inside the box (halfBox_um of Manifest.json,
  default 500 um, centred at the origin)
  1-event dump: counts summed over cells == SpeciesMeso.csv value at the same
  time (rel. tol 1e-9). Species present in only one of the two are reported.
  --mt (or more than one event in the dump): the csv is a mean over events, so
  only structure, monotonic cell size and box containment are checked.
Prints OK, or the mismatches (exit code 1).
"""
import csv, json, os, re, sys
import numpy as np
import h5py

TIME_RTOL = 1e-4   # csv times are printed with limited precision
COUNT_RTOL = 1e-9


def dec(x):
    return x.decode("utf-8") if isinstance(x, bytes) else str(x)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    force_mt = "--mt" in sys.argv
    if len(args) != 1:
        print(__doc__)
        return 2
    d = args[0]
    errs = []
    half_nm = 500.0 * 1000.0
    mpath = os.path.join(d, "Manifest.json")
    if os.path.exists(mpath):
        with open(mpath, encoding="utf-8") as f:
            half_nm = float(json.load(f).get("halfBox_um", 500)) * 1000.0
    else:
        errs.append("Manifest.json missing (assuming halfBox 500 um)")

    with h5py.File(os.path.join(d, "SpeciesMesoSpatial.h5"), "r") as h:
        species = [dec(s) for s in h.attrs["species"]]
        runs = [k for k in h.keys() if re.fullmatch(r"run\d+", k)]
        events = [(r, e) for r in runs for e in h[r].keys()]
        if not events:
            errs.append("no run/event groups")
        compare = len(events) == 1 and not force_mt
        csvt = {}
        if compare:
            with open(os.path.join(d, "SpeciesMeso.csv"), encoding="utf-8", newline="") as f:
                for row in csv.DictReader(f):
                    csvt.setdefault(float(row["time_ns"]), {})[row["species"]] = float(row["count"])
            ctimes = np.array(sorted(csvt))
            csv_species = {s for v in csvt.values() for s in v}
            for s in sorted(csv_species - set(species)):
                errs.append(f"species in csv but not in h5: {s}")
            # the csv only lists species that are non-zero at some time, so an
            # h5-only species is a mismatch only if it has counts (checked below)
        nsnap = 0
        total = np.zeros(len(species))
        for r, e in events:
            g = h[r][e]
            snaps = sorted(g.keys(), key=lambda k: int(k.replace("snapshot", "")))
            if not snaps:
                errs.append(f"{r}/{e}: no snapshots")
            last_t, last_c = -np.inf, 0.0
            for k in snaps:
                nsnap += 1
                s = g[k]
                p = f"{r}/{e}/{k}"
                t, c = float(s.attrs["time_ns"]), float(s.attrs["cellSize_nm"])
                counts, pos = s["counts"][...], s["position_nm"][...]
                if counts.ndim != 2 or counts.shape[1] != len(species):
                    errs.append(f"{p}: counts shape {counts.shape} vs {len(species)} species")
                    continue
                if pos.shape != (counts.shape[0], 3):
                    errs.append(f"{p}: position_nm shape {pos.shape} vs counts {counts.shape}")
                    continue
                if t < last_t:
                    errs.append(f"{p}: time_ns decreases ({t} < {last_t})")
                if c < last_c * (1 - 1e-12):
                    errs.append(f"{p}: cellSize_nm decreases ({c} < {last_c})")
                last_t, last_c = t, c
                if pos.size and np.abs(pos).max() > half_nm:
                    errs.append(f"{p}: position outside box (max |x| = {np.abs(pos).max():.6g} nm > {half_nm:g})")
                total += counts.sum(axis=0, dtype=np.float64)
                if compare:
                    i = int(np.argmin(np.abs(ctimes - t)))
                    if abs(ctimes[i] - t) > TIME_RTOL * abs(t):
                        errs.append(f"{p}: no csv time near {t} ns (nearest {ctimes[i]})")
                        continue
                    row = csvt[ctimes[i]]
                    sums = counts.sum(axis=0, dtype=np.int64)
                    for j, name in enumerate(species):
                        want = row.get(name, 0.0)
                        got = float(sums[j])
                        if abs(got - want) > COUNT_RTOL * max(abs(want), 1e-300) and got != want:
                            errs.append(f"{p} t={t:g} {name}: h5 sum {got:g} != csv {want:g}")
        if compare:
            zero_only = []
            for j, name in enumerate(species):
                if name not in csv_species:
                    if total[j] > 0:
                        errs.append(f"species in h5 (total count {total[j]:g}) but not in csv: {name}")
                    else:
                        zero_only.append(name)
            if zero_only:
                print("note: all-zero in h5 and absent from csv (consistent): " + ", ".join(zero_only))
        mode = "1-event full comparison" if compare else "structure + monotonic cell size + box only"
        print(f"{len(events)} event(s), {nsnap} snapshot(s), {len(species)} species; {mode}")
    if errs:
        print("MISMATCHES:")
        for x in errs[:200]:
            print("  " + x)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
