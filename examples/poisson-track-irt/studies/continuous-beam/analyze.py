#!/usr/bin/env python
"""Ratio-estimator G-values of the dose-rate study.

Usage:
    python analyze.py --plan <plan dir>/plan.json [--out <dir>]

Writes gvalues_time.csv and gvalues_1ms.csv (columns in README.md).
"""
import argparse
import csv
import json
import math
import os
import sys

import numpy as np

SPECIES = ["e_aq^-1", "°OH^0", "H^0", "H_2^0", "H2O2^0", "H3O^1", "OH^-1"]
TIME_TOLERANCE = 1e-9   # relative, to match computed grid times against species.csv


def ratio_estimator(y, x):
    """G = sum(y)/sum(x) * 100 and its delta-method SE.

    y, x: arrays of shape (n_realizations, n_times); y = molecule count, x = deposited energy [eV].
    Returns (G, SE) of shape (n_times,), in molecules per 100 eV. SE is NaN for n < 2,
    G and SE are NaN where sum(x) = 0.
    """
    y = np.asarray(y, dtype=float)
    x = np.asarray(x, dtype=float)
    n = y.shape[0]
    sum_x = x.sum(axis=0)
    with np.errstate(divide="ignore", invalid="ignore"):
        ratio = np.where(sum_x > 0, y.sum(axis=0) / sum_x, np.nan)
        if n < 2:
            se = np.full(ratio.shape, np.nan)
        else:
            resid = y - ratio * x
            var = (resid ** 2).sum(axis=0) / (n * (n - 1))
            se = np.sqrt(var) / (sum_x / n)
        se = np.where(sum_x > 0, se, np.nan)
    return ratio * 100.0, se * 100.0


def anchor0_grid(min_age_ns, points_per_decade, endpoint_ns):
    """The times min_age * 10^(k/ppd) below the endpoint, plus the endpoint (anchor 0 of the record grid)."""
    times = []
    k = 0
    while True:
        t = min_age_ns * 10.0 ** (k / points_per_decade)
        if not t < endpoint_ns:
            break
        times.append(t)
        k += 1
    times.append(float(endpoint_ns))
    return np.array(times)


