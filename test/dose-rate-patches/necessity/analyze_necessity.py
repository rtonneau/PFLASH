"""Analysis of the leave-one-out necessity runs (RunNecessity.ps1, ticket 06).

Reads <work>/runs/<variant>/<runId>{.launcher.txt, /ledger.json, /reactions.csv,
/activations.csv, /species.csv, /snapshots.h5} for every run of <work>/plan/plan.json
and writes one JSON (--out) for the report. Verdicts (each "PASS" when the
demonstration holds, else "NOT DEMONSTRATED" with the counts):

  patch1  finder: every history aborts on all-1 (memory cap or abort), every
          history completes on all. Coincident products (reactions with 3 or
          more products at one point, e_aq + e_aq -> 2 OH- + H2) are listed per
          run as the trigger.
  patch2  source contact: reactions between a source and an older one at the
          exact activation time of the new source: 0 on all-2, > 0 on all.
  patch3  rebound fold: molecules outside the box (any snapshot frame, plus the
          ledger's final allInsideBox check): > 0 on all-3, 0 on all.
  G       report only: G(beam-off) per species, mean and SE over realizations,
          delta all - variant with its SE and z, for all-2 and all-3.

Exit 0 when every verdict is PASS, 1 otherwise, 2 on missing inputs.
"""
import argparse
import csv
import json
import math
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "examples", "dose-rate-study", "scripts"))

ACTIVATION_TOLERANCE_NS = 1e-9


def read_launcher(path):
    out = {}
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            m = re.match(r"RESULT launcher\.(\w+)=(.*)$", line.strip())
            if m:
                out[m.group(1)] = m.group(2)
    if "reason" not in out:
        return None
    return {"reason": out["reason"], "exitCode": int(out.get("exitCode", -1)),
            "peakMiB": float(out.get("peakMiB", "nan")), "seconds": float(out.get("seconds", "nan"))}


def read_csv(path):
    if not os.path.exists(path):
        return None
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def source_contacts(reactions, activations):
    """Reactions at a source's activation time between it and an older source."""
    times = {int(a["sourceIndex"]): float(a["observedActivationTimeNs"]) for a in activations}
    found = []
    for r in reactions:
        try:
            a, b = int(r["sourceA"]), int(r["sourceB"])
        except ValueError:
            continue
        if a < 0 or b < 0 or a == b:
            continue
        new, old = max(a, b), min(a, b)
        t = float(r["timeNs"])
        if new in times and abs(t - times[new]) <= ACTIVATION_TOLERANCE_NS * max(1.0, times[new]):
            found.append({"timeNs": t, "newSource": new, "oldSource": old,
                          "reaction": "%s + %s -> %s" % (r["reactantA"], r["reactantB"], r["products"])})
    return found


def coincident_products(reactions):
    out = []
    for r in reactions:
        products = [p for p in r["products"].split(" + ") if p]
        if len(products) >= 3:
            out.append({"timeNs": float(r["timeNs"]), "reaction": "%s + %s -> %s" % (
                r["reactantA"], r["reactantB"], r["products"]), "sourceA": r["sourceA"], "sourceB": r["sourceB"]})
    return out


def outside_box(path, half_nm):
    if not os.path.exists(path):
        return None
    import snapshots
    data = snapshots.read(path)
    per_frame = []
    worst = 0.0
    for f in data.frames:
        if len(f) == 0:
            per_frame.append(0)
            continue
        excess = np.max(np.abs(f.positions_nm.astype(float)), axis=1) - half_nm
        per_frame.append(int(np.sum(excess > 0)))
        worst = max(worst, float(np.max(excess)))
    return {"frames": len(data.frames), "outsidePerFrame": per_frame, "outsideTotal": int(sum(per_frame)),
            "outsideLastFrame": per_frame[-1] if per_frame else 0, "maxExcess_nm": worst}


def g_beam_off(species_rows):
    if not species_rows:
        return None
    t_end = max(float(r["timeNs"]) for r in species_rows)
    return {r["species"]: float(r["gFinalPer100eV"]) for r in species_rows if float(r["timeNs"]) == t_end}


