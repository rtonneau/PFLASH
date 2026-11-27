#!/usr/bin/env python3
"""Acceptance checks of the phase-space source (README.md, "Acceptance").

Runs the built phsp-record and phsp-replay and checks, with numbers:

  ENERGY       stage-2 (chemistry off) deposit per history vs the stage-1
               in-box deposit per history over the same histories: paired
               difference d = stage 2 - stage 1, |mean(d) / SE(d)| < 3.
  REPEAT       the physics and pre-chemical columns of Histories.csv (every
               column) are identical for: the same range twice, the range
               split into two runs in one process, the second part alone in
               a fresh process, 2 threads, and another /random/setSeeds with
               the same base seed (chemistry on, short end time). Species.csv
               is not compared: the chemistry stage is not bit-repeatable from
               one process to the next (README, "Chemistry reproducibility").
  LATTICE      box near the surface (depth 50 um): Scoring lattice (pitch P,
               beam uniform over one P x P cell, fluence N / P^2) vs a single
               box (pitch 0, beam uniform over a 2W x 2W square, fluence
               N / (2W)^2, W = 5.5 P), both recorded and replayed end to end:
               the mean stage-2 deposit per unit fluence agrees within 3
               combined SE, for the whole lattice and for the lattice boxes
               inside the single box's field (the exact equivalent of the
               finite field); the share of the deposit outside it is printed.
  CHEMVARIANT  dissolved O2 0 vs 21 % on the same histories (chemistry on,
               1 us): identical Histories.csv (physics and pre-chemical
               columns), and G(e_aq) at 1 us lower with O2 by more than 3
               combined SE.

Python 3 standard library only. Prints one PASS/FAIL line per check; exit
status 0 only when every check selected passes.

  python scripts/acceptance.py --build <build dir> --out <output dir>
         [--threads N] [--checks energy,repeat,lattice,chemvariant]
"""

import argparse
import csv
import math
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

EXAMPLE_DIR = Path(__file__).resolve().parent.parent
RECORD_MACRO = EXAMPLE_DIR / "macros" / "record.mac"

# Stage-2 base seed of every acceptance replay.
BASE_SEED = 20261127
# Stage-1 seeds.
RECORD_SEED = 1
LATTICE_SEED = 5

# LATTICE geometry: the phantom, box, shell and pitch P of macros/record.mac,
# the box at 50 um depth. The single box gets a square beam of half side
# W = (SINGLE_FIELD_CELLS + 1/2) P, so that the lattice boxes with
# max(|ix|, |iy|) <= SINGLE_FIELD_CELLS see exactly the offsets the single box
# sees (translation invariance): the "restricted" comparison has no
# field-size bias, and the deposit of the other lattice boxes is the part of
# the broad-beam deposit that the finite field misses (README, "Acceptance").
LATTICE_DEPTH_UM = 50.0
LATTICE_PRIMARIES = 200000
SINGLE_FIELD_CELLS = 5              # W = 5.5 P = 275 um for P = 50 um
SINGLE_PRIMARIES = 10000000

# REPEAT end time (the chemistry runs, but only briefly).
REPEAT_END_TIME = "10 ns"


# ---- running the executables -------------------------------------------------

class Tools:
    def __init__(self, build: Path):
        exe = ".exe" if os.name == "nt" else ""
        self.record = build / f"phsp-record{exe}"
        self.replay = build / f"phsp-replay{exe}"
        self.dump = build / f"phsp-dump{exe}"
        for tool in (self.record, self.replay, self.dump):
            if not tool.is_file():
                sys.exit(f"acceptance: {tool} not found (build the example first)")
        self.env = dict(os.environ)
        geant4_bin = geant4_bin_dir(build)
        if geant4_bin is not None:
            self.env["PATH"] = str(geant4_bin) + os.pathsep + self.env.get("PATH", "")


