#!/usr/bin/env python
"""Timing-spike table of the dose-rate study.

Usage:
    python spike_report.py <spike dir> [--evidence <file.md>]

Scans <spike dir> for plan directories (plan.json + run_log.csv, from make_configs.py and
run_study.ps1) and writes, for every run (latest run_log row per id):
  rate, status (ok / truncated / FAILED), wall time, peak working set, arrivals activated / scheduled,
  simulated time reached, dose reached, live molecules at the end, steady-state stop (time, arrivals)
  or none, snapshots.h5 size when present, and the estimated wall time of the full history.
Full-history estimate: the measured wall time of a finished run; for a truncated run, the measured
wall time plus the remaining arrivals at the activation rate of the last third of its
HISTORY_PROGRESS lines (a lower bound when the history slows down as molecules accumulate).
Outputs spike_table.md and spike_table.csv in <spike dir>; with --evidence (default
<spike dir>/../evidence/04-spike-table.md when that directory exists) also a copy of the markdown.
"""
import argparse
import csv
import json
import os
import re
import sys

EV_TO_J = 1.602176634e-19
PROGRESS = re.compile(r"HISTORY_PROGRESS wall_s=(\S+) simTime_ns=(\S+) endpoint_ns=(\S+) "
                      r"activated=(\d+)/(\d+) liveMolecules=(\d+)")


def parse_progress(lines):
    """[(wall_s, simTime_ns, endpoint_ns, activated, scheduled, live)] from log lines."""
    out = []
    for line in lines:
        m = PROGRESS.search(line)
        if m:
            w, t, e, a, n, live = m.groups()
            out.append((float(w), float(t), float(e), int(a), int(n), int(live)))
    return out


