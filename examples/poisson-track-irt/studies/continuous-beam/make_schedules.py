#!/usr/bin/env python
"""Build the schedules, history configs and plan.json of the dose-rate G-value study.

Usage:
    python make_schedules.py --library <capture library dir> --study study.json --out <plan dir>

See README.md for the study.json format and the plan.json layout.
"""
import argparse
import csv
import json
import os
import sys

import numpy as np

EV_TO_J = 1.602176634e-19
WATER_DENSITY_KG_M3 = 1000.0
MAX_ROWS = 100000          # history schedule limit
MAX_SEED = 2147483647      # history chemistrySeed limit

DEFAULTS = {
    "boxSideUm": 5.0,
    "windowNs": 1.0e6,
    "endpointNs": 1.0e6,
    "gridMinAgeNs": 1.0,
    "gridPointsPerDecade": 3,
    "chemistrySeedBase": 1000,
}


def box_mass_kg(box_side_um):
    side_m = box_side_um * 1e-6
    return WATER_DENSITY_KG_M3 * side_m ** 3


def poisson_rate_hz(dose_rate_gy_s, box_mass, mean_edep_ev):
    """Mean arrival rate [1/s] of tracks giving the dose rate in a box of the given mass."""
    return dose_rate_gy_s * box_mass / (mean_edep_ev * EV_TO_J)


def rate_key(rate):
    return "%g" % float(rate)