def geant4_bin_dir(build: Path):
    """<install>/bin from Geant4_DIR in CMakeCache.txt (the PATH the ctests use)."""
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return None
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("Geant4_DIR:"):
            path = (Path(line.split("=", 1)[1].strip()) / ".." / ".." / ".." / "bin").resolve()
            return path if path.is_dir() else None
    return None


def run(tools: Tools, command, log: Path, done_tag: str) -> str:
    """Runs a command, writes its output to `log`, returns its *_DONE line."""
    log.parent.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    with open(log, "w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(str(c) for c in command) + "\n")
        out.flush()
        result = subprocess.run([str(c) for c in command], stdout=out, stderr=subprocess.STDOUT,
                                env=tools.env, cwd=str(log.parent))
    elapsed = time.monotonic() - start
    text = log.read_text(encoding="utf-8", errors="replace")
    match = re.search(done_tag + r"[^\r\n]*", text)
    if result.returncode != 0 or match is None:
        raise RuntimeError(f"{command[0].name} failed (exit {result.returncode}, {elapsed:.0f} s); log: {log}")
    print(f"    {log.parent.name}/{log.name}: {elapsed:.0f} s", flush=True)
    return match.group(0)


def done_value(line: str, key: str) -> str:
    match = re.search(r"(?:^| )" + re.escape(key) + r"=([^ ]+)", line)
    if match is None:
        raise RuntimeError(f"no {key}= in {line}")
    return match.group(1)


def record(tools: Tools, macro: Path, output: Path, seed: int, threads: int) -> str:
    output.parent.mkdir(parents=True, exist_ok=True)
    return run(tools, [tools.record, macro, "--output", output, "--threads", threads, "--seed", seed],
               output.parent / (output.name + ".log"), "RECORD_DONE")


def write_replay_macro(path: Path, files, extra_pre=(), extra_post=(), ranges=((0, None),)):
    """A phsp-replay macro: files, base seed, `extra_pre` before /run/initialize,
    `extra_post` after it, then one first/beamOn pair per (first, count)."""
    lines = ["/control/verbose 0", "/run/verbose 0", "/process/em/verbose 0"]
    lines += [f"/phsp/source/addFile {Path(f).as_posix()}" for f in files]
    lines.append(f"/phsp/source/baseSeed {BASE_SEED}")
    lines += list(extra_pre)
    lines.append("/run/initialize")
    lines += list(extra_post)
    for first, count in ranges:
        lines += [f"/phsp/source/first {first}", f"/run/beamOn {count}"]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def replay(tools: Tools, macro: Path, output: Path, threads: int) -> str:
    return run(tools, [tools.replay, macro, "--output", output, "--threads", threads],
               output.parent / (output.name + ".log"), "REPLAY_DONE")


# ---- reading the outputs -----------------------------------------------------

def dump_section(tools: Tools, phsp: Path, section: str):
    text = subprocess.run([str(tools.dump), str(phsp), "--section", section], capture_output=True, text=True,
                          env=tools.env, check=True).stdout
    return list(csv.DictReader(text.splitlines()))


def dump_header(tools: Tools, phsp: Path) -> dict:
    return {row["field"]: row["value"] for row in dump_section(tools, phsp, "header")}


def read_csv(path: Path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.reader(f))


def history_rows(output: Path):
    with open(output / "Histories.csv", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


# ---- statistics ----------------------------------------------------------------

def mean_se(values):
    n = len(values)
    mean = sum(values) / n
    if n < 2:
        return mean, float("nan")
    variance = sum((v - mean) ** 2 for v in values) / (n - 1)
    return mean, math.sqrt(variance / n)


def per_primary_mean_se(deposits_by_event, primaries: int):
    """Mean and SE of the deposit per primary over `primaries` primaries, of
    which only those in `deposits_by_event` deposited anything (the others are
    0). A primary entering several boxes counts once, with the sum."""
    total = sum(deposits_by_event.values())
    squares = sum(d * d for d in deposits_by_event.values())
    mean = total / primaries
    variance = (squares - primaries * mean * mean) / (primaries - 1)
    return mean, math.sqrt(max(variance, 0.0) / primaries)


def ratio_se(numerators, denominators):
    """Pooled ratio sum(c) / sum(E) and its SE (delta method over units)."""
    n = len(numerators)
    total_c, total_e = sum(numerators), sum(denominators)
    ratio = total_c / total_e
    residual = sum((c - ratio * e) ** 2 for c, e in zip(numerators, denominators))
    return ratio, math.sqrt(n / (n - 1) * residual) / total_e


def fmt(x, digits=4):
    return f"{x:.{digits}g}"


# ---- checks -----------------------------------------------------------------------

class Context:
    def __init__(self, tools: Tools, out: Path, threads: int):
        self.tools = tools
        self.out = out
        self.threads = threads
        self._default_file = None

    def default_file(self) -> Path:
        """The stage-1 file of macros/record.mac (seed RECORD_SEED), recorded once."""
        if self._default_file is None:
            path = self.out / "stage1" / "record.phsp"
            print(f"  stage 1: {RECORD_MACRO.name} --seed {RECORD_SEED} --threads {self.threads}", flush=True)
            done = record(self.tools, RECORD_MACRO, path, RECORD_SEED, self.threads)
            print("    " + " ".join(done.split()[1:13]), flush=True)
            self._default_file = path
        return self._default_file

    def histories(self, phsp: Path) -> int:
        return int(dump_header(self.tools, phsp)["historiesWritten"])


def check_energy(ctx: Context):
    phsp = ctx.default_file()
    n = ctx.histories(phsp)
    macro = write_replay_macro(ctx.out / "energy" / "energy.mac", [phsp], ["/phsp/chem/enable false"],
                               ranges=[(0, n)])
    done = replay(ctx.tools, macro, ctx.out / "energy" / "replay", ctx.threads)
    rows = history_rows(ctx.out / "energy" / "replay")
    stage1 = [float(r["stage1InBoxDepositEv"]) for r in rows]
    stage2 = [float(r["stage2DepositEv"]) for r in rows]
    m1, s1 = mean_se(stage1)
    m2, s2 = mean_se(stage2)
    md, sd = mean_se([b - a for a, b in zip(stage1, stage2)])
    z = md / sd
    killed = int(done_value(done, "tracksKilledOutsideBox"))
    ok = len(rows) == n and abs(z) < 3 and killed == 0
    return ok, (f"{n} histories; stage 1 {fmt(m1)} +/- {fmt(s1)} eV, stage 2 {fmt(m2)} +/- {fmt(s2)} eV per history; "
                f"paired difference {fmt(md)} +/- {fmt(sd)} eV, z = {z:.2f} (|z| < 3); "
                f"tracks killed outside the box {killed}")


def check_repeat(ctx: Context):
    phsp = ctx.default_file()
    n = ctx.histories(phsp)
    k = n // 2
    base = ctx.out / "repeat"
    post = [f"/scheduler/endTime {REPEAT_END_TIME}"]
    full = write_replay_macro(base / "full.mac", [phsp], extra_post=post, ranges=[(0, n)])
    split = write_replay_macro(base / "split.mac", [phsp], extra_post=post, ranges=[(0, k), (k, n - k)])
    second = write_replay_macro(base / "second.mac", [phsp], extra_post=post, ranges=[(k, n - k)])
    seeds = write_replay_macro(base / "seeds.mac", [phsp], ["/random/setSeeds 987654 321"], post, ranges=[(0, n)])
    runs = {
        "a (one run, 1 thread)": (full, 1, 0),
        "b (the same again)": (full, 1, 0),
        "c (split into two runs, one process)": (split, 1, 0),
        "g (second part alone, fresh process)": (second, 1, k),
        "d (2 threads)": (full, 2, 0),
        "f (other /random/setSeeds)": (seeds, ctx.threads, 0),
    }
    results = {}
    for name, (macro, threads, offset) in runs.items():
        tag = name.split()[0]
        replay(ctx.tools, macro, base / tag, threads)
        results[name] = (read_csv(base / tag / "Histories.csv"), offset)
    reference, _ = results["a (one run, 1 thread)"]
    header = reference[0]
    if "preChemEaq" not in header:
        return False, f"Histories.csv has no pre-chemical columns (chemistry off?): {header}"
    parts, ok = [], len(reference) == n + 1
    for name, (rows, offset) in results.items():
        if name.startswith("a "):
            continue
        expected = reference[offset + 1:]
        differing = sum(1 for x, y in zip(rows[1:], expected) if x != y)
        differing += abs(len(rows) - 1 - len(expected))
        if rows[0] != header:
            differing = len(expected)
        ok = ok and differing == 0
        parts.append(f"{name.split()[0]} {differing}/{len(expected)}")
    return ok, (f"{n} histories, chemistry on (end time {REPEAT_END_TIME}), all {len(header)} Histories.csv columns "
                f"(stage-2 deposit, pre-chemical counts) compared to run a; differing rows: " + ", ".join(parts) +
                "; Species.csv not compared (chemistry not bit-repeatable, documented)")


def lattice_pitch_um() -> float:
    """The lattice pitch of macros/record.mac (the default)."""
    text = RECORD_MACRO.read_text(encoding="utf-8")
    match = re.search(r"^/phsp/geom/latticePitch\s+([0-9.eE+-]+)\s+um", text, re.M)
    if match is None:
        raise RuntimeError(f"no '/phsp/geom/latticePitch <value> um' in {RECORD_MACRO}")
    return float(match.group(1))


def record_macro_value(name: str) -> str:
    match = re.search(r"^/phsp/geom/" + name + r"\s+(.+)$", RECORD_MACRO.read_text(encoding="utf-8"), re.M)
    if match is None:
        raise RuntimeError(f"no /phsp/geom/{name} in {RECORD_MACRO}")
    return match.group(1).strip()


def lattice_macro(path: Path, pitch_um: float, half_field_um: float, primaries: int) -> Path:
    phantom = record_macro_value("phantomSide")
    value, unit = phantom.split()
    phantom_mm = float(value) * {"mm": 1.0, "um": 1e-3, "cm": 10.0}[unit]
    lines = [
        "/control/verbose 0", "/run/verbose 0", "/process/em/verbose 0",
        f"/phsp/geom/phantomSide {phantom}",
        f"/phsp/geom/depth {LATTICE_DEPTH_UM} um",
        f"/phsp/geom/boxSide {record_macro_value('boxSide')}",
        f"/phsp/geom/shell {record_macro_value('shell')}",
        f"/phsp/geom/latticePitch {pitch_um} um",
        "/run/initialize",
        "/gps/particle e-", "/gps/ene/type Mono", "/gps/ene/mono 500 keV", "/gps/direction 0 0 1",
        "/gps/pos/type Plane", "/gps/pos/shape Square",
        f"/gps/pos/centre 0 0 {-phantom_mm / 2} mm",
        f"/gps/pos/halfx {half_field_um} um", f"/gps/pos/halfy {half_field_um} um",
        f"/run/beamOn {primaries}",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def fluence_normalised(rows, primaries: int, area_um2: float, column: str, keep=lambda row: True):
    by_event = {}
    for r in rows:
        if not keep(r):
            continue
        event = int(r["stage1EventId"])
        by_event[event] = by_event.get(event, 0.0) + float(r[column])
    mean, se = per_primary_mean_se(by_event, primaries)
    return mean * area_um2, se * area_um2   # eV per (primary / um^2) = eV um^2


def check_lattice(ctx: Context):
    pitch = lattice_pitch_um()
    half_field = (SINGLE_FIELD_CELLS + 0.5) * pitch
    base = ctx.out / "lattice"
    modes = {
        "lattice": (pitch, pitch / 2, LATTICE_PRIMARIES, pitch * pitch),
        "single": (0.0, half_field, SINGLE_PRIMARIES, (2 * half_field) ** 2),
    }
    rows, info = {}, {}
    for mode, (p, half, primaries, area) in modes.items():
        macro = lattice_macro(base / f"{mode}.mac", p, half, primaries)
        print(f"  stage 1 {mode}: pitch {p:g} um, beam half side {half:g} um, {primaries} primaries", flush=True)
        phsp = base / mode / "record.phsp"
        done = record(ctx.tools, macro, phsp, LATTICE_SEED, ctx.threads)
        n = ctx.histories(phsp)
        replay_macro = write_replay_macro(base / f"{mode}-replay.mac", [phsp], ["/phsp/chem/enable false"],
                                          ranges=[(0, n)])
        replay(ctx.tools, replay_macro, base / f"{mode}-replay", ctx.threads)
        rows[mode] = history_rows(base / f"{mode}-replay")
        info[mode] = (n, primaries, area, float(done_value(done, "hitFraction")),
                      float(done_value(done, "timePerPrimaryMs")))

    def inside(row):
        return max(abs(int(row["boxIx"])), abs(int(row["boxIy"]))) <= SINGLE_FIELD_CELLS

    def value(mode, column, keep=lambda row: True):
        n, primaries, area, _, _ = info[mode]
        return fluence_normalised(rows[mode], primaries, area, column, keep)

    def z(a, b):
        return (a[0] - b[0]) / math.hypot(a[1], b[1])

    results = {}
    for column in ("stage2DepositEv", "stage1InBoxDepositEv"):
        full, restricted, single = value("lattice", column), value("lattice", column, inside), value("single", column)
        results[column] = (full, restricted, single, z(full, single), z(restricted, single))
    full, restricted, single, z_full, z_restricted = results["stage2DepositEv"]
    _, _, _, z1_full, z1_restricted = results["stage1InBoxDepositEv"]
    missed = 1 - restricted[0] / full[0]
    ok = abs(z_full) < 3 and abs(z_restricted) < 3
    (nl, _, _, hl, tl), (ns, _, _, hs, ts) = info["lattice"], info["single"]
    return ok, (f"box at {LATTICE_DEPTH_UM:g} um, stage-2 deposit per unit fluence (eV um^2): lattice P = {pitch:g} um "
                f"(fluence N/P^2, {nl} histories) {fmt(full[0])} +/- {fmt(full[1])}; single box (pitch 0, fluence "
                f"N/(2W)^2, W = {half_field:g} um, {ns} histories) {fmt(single[0])} +/- {fmt(single[1])}; "
                f"z = {z_full:.2f} (|z| < 3); lattice restricted to the field (|ix|, |iy| <= {SINGLE_FIELD_CELLS}) "
                f"{fmt(restricted[0])} +/- {fmt(restricted[1])}, z = {z_restricted:.2f} (|z| < 3); share of the "
                f"broad-beam deposit outside the field {100 * missed:.1f} %; stage 1: z = {z1_full:.2f} (full), "
                f"{z1_restricted:.2f} (restricted); hit fraction {fmt(hl, 3)} vs {fmt(hs, 3)}, time per primary "
                f"{fmt(tl, 3)} vs {fmt(ts, 3)} ms")


def species_at(output: Path, species: str, time_ns: str):
    counts = {}
    for row in read_csv(output / "Species.csv")[1:]:
        if row[1] == time_ns and row[2] == species:
            counts[int(row[0])] = counts.get(int(row[0]), 0) + int(row[3])
    return counts


def check_chemvariant(ctx: Context):
    phsp = ctx.default_file()
    n = ctx.histories(phsp)
    base = ctx.out / "chemvariant"
    no_o2 = write_replay_macro(base / "no-o2.mac", [phsp], ranges=[(0, n)])
    o2 = write_replay_macro(base / "o2.mac", [phsp], ["/phsp/chem/o2 21 %"], ranges=[(0, n)])
    done_a = replay(ctx.tools, no_o2, base / "no-o2", ctx.threads)
    done_b = replay(ctx.tools, o2, base / "o2", ctx.threads)
    rows_a = read_csv(base / "no-o2" / "Histories.csv")
    rows_b = read_csv(base / "o2" / "Histories.csv")
    header = rows_a[0]
    differing = sum(1 for x, y in zip(rows_a[1:], rows_b[1:]) if x != y) + abs(len(rows_a) - len(rows_b))
    same_header = rows_b[0] == header and "preChemEaq" in header
    end_times = {done_value(d, "endTimeNs") for d in (done_a, done_b)}
    killed = [int(done_value(d, "moleculesKilledOutsideWater")) for d in (done_a, done_b)]

    index = header.index("globalIndex")
    deposit = header.index("stage2DepositEv")
    deposits = {int(r[index]): float(r[deposit]) for r in rows_a[1:]}
    keys = [g for g in sorted(deposits) if deposits[g] > 0]
    g_values = {}
    for name in ("no-o2", "o2"):
        counts = species_at(base / name, "e_aq^-1", "1000")
        g, se = ratio_se([counts.get(g, 0) for g in keys], [deposits[g] for g in keys])
        g_values[name] = (100 * g, 100 * se)
    (g0, s0), (g1, s1) = g_values["no-o2"], g_values["o2"]
    z = (g0 - g1) / math.hypot(s0, s1)
    ok = (same_header and differing == 0 and len(rows_a) == n + 1 and end_times == {"1000"} and z > 3
          and killed == [0, 0])
    return ok, (f"{n} histories, chemistry on, 1 us: Histories.csv (all {len(header)} columns: stage-2 deposit, "
                f"pre-chemical counts) differing rows O2 0 vs 21 %: {differing}; G(e_aq) at 1 us "
                f"{fmt(g0)} +/- {fmt(s0)} without O2, {fmt(g1)} +/- {fmt(s1)} with 21 % O2 per 100 eV "
                f"(difference {z:.1f} combined SE, > 3); molecules killed outside water {killed[0]}, {killed[1]}")


CHECKS = {"energy": ("ENERGY", check_energy), "repeat": ("REPEAT", check_repeat),
          "lattice": ("LATTICE", check_lattice), "chemvariant": ("CHEMVARIANT", check_chemvariant)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, type=Path, help="build directory with phsp-record/replay/dump")
    parser.add_argument("--out", required=True, type=Path, help="output directory (replaced)")
    parser.add_argument("--threads", type=int, default=max(1, min(18, (os.cpu_count() or 2) - 2)))
    parser.add_argument("--checks", default="energy,repeat,lattice,chemvariant")
    args = parser.parse_args()

    selected = [c.strip().lower() for c in args.checks.split(",") if c.strip()]
    for c in selected:
        if c not in CHECKS:
            parser.error(f"unknown check '{c}' (known: {', '.join(CHECKS)})")
    tools = Tools(args.build.resolve())
    out = args.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    ctx = Context(tools, out, args.threads)

    start = time.monotonic()
    lines, all_ok = [], True
    for c in selected:
        label, function = CHECKS[c]
        print(f"[{label}]", flush=True)
        try:
            ok, text = function(ctx)
        except Exception as error:  # a failed run is a FAIL with its cause
            ok, text = False, f"error: {error}"
        line = f"{label} {'PASS' if ok else 'FAIL'} {text}"
        print(line, flush=True)
        lines.append(line)
        all_ok = all_ok and ok
    summary = f"total {time.monotonic() - start:.0f} s, {args.threads} threads"
    (out / "acceptance.txt").write_text("\n".join(lines + [summary]) + "\n", encoding="utf-8")
    print("\n" + "\n".join(lines) + "\n" + summary)
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
