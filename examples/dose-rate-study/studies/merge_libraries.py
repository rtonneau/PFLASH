#!/usr/bin/env python
"""Merge capture libraries (written by separate `capture` processes) into one.

Usage:
    python merge_libraries.py --out <new dir> <part dir> [<part dir> ...]

Packets are renumbered 0..N-1 in the order of the parts, packet files are copied
under their new names, library.csv is rewritten, and capture.json is rebuilt from
the first part's header with the packets, the skipped empty events and the totals
of every part. The parts must share the compatibility tag, the capture box and the
world, and their physics seeds must be distinct. Prints the merged statistics and
writes them to <out>/merge.json.
"""
import argparse
import csv
import json
import os
import shutil
import statistics
import sys

SAME_KEYS = ("compatibilityTag", "captureHalfSide_nm", "worldHalfSide_nm", "captureTime_ns", "physics",
             "chemistry", "macro")
SUM_KEYS = ("packetCount", "moleculeCount", "primaryEnergy_eV", "energyDeposit_eV", "escapedEnergy_eV",
            "outsideDeposit_eV", "worldEscape_eV", "droppedMoleculeCount")


def packet_file_name(index):
    return "packet-%05d.txt" % index


def read_part(directory):
    with open(os.path.join(directory, "library.csv"), newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        header = reader.fieldnames
    with open(os.path.join(directory, "capture.json"), encoding="utf-8") as handle:
        summary = json.load(handle)
    if [int(r["packetIndex"]) for r in rows] != list(range(len(rows))):
        raise ValueError(directory + ": library.csv packet indices are not 0..N-1")
    if len(summary["packets"]) != len(rows):
        raise ValueError(directory + ": capture.json and library.csv disagree on the packet count")
    return header, rows, summary


def merge_summaries(parts):
    """Pure merge of (header, rows, summary) parts -> (header, rows, summary, stats)."""
    if not parts:
        raise ValueError("no part to merge")
    header0, _, first = parts[0]
    seeds = set()
    out_rows, out_packets, skipped = [], [], []
    totals = {k: 0 for k in SUM_KEYS}
    max_residual = 0.0
    for header, rows, summary in parts:
        if header != header0:
            raise ValueError("library.csv headers differ between parts")
        for key in SAME_KEYS:
            if summary.get(key) != first.get(key):
                raise ValueError("parts differ in %s" % key)
        for row, packet in zip(rows, summary["packets"]):
            seed = int(row["physicsSeed"])
            if seed in seeds:
                raise ValueError("physics seed %d appears twice" % seed)
            seeds.add(seed)
            index = len(out_rows)
            new_row = dict(row, packetIndex=str(index), file=packet_file_name(index))
            new_packet = dict(packet, packetIndex=index, file=packet_file_name(index))
            new_row["_source"] = row["file"]
            out_rows.append(new_row)
            out_packets.append(new_packet)
        for event in summary["skippedEmptyEvents"]:
            if int(event["physicsSeed"]) in seeds:
                raise ValueError("skipped physics seed %s appears twice" % event["physicsSeed"])
            seeds.add(int(event["physicsSeed"]))
            skipped.append(event)
        for key in SUM_KEYS:
            totals[key] += summary["totals"].get(key, 0)
        max_residual = max(max_residual, summary["totals"].get("maxAbsEnergyResidual_eV", 0.0))
    totals["maxAbsEnergyResidual_eV"] = max_residual
    merged = dict(first)
    merged["firstPhysicsSeed"] = min(int(r["physicsSeed"]) for r in out_rows)
    merged["maxEmptyEvents"] = sum(p[2].get("maxEmptyEvents", 0) for p in parts)
    merged["skippedEmptyEvents"] = skipped
    merged["meanBoxDeposit_eV"] = totals["energyDeposit_eV"] / totals["packetCount"]
    merged["totals"] = totals
    merged["packets"] = out_packets
    merged["mergedFrom"] = len(parts)
    deposits = [float(r["energyDepositEv"]) for r in out_rows]
    launched = len(out_rows) + len(skipped)
    stats = {
        "parts": len(parts),
        "packetCount": len(out_rows),
        "skippedEmptyEvents": len(skipped),
        "hitFraction": len(out_rows) / launched,
        "moleculeCount": sum(int(r["moleculeCount"]) for r in out_rows),
        "energyDepositSum_eV": sum(deposits),
        "meanDepositPerPacket_eV": statistics.fmean(deposits),
        "sdDepositPerPacket_eV": statistics.stdev(deposits) if len(deposits) > 1 else float("nan"),
        "minDeposit_eV": min(deposits),
        "maxDeposit_eV": max(deposits),
        "meanDepositPerLaunched_eV": sum(deposits) / launched,
        "maxAbsEnergyResidual_eV": max_residual,
    }
    return header0, out_rows, merged, stats


def library_dose_gy(energy_ev, half_box_um):
    mass_kg = 1000.0 * (2 * half_box_um * 1e-6) ** 3
    return energy_ev * 1.602176634e-19 / mass_kg


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True)
    parser.add_argument("parts", nargs="+")
    args = parser.parse_args(argv)
    if os.path.isdir(args.out) and os.listdir(args.out):
        print("merge_libraries: output directory is not empty: " + args.out, file=sys.stderr)
        return 2
    try:
        parts = [read_part(p) for p in args.parts]
        header, rows, merged, stats = merge_summaries(parts)
    except (ValueError, OSError, KeyError) as error:
        print("merge_libraries: " + str(error), file=sys.stderr)
        return 2
    os.makedirs(args.out, exist_ok=True)
    k = 0
    for part_dir, (_, part_rows, _) in zip(args.parts, parts):
        for _ in part_rows:
            row = rows[k]
            shutil.copyfile(os.path.join(part_dir, row.pop("_source")), os.path.join(args.out, row["file"]))
            k += 1
    with open(os.path.join(args.out, "library.csv"), "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=header, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    with open(os.path.join(args.out, "capture.json"), "w", encoding="utf-8") as handle:
        json.dump(merged, handle, indent=2)
    stats["libraryDose_Gy"] = library_dose_gy(stats["energyDepositSum_eV"], merged["captureHalfSide_nm"] * 1e-3)
    stats["parts_dirs"] = [os.path.abspath(p) for p in args.parts]
    with open(os.path.join(args.out, "merge.json"), "w", encoding="utf-8") as handle:
        json.dump(stats, handle, indent=2)
    print("MERGE_DONE " + " ".join("%s=%s" % (k, v) for k, v in stats.items() if k != "parts_dirs"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