def read_species(path, species):
    """Return (sorted times array, {species: counts array}) from a species.csv (UTF-8)."""
    table = {}
    with open(path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if row["species"] in species:
                table.setdefault(float(row["timeNs"]), {})[row["species"]] = int(row["count"])
    times = np.array(sorted(table))
    counts = {s: np.array([table[t].get(s, 0) for t in times], dtype=float) for s in species}
    return times, counts


def read_activations(path):
    """Return (expected activation times, energy deposits [eV]) from activations.csv."""
    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    return (np.array([float(r["expectedActivationTimeNs"]) for r in rows]),
            np.array([float(r["energyDepositEv"]) for r in rows]))


def deposited_energy(times, act_times, act_energy):
    """E(t): energy of the sources with expected activation time <= t, for each t."""
    return np.array([act_energy[act_times <= t].sum() for t in times])


def match_times(wanted, available):
    """Index in `available` of each wanted time (or -1 when absent)."""
    index = np.full(len(wanted), -1, dtype=int)
    for i, t in enumerate(wanted):
        j = int(np.searchsorted(available, t))
        for k in (j - 1, j):
            if 0 <= k < len(available) and math.isclose(available[k], t, rel_tol=TIME_TOLERANCE):
                index[i] = k
    return index


def load_realization(output_dir, grid, species):
    """Counts (species -> array over grid, NaN where the grid time is absent) and E(t) for one run."""
    times, counts = read_species(os.path.join(output_dir, "species.csv"), species)
    idx = match_times(grid, times)
    present = idx >= 0
    n = {s: np.where(present, counts[s][np.where(present, idx, 0)], np.nan) for s in species}
    act_times, act_energy = read_activations(os.path.join(output_dir, "activations.csv"))
    e = deposited_energy(grid, act_times, act_energy)
    return present, n, e, len(act_times), act_energy.sum()


def check_ledger(output_dir):
    with open(os.path.join(output_dir, "ledger.json"), encoding="utf-8") as handle:
        return bool(json.load(handle)["checks"]["allPassed"])


def analyze(plan, plan_dir, species=SPECIES, allow_partial=False):
    """Return (time rows, 1 ms rows) as lists of dicts."""
    grid = anchor0_grid(plan["gridMinAgeNs"], plan["gridPointsPerDecade"], plan["endpointNs"])
    runs_by_rate = {}
    for run in plan["runs"]:
        runs_by_rate.setdefault(float(run["rate_Gy_s"]), []).append(run)

    per_rate = {}
    for rate in sorted(float(r) for r in plan["rates"]):
        loaded = []
        for run in runs_by_rate.get(rate, []):
            out = os.path.normpath(os.path.join(plan_dir, run["output"]))
            if not os.path.isfile(os.path.join(out, "ledger.json")):
                if allow_partial:
                    continue
                raise FileNotFoundError("missing output of run %s: %s" % (run["id"], out))
            if not check_ledger(out):
                raise RuntimeError("run %s did not pass its checks (ledger allPassed false)" % run["id"])
            loaded.append(load_realization(out, grid, species))
        per_rate[rate] = loaded

    time_rows = []
    one_ms = {}
    for rate, loaded in per_rate.items():
        n_total = int(plan["realizations"]["%g" % rate])
        n_empty = int(plan["emptyRealizations"]["%g" % rate])
        n = len(loaded)
        if n:
            common = np.logical_and.reduce([item[0] for item in loaded])
            e = np.array([item[2] for item in loaded])
            tracks = float(np.mean([item[3] for item in loaded] + [0] * n_empty)) if n_total else float("nan")
            doses = [item[4] for item in loaded]
            mean_dose = (sum(doses) * 1.602176634e-19 / plan["boxMass_kg"]) / n_total if n_total else float("nan")
        else:
            common = np.zeros(len(grid), dtype=bool)
            tracks, mean_dose = 0.0, 0.0
        for s in species:
            if n:
                y = np.array([item[1][s] for item in loaded])
                g, se = ratio_estimator(y[:, common], e[:, common])
            else:
                g = se = np.array([])
            for t, gv, sv in zip(grid[common], g, se):
                time_rows.append({"rate_Gy_s": rate, "time_ns": t, "species": s,
                                  "G_per_100eV": gv, "SE": sv, "n_realizations": n})
            has_end = n and common[-1]
            one_ms[(rate, s)] = {
                "rate_Gy_s": rate, "species": s,
                "G_per_100eV": g[-1] if has_end else float("nan"),
                "SE": se[-1] if has_end else float("nan"),
                "n_realizations": n, "n_empty": n_empty,
                "mean_tracks": tracks, "mean_dose_Gy": mean_dose}
    ref = min(per_rate)
    final_rows = []
    for (rate, s), row in one_ms.items():
        r = one_ms[(ref, s)]
        row["delta_vs_ref"] = row["G_per_100eV"] - r["G_per_100eV"]
        row["delta_SE"] = (math.sqrt(row["SE"] ** 2 + r["SE"] ** 2) if rate != ref else float("nan"))
        final_rows.append(row)
    return time_rows, final_rows


TIME_COLUMNS = ["rate_Gy_s", "time_ns", "species", "G_per_100eV", "SE", "n_realizations"]
ONE_MS_COLUMNS = ["rate_Gy_s", "species", "G_per_100eV", "SE", "n_realizations", "n_empty",
                  "mean_tracks", "mean_dose_Gy", "delta_vs_ref", "delta_SE"]


def write_csv(path, columns, rows):
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(columns)
        for row in rows:
            writer.writerow([repr(float(row[c])) if isinstance(row[c], (float, np.floating)) else row[c]
                             for c in columns])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--plan", required=True, help="plan.json")
    parser.add_argument("--out", help="output directory (default: next to plan.json)")
    parser.add_argument("--allow-partial", action="store_true", help="ignore runs without output")
    args = parser.parse_args(argv)
    plan_dir = os.path.dirname(os.path.abspath(args.plan))
    with open(args.plan, encoding="utf-8") as handle:
        plan = json.load(handle)
    out_dir = args.out or plan_dir
    os.makedirs(out_dir, exist_ok=True)
    try:
        time_rows, final_rows = analyze(plan, plan_dir, allow_partial=args.allow_partial)
    except (OSError, RuntimeError) as error:
        print("analyze: " + str(error), file=sys.stderr)
        return 1
    write_csv(os.path.join(out_dir, "gvalues_time.csv"), TIME_COLUMNS, time_rows)
    write_csv(os.path.join(out_dir, "gvalues_1ms.csv"), ONE_MS_COLUMNS, final_rows)
    print("wrote %d time rows and %d endpoint rows to %s" % (len(time_rows), len(final_rows), out_dir))
    return 0


if __name__ == "__main__":
    sys.exit(main())
