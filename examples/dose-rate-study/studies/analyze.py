#!/usr/bin/env python
"""Mean G(t) +- SE over realizations of the doseCutoff histories of a plan.

Usage:
    python analyze.py --plan <plan dir>/plan.json [--plan <other plan.json> ...] [--out <dir>]
                      [--allow-partial]

Several --plan options pool their runs (a run listed by two plans, same output dir, counts once);
--out defaults to the directory of the first plan.

Reads species.csv and ledger.json of every run (run_study.ps1 output). A run without a passing
ledger is an error unless --allow-partial, which skips it (listed in the summary).

Per rate, time and species, over the n realizations that have the time (ratio estimator):
  G_cum(t)   = 100 * sum_r N_r(t) / sum_r E_r(t)       (E_r(t): deposit up to t, the cumulative dose)
  G_final(t) = 100 * sum_r N_r(t) / sum_r E_r(beam-off)
  SE by the delta method: R = sum y / sum x, SE = 100 sqrt(sum (y_r - R x_r)^2 / (n (n-1))) / mean(x);
  NaN when n < 2 (or sum x = 0).
Times: the common log grid t_k = 1 ps * 10^(k/10) (same for every rate). Each history also has
its own beam-off row, aggregated separately (gvalues_beamoff.csv, mean beam-off time). A row is
`extrapolated` when any realization's row is (steady-state stop: N held at the last window mean);
`extrapolatedFraction` gives the share.

Writes gvalues_time.csv and gvalues_beamoff.csv (all rates), plus one gvalues_time_rate<r>.csv per
rate (same columns):
  rate_Gy_s,time_ns,species,G_cum,SE_cum,G_final,SE_final,n_realizations,extrapolated,extrapolatedFraction
  (beam-off: plus mean_dose_Gy, mean_beamOff_ns)
"""
import argparse
import csv
import json
import math
import os
import sys
from collections import defaultdict

GRID_START_NS = 1e-3
GRID_PER_DECADE = 10


def grid_index(time_ns):
    """k if time_ns is on the common grid (relative 1e-9), else None."""
    if time_ns <= 0:
        return None
    x = GRID_PER_DECADE * math.log10(time_ns / GRID_START_NS)
    k = round(x)
    return k if abs(x - k) < 1e-6 else None


def grid_time(k):
    return GRID_START_NS * 10.0 ** (k / GRID_PER_DECADE)


def ratio_estimate(ys, xs):
    """(100 * sum y / sum x, delta-method SE). NaN-safe."""
    n = len(ys)
    sx = sum(xs)
    if n == 0 or sx <= 0:
        return float("nan"), float("nan")
    r = sum(ys) / sx
    if n < 2:
        return 100.0 * r, float("nan")
    var = sum((y - r * x) ** 2 for y, x in zip(ys, xs)) / (n * (n - 1))
    return 100.0 * r, 100.0 * math.sqrt(var) / (sx / n)


def read_species(path):
    """-> (grid rows {k: {species: row}}, beam-off rows {species: row}, beam-off time)."""
    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    grid, last_time = defaultdict(dict), max(float(r["timeNs"]) for r in rows)
    beam_off = {}
    for row in rows:
        t = float(row["timeNs"])
        rec = {"N": float(row["count"]), "E": float(row["energyDepositEv"]),
               "dose": float(row["doseGy"]), "x": int(row["extrapolated"])}
        if t == last_time:
            beam_off[row["species"]] = rec
        k = grid_index(t)
        if k is not None and t != last_time:
            grid[k][row["species"]] = rec
    return grid, beam_off, last_time


def aggregate(runs):
    """runs: list of (grid, beam_off, beam_off_time) of one rate -> (time rows, beam-off rows)."""
    finals = [next(iter(b.values()))["E"] for _, b, _ in runs]   # E at beam-off (same for every species)
    out_time, out_off = [], []
    keys = sorted({k for g, _, _ in runs for k in g})
    species = sorted({s for g, _, _ in runs for k in g for s in g[k]} | {s for _, b, _ in runs for s in b})
    for k in keys:
        for s in species:
            members = [(g[k][s], f) for (g, _, _), f in zip(runs, finals) if k in g and s in g[k]]
            if not members:
                continue
            out_time.append(_row(grid_time(k), s, members))
    for s in species:
        members = [(b[s], f) for (_, b, _), f in zip(runs, finals) if s in b]
        if members:
            row = _row(sum(t for _, _, t in runs) / len(runs), s, members)
            row["mean_dose_Gy"] = sum(m["dose"] for m, _ in members) / len(members)
            out_off.append(row)
    return out_time, out_off