def analyze_run(run_dir, half_nm):
    run = {"launcher": read_launcher(run_dir + ".launcher.txt")}
    ledger_path = os.path.join(run_dir, "ledger.json")
    ledger = None
    if os.path.exists(ledger_path):
        with open(ledger_path, encoding="utf-8") as handle:
            ledger = json.load(handle)
    run["ledger"] = None if ledger is None else {
        "allPassed": ledger["checks"]["allPassed"],
        "allInsideBox": ledger["checks"]["end"].get("allInsideBox"),
        "sourceActivation": ledger.get("sourceActivation"),
        "liveMolecules": ledger["checks"]["end"].get("liveMolecules"),
        "doseGy": ledger.get("dose", {}).get("finalDoseGy", ledger.get("dose", {}).get("final_Gy")),
        "wallTime_s": ledger.get("wallTime_s"),
        "reflectingBoxFolds": ledger.get("reflectingBoxFolds"),
    }
    launcher = run["launcher"]
    run["completed"] = bool(launcher and launcher["reason"] == "complete" and ledger is not None)
    run["aborted"] = bool(launcher and (launcher["reason"] != "complete" or ledger is None))
    reactions = read_csv(os.path.join(run_dir, "reactions.csv"))
    activations = read_csv(os.path.join(run_dir, "activations.csv"))
    if reactions is not None and activations is not None:
        contacts = source_contacts(reactions, activations)
        run["sourceContacts"] = len(contacts)
        run["sourceContactExamples"] = contacts[:5]
        coincident = coincident_products(reactions)
        run["coincidentProductReactions"] = len(coincident)
        run["coincidentProductExamples"] = coincident[:3]
        run["reactionsLogged"] = len(reactions)
    run["snapshots"] = outside_box(os.path.join(run_dir, "snapshots.h5"), half_nm)
    run["gBeamOff"] = g_beam_off(read_csv(os.path.join(run_dir, "species.csv")))
    return run


def mean_se(values):
    v = np.asarray(values, dtype=float)
    if len(v) == 0:
        return None, None
    return float(v.mean()), (float(v.std(ddof=1) / math.sqrt(len(v))) if len(v) > 1 else float("nan"))