def read_library(library_dir):
    """Return (number of packets, mean energy deposit [eV], origins) of a capture library.

    origins[i] is packet i's primaryOrigin_nm (x, y, z) from capture.json. Packet molecule positions
    are stored relative to that origin, so the schedule offset must be the origin itself to put the
    track back where it was captured (the -z face of the 5 um box).
    """
    path = os.path.join(library_dir, "library.csv")
    with open(path, newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise ValueError("library.csv is empty: " + path)
    indices = sorted(int(r["packetIndex"]) for r in rows)
    if indices != list(range(len(rows))):
        raise ValueError("library.csv packet indices are not 0..N-1")
    mean = sum(float(r["energyDepositEv"]) for r in rows) / len(rows)
    if not mean > 0:
        raise ValueError("library mean energy deposit is not positive")
    with open(os.path.join(library_dir, "capture.json"), encoding="utf-8") as handle:
        packets = json.load(handle)["packets"]
    origins = {}
    for packet in packets:
        origin = packet.get("primaryOrigin_nm")
        if origin is None or len(origin) != 3:
            raise ValueError("capture.json: packet %s has no primaryOrigin_nm" % packet.get("packetIndex"))
        origins[int(packet["packetIndex"])] = [float(v) for v in origin]
    if sorted(origins) != list(range(len(rows))):
        raise ValueError("capture.json packets do not match library.csv")
    return len(rows), mean, [origins[i] for i in range(len(rows))]


def load_study(path):
    with open(path, encoding="utf-8") as handle:
        raw = json.load(handle)
    study = dict(DEFAULTS)
    study.update(raw)
    for key in ("rates", "realizations", "sourceSeed"):
        if key not in study:
            raise ValueError("study.json: missing key '%s'" % key)
    study["rates"] = [float(r) for r in study["rates"]]
    if not study["rates"] or any(r <= 0 for r in study["rates"]):
        raise ValueError("study.json: rates must be a non-empty list of positive numbers")
    realizations = {}
    for rate in study["rates"]:
        key = rate_key(rate)
        found = [k for k in study["realizations"] if float(k) == rate]
        if len(found) != 1:
            raise ValueError("study.json: realizations needs exactly one entry for rate %s" % key)
        n = int(study["realizations"][found[0]])
        if n < 0:
            raise ValueError("study.json: negative realization count for rate %s" % key)
        realizations[key] = n
    study["realizations"] = realizations
    if int(study["sourceSeed"]) < 1:
        raise ValueError("study.json: sourceSeed must be a positive integer")
    if study["windowNs"] > study["endpointNs"]:
        raise ValueError("study.json: windowNs must not exceed endpointNs")
    return study


def draw_arrivals(rng, mean_count, window_ns):
    """Poisson count, then that many uniform times on [0, window), sorted."""
    count = int(rng.poisson(mean_count))
    return np.sort(rng.uniform(0.0, window_ns, size=count))


def build_plan(study, n_packets, mean_edep_ev):
    """Draw every realization and assign disjoint packet slices. Pure function (no files)."""
    box_mass = box_mass_kg(study["boxSideUm"])
    rng = np.random.Generator(np.random.PCG64(int(study["sourceSeed"])))
    window_ns = float(study["windowNs"])
    runs = []
    empty = {}
    next_packet = 0
    realization_counter = 0
    for rate in study["rates"]:
        key = rate_key(rate)
        lam = poisson_rate_hz(rate, box_mass, mean_edep_ev)
        empty[key] = 0
        for r in range(study["realizations"][key]):
            seed = int(study["chemistrySeedBase"]) + realization_counter
            realization_counter += 1
            if seed > MAX_SEED:
                raise ValueError("chemistry seed exceeds %d" % MAX_SEED)
            times = draw_arrivals(rng, lam * window_ns * 1e-9, window_ns)
            n = len(times)
            if n == 0:
                empty[key] += 1
                continue
            if n > MAX_ROWS:
                raise ValueError("rate %s realization %d has %d arrivals (> %d)" % (key, r, n, MAX_ROWS))
            runs.append({
                "id": "r%s_%03d" % (key, r),
                "rate_Gy_s": rate,
                "realization": r,
                "chemistrySeed": seed,
                "nPackets": n,
                "packetStart": next_packet,
                "timesNs": [float(t) for t in times],
            })
            next_packet += n
    if next_packet > n_packets:
        raise ValueError(
            "the library is too small: the drawn realizations need %d packets, the library has %d "
            "(capture at least %d packets, or lower the rates/realizations)"
            % (next_packet, n_packets, next_packet))
    return {"runs": runs, "empty": empty, "packetsUsed": next_packet,
            "meanArrivalRateHz": {rate_key(r): poisson_rate_hz(r, box_mass, mean_edep_ev) for r in study["rates"]},
            "boxMass_kg": box_mass}


def relpath_or_abs(path, start):
    try:
        return os.path.relpath(path, start).replace("\\", "/")
    except ValueError:  # other drive on Windows
        return os.path.abspath(path).replace("\\", "/")


def write_schedule(path, run, endpoint_ns, origins_nm):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("# time_ns packet_index dx_um dy_um dz_um\n")
        previous = 0.0
        for k, t in enumerate(run["timesNs"]):
            t = max(round(float(t), 6), previous)   # keep nondecreasing after rounding
            if not t < endpoint_ns:
                raise ValueError("arrival time %r is not below the endpoint" % t)
            previous = t
            packet = run["packetStart"] + k
            dx, dy, dz = (v * 1e-3 for v in origins_nm[packet])      # nm -> um
            handle.write("%.6f %d %.12f %.12f %.12f\n" % (t, packet, dx, dy, dz))


def write_config(path, study, run, library_dir, schedule_path):
    base = os.path.dirname(path)
    lines = [
        "# generated by make_schedules.py",
        "mode = deterministic",
        "libraryDir = %s" % relpath_or_abs(library_dir, base),
        "endpointNs = %r" % float(study["endpointNs"]),
        "chemistrySeed = %d" % run["chemistrySeed"],
        "scheduleFile = %s" % relpath_or_abs(schedule_path, base),
        "worldHalfSideUm = 1000",
        "halfBoxUm = %r" % (study["boxSideUm"] / 2.0),
        "gridMinAgeNs = %r" % float(study["gridMinAgeNs"]),
        "gridPointsPerDecade = %d" % int(study["gridPointsPerDecade"]),
    ]
    if float(study.get("progressIntervalS", 0)) > 0:
        lines.append("progressIntervalS = %r" % float(study["progressIntervalS"]))
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def write_study(out_dir, library_dir, study, plan, n_packets, mean_edep_ev, output_root, origins_nm):
    os.makedirs(out_dir, exist_ok=True)
    entries = []
    for run in plan["runs"]:
        run_dir = os.path.join(out_dir, "runs", run["id"])
        os.makedirs(run_dir, exist_ok=True)
        schedule = os.path.join(run_dir, "schedule.txt")
        config = os.path.join(run_dir, "history.cfg")
        write_schedule(schedule, run, float(study["endpointNs"]), origins_nm)
        write_config(config, study, run, library_dir, schedule)
        entries.append({
            "id": run["id"],
            "rate_Gy_s": run["rate_Gy_s"],
            "realization": run["realization"],
            "chemistrySeed": run["chemistrySeed"],
            "nPackets": run["nPackets"],
            "packetStart": run["packetStart"],
            "config": relpath_or_abs(config, out_dir),
            "schedule": relpath_or_abs(schedule, out_dir),
            "output": relpath_or_abs(os.path.join(output_root, run["id"]), out_dir),
        })
    plan_doc = {
        "formatVersion": 1,
        "library": relpath_or_abs(library_dir, out_dir),
        "librarySize": n_packets,
        "libraryMeanEnergyDepositEv": mean_edep_ev,
        "packetsUsed": plan["packetsUsed"],
        "boxSideUm": study["boxSideUm"],
        "boxMass_kg": plan["boxMass_kg"],
        "windowNs": study["windowNs"],
        "endpointNs": study["endpointNs"],
        "gridMinAgeNs": study["gridMinAgeNs"],
        "gridPointsPerDecade": study["gridPointsPerDecade"],
        "sourceSeed": study["sourceSeed"],
        "chemistrySeedBase": study["chemistrySeedBase"],
        "rates": study["rates"],
        "realizations": study["realizations"],
        "emptyRealizations": plan["empty"],
        "meanArrivalRateHz": plan["meanArrivalRateHz"],
        "outputRoot": relpath_or_abs(output_root, out_dir),
        "runs": entries,
    }
    with open(os.path.join(out_dir, "plan.json"), "w", encoding="utf-8") as handle:
        json.dump(plan_doc, handle, indent=2)
    return plan_doc


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--library", required=True, help="capture library directory (library.csv)")
    parser.add_argument("--study", required=True, help="study.json")
    parser.add_argument("--out", required=True, help="directory for plan.json, runs/<id>/ (new or empty)")
    parser.add_argument("--output-root", help="where run_study.ps1 writes the history outputs (default <out>/out)")
    args = parser.parse_args(argv)
    if os.path.isdir(args.out) and os.listdir(args.out):
        print("make_schedules: output directory is not empty: " + args.out, file=sys.stderr)
        return 2
    try:
        study = load_study(args.study)
        n_packets, mean_edep, origins = read_library(args.library)
        plan = build_plan(study, n_packets, mean_edep)
        output_root = args.output_root or os.path.join(args.out, "out")
        doc = write_study(args.out, args.library, study, plan, n_packets, mean_edep, output_root, origins)
    except (ValueError, OSError) as error:
        print("make_schedules: " + str(error), file=sys.stderr)
        return 2
    print("plan: %d runs, %d packets used of %d, empty draws %s"
          % (len(doc["runs"]), doc["packetsUsed"], n_packets, json.dumps(doc["emptyRealizations"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