def _row(time_ns, species, members):
    ys = [m["N"] for m, _ in members]
    g_cum, se_cum = ratio_estimate(ys, [m["E"] for m, _ in members])
    g_fin, se_fin = ratio_estimate(ys, [f for _, f in members])
    nx = sum(m["x"] for m, _ in members)
    return {"time_ns": time_ns, "species": species, "G_cum": g_cum, "SE_cum": se_cum,
            "G_final": g_fin, "SE_final": se_fin, "n_realizations": len(members),
            "extrapolated": int(nx > 0), "extrapolatedFraction": nx / len(members)}


def load_runs(plan_path, allow_partial, by_rate=None, seen=None):
    """Adds the runs of one plan to by_rate; seen (set of normalised output dirs) skips repeats."""
    plan_dir = os.path.dirname(os.path.abspath(plan_path))
    with open(plan_path, encoding="utf-8") as handle:
        plan = json.load(handle)
    by_rate = defaultdict(list) if by_rate is None else by_rate
    seen = set() if seen is None else seen
    skipped = []
    for run in plan["runs"]:
        out = os.path.join(plan_dir, run["output"])
        key = os.path.normcase(os.path.normpath(out))
        if key in seen:
            continue
        seen.add(key)
        ledger_path = os.path.join(out, "ledger.json")
        ok = False
        if os.path.exists(ledger_path):
            with open(ledger_path, encoding="utf-8") as handle:
                ok = bool(json.load(handle).get("checks", {}).get("allPassed"))
        if not ok or not os.path.exists(os.path.join(out, "species.csv")):
            if not allow_partial:
                raise ValueError("run %s has no passing ledger (use --allow-partial to skip it)" % run["id"])
            skipped.append(run["id"])
            continue
        by_rate[float(run["rate_Gy_s"])].append(read_species(os.path.join(out, "species.csv")))
    return plan, by_rate, skipped


TIME_COLS = ["rate_Gy_s", "time_ns", "species", "G_cum", "SE_cum", "G_final", "SE_final",
             "n_realizations", "extrapolated", "extrapolatedFraction"]


def write_csv(path, rows, cols):
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=cols, lineterminator="\n", extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({k: ("%.10g" % v if isinstance(v, float) else v) for k, v in row.items()})


def rate_file_name(rate):
    return "gvalues_time_rate%s.csv" % ("%g" % rate)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--plan", required=True, action="append")
    parser.add_argument("--out")
    parser.add_argument("--allow-partial", action="store_true")
    args = parser.parse_args(argv)
    out = args.out or os.path.dirname(os.path.abspath(args.plan[0]))
    by_rate, seen, skipped = defaultdict(list), set(), []
    try:
        for plan_path in args.plan:
            skipped += load_runs(plan_path, args.allow_partial, by_rate, seen)[2]
    except (ValueError, OSError, KeyError) as error:
        print("analyze: " + str(error), file=sys.stderr)
        return 2
    time_rows, off_rows = [], []
    for rate in sorted(by_rate):
        t, o = aggregate(by_rate[rate])
        for row in t:
            time_rows.append(dict(row, rate_Gy_s=rate))
        for row in o:
            off_rows.append(dict(row, rate_Gy_s=rate, mean_beamOff_ns=row["time_ns"]))
    os.makedirs(out, exist_ok=True)
    write_csv(os.path.join(out, "gvalues_time.csv"), time_rows, TIME_COLS)
    for rate in sorted(by_rate):
        write_csv(os.path.join(out, rate_file_name(rate)),
                  [r for r in time_rows if r["rate_Gy_s"] == rate], TIME_COLS)
    write_csv(os.path.join(out, "gvalues_beamoff.csv"), off_rows,
              TIME_COLS + ["mean_dose_Gy", "mean_beamOff_ns"])
    print("analyze: %d rates, %s realizations, skipped %s"
          % (len(by_rate), {("%g" % r): len(v) for r, v in sorted(by_rate.items())}, skipped or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