def g_table(runs_ref, runs_var):
    species = sorted({s for r in runs_ref + runs_var if r.get("gBeamOff") for s in r["gBeamOff"]})
    rows = []
    for s in species:
        a = [r["gBeamOff"].get(s, 0.0) for r in runs_ref if r.get("gBeamOff")]
        b = [r["gBeamOff"].get(s, 0.0) for r in runs_var if r.get("gBeamOff")]
        ma, sa = mean_se(a)
        mb, sb = mean_se(b)
        if ma is None or mb is None:
            continue
        se = math.sqrt(sa ** 2 + sb ** 2) if not (math.isnan(sa) or math.isnan(sb)) else float("nan")
        delta = ma - mb
        z = delta / se if se and not math.isnan(se) else float("nan")
        rows.append({"species": s, "refMean": ma, "refSE": sa, "variantMean": mb, "variantSE": sb,
                     "delta": delta, "deltaSE": se, "z": z, "nRef": len(a), "nVariant": len(b)})
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--settings", required=True)
    parser.add_argument("--config", help="named config (default: defaultConfig)")
    parser.add_argument("--work", required=True, help="<necessity work dir>/<config>")
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    with open(args.settings, encoding="utf-8") as handle:
        settings = json.load(handle)
    config = args.config or settings["defaultConfig"]
    study = settings["configs"][config]
    plan_path = os.path.join(args.work, "plan", "plan.json")
    if not os.path.exists(plan_path):
        print("analyze_necessity: no plan " + plan_path, file=sys.stderr)
        return 2
    with open(plan_path, encoding="utf-8") as handle:
        plan = json.load(handle)
    half_nm = float(study["halfBoxUm"]) * 1000.0
    ids = [r["id"] for r in plan["runs"]]
    n = len(ids)
    runs = {}
    for v in settings["variants"]:
        runs[v] = {}
        for rid in ids:
            runs[v][rid] = analyze_run(os.path.join(args.work, "runs", v, rid), half_nm)
    missing = [(v, rid) for v in runs for rid in runs[v] if runs[v][rid]["launcher"] is None]
    ref = settings["reference"]

    def lst(v):
        return [runs[v][rid] for rid in ids]

    verdicts = {}
    # Patch 1.
    ref_completed = sum(r["completed"] for r in lst(ref))
    p1_aborted = sum(r["aborted"] for r in lst("all-1"))
    reasons = {}
    for r in lst("all-1"):
        key = r["launcher"]["reason"] if r["launcher"] else "missing"
        reasons[key] = reasons.get(key, 0) + 1
    coincident_runs = [rid for rid in ids if runs[ref][rid].get("coincidentProductReactions", 0) > 0]
    verdicts["patch1"] = {
        "pass": p1_aborted == n and ref_completed == n,
        "line": "patch1: all-1 aborted %d/%d (%s), %s completed %d/%d" % (
            p1_aborted, n, ", ".join("%s %d" % kv for kv in sorted(reasons.items())), ref, ref_completed, n),
        "allMinus1Aborted": p1_aborted, "allMinus1Reasons": reasons, "referenceCompleted": ref_completed,
        "runsWithCoincidentProducts": coincident_runs,
        "coincidentProductReactions": {rid: runs[ref][rid].get("coincidentProductReactions") for rid in ids},
    }
    # Patch 2.
    c_ref = sum(r.get("sourceContacts", 0) for r in lst(ref))
    c_var = sum(r.get("sourceContacts", 0) for r in lst("all-2"))
    verdicts["patch2"] = {
        "pass": c_var == 0 and c_ref > 0,
        "line": "patch2: new-old contacts at activation all-2 %d, %s %d" % (c_var, ref, c_ref),
        "allMinus2Contacts": c_var, "referenceContacts": c_ref,
        "perRun": {v: {rid: runs[v][rid].get("sourceContacts") for rid in ids} for v in (ref, "all-2")},
    }
    # Patch 3.

    def out_count(r):
        s = (r["snapshots"] or {}).get("outsideTotal", 0)
        final = 1 if (r["ledger"] and r["ledger"]["allInsideBox"] is False) else 0
        return s, final

    o_ref = [out_count(r) for r in lst(ref)]
    o_var = [out_count(r) for r in lst("all-3")]
    s_ref, f_ref = sum(a for a, _ in o_ref), sum(b for _, b in o_ref)
    s_var, f_var = sum(a for a, _ in o_var), sum(b for _, b in o_var)
    verdicts["patch3"] = {
        "pass": (s_var + f_var) > 0 and (s_ref + f_ref) == 0,
        "line": "patch3: out-of-box all-3 %d snapshot molecules, %d/%d runs failing allInsideBox; %s %d, %d/%d" % (
            s_var, f_var, n, ref, s_ref, f_ref, n),
        "allMinus3SnapshotOutside": s_var, "allMinus3FinalOutsideRuns": f_var,
        "referenceSnapshotOutside": s_ref, "referenceFinalOutsideRuns": f_ref,
        "maxExcess_nm": {v: max([(r["snapshots"] or {}).get("maxExcess_nm", 0.0) for r in lst(v)] or [0.0])
                         for v in (ref, "all-3")},
    }
    gdelta = {v: g_table(lst(ref), lst(v)) for v in ("all-2", "all-3")}
    result = {"formatVersion": 1, "config": config, "study": study, "settings": settings, "runIds": ids, "missingRuns": missing,
              "verdicts": verdicts, "gBeamOffDelta": gdelta, "runs": runs}
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=1)

    for key in ("patch1", "patch2", "patch3"):
        v = verdicts[key]
        print("%s -> %s" % (v["line"], "PASS" if v["pass"] else "NOT DEMONSTRATED"))
    for v, rows in gdelta.items():
        print("\nG(beam-off) /100 eV, %s - %s (report only):" % (ref, v))
        print("  %-10s %10s %10s %10s %8s" % ("species", ref, v, "delta", "z"))
        for row in rows:
            print("  %-10s %10.4f %10.4f %+10.4f %8.2f" % (
                row["species"], row["refMean"], row["variantMean"], row["delta"], row["z"]))
    if missing:
        print("\nmissing runs: %d" % len(missing), file=sys.stderr)
        return 2
    print("\nresults: " + args.out)
    return 0 if all(v["pass"] for v in verdicts.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