def estimate_full_wall(wall_s, progress):
    """Measured wall + remaining arrivals at the recent activation rate (None if unknown)."""
    if not progress:
        return None
    last = progress[-1]
    remaining = last[4] - last[3]
    if remaining <= 0:
        return wall_s
    tail = progress[-max(2, len(progress) // 3):]
    if len(tail) < 2 or tail[-1][3] <= tail[0][3]:
        return None
    rate = (tail[-1][3] - tail[0][3]) / (tail[-1][0] - tail[0][0])
    return wall_s + remaining / rate


def live_at_end(species_csv):
    """Sum of counts at the last measured (non-extrapolated) species.csv time."""
    with open(species_csv, newline="", encoding="utf-8") as handle:
        rows = [r for r in csv.DictReader(handle) if r["extrapolated"] == "0"]
    if not rows:
        return None
    t = max(float(r["timeNs"]) for r in rows)
    return int(sum(float(r["count"]) for r in rows if float(r["timeNs"]) == t))


def latest_log_rows(path):
    with open(path, newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    latest = {}
    for row in rows:
        latest[row["id"]] = row
    return latest


def library_deposits(library_dir):
    with open(os.path.join(library_dir, "library.csv"), newline="", encoding="utf-8") as handle:
        return [float(r["energyDepositEv"]) for r in csv.DictReader(handle)]


def fmt(v, spec="%.3g"):
    if v is None:
        return ""
    return spec % v if isinstance(v, float) else str(v)


def collect(spike_dir):
    rows = []
    for root, _, files in os.walk(spike_dir):
        if "plan.json" not in files or "run_log.csv" not in files:
            continue
        with open(os.path.join(root, "plan.json"), encoding="utf-8") as handle:
            plan = json.load(handle)
        mass = plan["boxMass_kg"]
        deposits = library_deposits(os.path.join(root, plan["library"]))
        log = latest_log_rows(os.path.join(root, "run_log.csv"))
        out_root = os.path.join(root, plan["outputRoot"])
        for run in plan["runs"]:
            entry = log.get(run["id"])
            if entry is None:
                continue
            out = os.path.join(root, run["output"])
            log_path = os.path.join(out_root, "logs", run["id"] + ".log")
            lines = open(log_path, encoding="utf-8", errors="replace").read().splitlines() \
                if os.path.exists(log_path) else []
            progress = parse_progress(lines)
            wall = float(entry["wallTime_s"])
            row = {"plan": os.path.relpath(root, spike_dir).replace("\\", "/"), "id": run["id"],
                   "rate_Gy_s": float(run["rate_Gy_s"]), "status": entry["status"], "wall_s": wall,
                   "peakWorkingSet_MB": float(entry["peakWorkingSet_MB"]), "snapshots": run.get("snapshots", False)}
            ledger_path = os.path.join(out, "ledger.json")
            if entry["status"] == "ok" and os.path.exists(ledger_path):
                with open(ledger_path, encoding="utf-8") as handle:
                    ledger = json.load(handle)
                ss = ledger.get("steadyState", {})
                dose = ledger["dose"]
                row.update(arrivals=dose["arrivalCount"], scheduled=dose["arrivalCount"],
                           simTime_ns=ledger["settings"]["endpoint_ns"], beamOff_ns=dose["beamOff_ns"],
                           dose_Gy=dose["final_Gy"], liveAtEnd=live_at_end(os.path.join(out, "species.csv")),
                           steadyStop=bool(ss.get("reached")) and bool(ss.get("stoppedEarly", ss.get("reached"))),
                           steadyStop_ns=ss.get("stopTime_ns"), steadyStopArrivals=ss.get("activatedSourcesAtStop"),
                           fullWall_s=wall)
                if row["steadyStop"]:
                    row["arrivals"] = ss.get("activatedSourcesAtStop")
                    row["dose_Gy"] = sum(deposits[run["firstPacket"]:run["firstPacket"] + row["arrivals"]]) \
                        * EV_TO_J / mass
                    row["simTime_ns"] = ss.get("stopTime_ns")
            else:
                last = progress[-1] if progress else None
                activated = last[3] if last else 0
                row.update(arrivals=activated, scheduled=last[4] if last else None,
                           simTime_ns=last[1] if last else None, beamOff_ns=last[2] if last else None,
                           dose_Gy=sum(deposits[run["firstPacket"]:run["firstPacket"] + activated]) * EV_TO_J / mass,
                           liveAtEnd=last[5] if last else None, steadyStop=False, steadyStop_ns=None,
                           steadyStopArrivals=None, fullWall_s=estimate_full_wall(wall, progress))
            snap = os.path.join(out, "snapshots.h5")
            row["snapshot_MB"] = os.path.getsize(snap) / 2 ** 20 if os.path.exists(snap) else None
            rows.append(row)
    rows.sort(key=lambda r: (r["snapshots"], r["rate_Gy_s"]))
    return rows


COLS = [("rate_Gy_s", "rate (Gy/s)", "%g"), ("status", "status", None), ("wall_s", "wall (s)", "%.0f"),
        ("peakWorkingSet_MB", "peak RSS (MB)", "%.0f"), ("arrivals", "arrivals", None),
        ("scheduled", "scheduled", None), ("simTime_ns", "sim time reached (ns)", "%.3g"),
        ("beamOff_ns", "beam-off (ns)", "%.3g"), ("dose_Gy", "dose reached (Gy)", "%.3f"),
        ("liveAtEnd", "live molecules at end", None), ("steadyStop", "steady-state stop", None),
        ("steadyStop_ns", "stop time (ns)", "%.3g"), ("steadyStopArrivals", "arrivals at stop", None),
        ("snapshot_MB", "snapshots.h5 (MB)", "%.2f"), ("fullWall_s", "est. full history (s)", "%.0f"),
        ("id", "run", None), ("plan", "plan", None)]


def to_markdown(rows):
    head = "| " + " | ".join(c[1] for c in COLS) + " |"
    sep = "|" + "|".join("---" for _ in COLS) + "|"
    body = []
    for r in rows:
        cells = []
        for key, _, spec in COLS:
            v = r.get(key)
            if key == "steadyStop":
                v = "yes" if v else "none"
            cells.append(fmt(v, spec) if spec else fmt(v))
        body.append("| " + " | ".join(cells) + " |")
    note = ("\nest. full history: measured wall for finished runs; for truncated runs, measured wall + "
            "remaining arrivals at the activation rate of the last third of the progress lines "
            "(lower bound if the run keeps slowing down).\n")
    return "\n".join([head, sep] + body) + "\n" + note


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("spike_dir")
    parser.add_argument("--evidence")
    args = parser.parse_args(argv)
    rows = collect(args.spike_dir)
    if not rows:
        print("spike_report: no run found under " + args.spike_dir, file=sys.stderr)
        return 2
    md = to_markdown(rows)
    with open(os.path.join(args.spike_dir, "spike_table.md"), "w", encoding="utf-8") as handle:
        handle.write(md)
    with open(os.path.join(args.spike_dir, "spike_table.csv"), "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=[c[0] for c in COLS], lineterminator="\n", extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    evidence = args.evidence
    default_dir = os.path.join(args.spike_dir, os.pardir, "evidence")
    if evidence is None and os.path.isdir(default_dir):
        evidence = os.path.join(default_dir, "04-spike-table.md")
    if evidence:
        with open(evidence, "w", encoding="utf-8") as handle:
            handle.write("# Ticket 04 timing spike\n\n" + md)
    print(md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
