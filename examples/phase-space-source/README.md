# phase-space-source

Standalone Geant4-DNA example: simulate the physical stage of an electron beam
in a water phantom once, save the particles that enter a small scoring box at
depth in a reusable **Phase-space file**, and replay that file in several
Geant4-DNA physics + chemistry runs of the box. The same histories are
replayed in the same order with the same physics in every run, so chemistry
variants can be compared history by history and the physical stage of a run
can be repeated exactly (the chemistry stage only statistically, see
"Limits").

Purpose: a 500 keV e- has a CSDA range of about 1.8 mm in water, so
Geant4-DNA physics over the whole phantom is slow, and a 4 um box at 1 mm
depth under a beam of its own size gets almost no primaries. Stage 1 runs the
phantom once with condensed-history physics outside thin DNA shells around
the boxes; stage 2 then runs only the box, as often as needed.

Not to be confused with `examples/dose-rate-study`, which captures
**Track packets** (chemistry species clouds at 1 ps) and replays species.
This example records and replays **particles** (a phase space), so stage 2
redoes the physical, pre-chemical and chemical stages in the box.

Design records: `docs/adr/0014-phase-space-crossing-rule.md` (every inward
crossing is recorded, stage 1 kills nothing, the Scoring lattice) and
`docs/adr/0015-per-history-seeding.md` (stage 2 reseeds per history). Glossary:
Phase-space file, Phase-space history, Inward crossing, Scoring lattice in
`CONTEXT.md`.

The example copies the code it needs (`DataNode`/`JsonWriter` from PFLASH
`src/scoring/`, `PureWaterReactions` and `ChemistryTypes` from PFLASH
`src/chemistry/`) and links nothing from PFLASH `sim` or the other examples;
a change there does not reach this example.

Status: complete (tickets 01 to 05). The acceptance results are in
"Acceptance", the defaults and their measurements in "Defaults: shell and
pitch", and what the example does not do in "Limits".

## Workflow

Two executables and a dump tool, built in `build/` (see "Build and test"),
run from the example directory:

```
# Stage 1: 500 keV e- into the phantom; writes phsp/record.phsp + .json
build/phsp-record macros/record.mac --threads 4 --seed 1 --output phsp/record
# Inspect the file
build/phsp-dump phsp/record.phsp --section header
# Stage 2: replay histories 0..99 with physics + chemistry; writes replay/
build/phsp-replay macros/replay.mac --threads 4 --output replay
# The four acceptance checks (about 13 min on 18 threads)
python scripts/acceptance.py --build build --out <dir>
```

1. `phsp-record` (stage 1) fires the beam (`/gps/...`) into a water phantom
   and writes every Inward crossing of a scoring box into one Phase-space
   file. Each box sits in its own thin shell of Geant4-DNA physics; the rest
   of the phantom is condensed history.
2. `phsp-replay` (stage 2) reads one or more files and replays a range of
   their histories, one history per event, in a water box of the same side
   with Geant4-DNA physics, then (by default) an IRT_syn chemistry stage. A
   chemistry variant (pH, O2, end time) is another `phsp-replay` run of the
   same range.
3. Outputs: stage 1 writes `<name>.phsp` (binary, below) and `<name>.json`
   (sidecar); stage 2 writes `Histories.csv` (one row per history: deposits,
   pre-chemical counts), `Species.csv` (species counts and G-values on a time
   grid) and `Manifest.json` (files, seeds, ranges, settings).

## Crossing rule and lattice equivalence (ADR 0014)

- Every Inward crossing of any track into a scoring box is one record, and
  stage 1 kills nothing. A track that leaves the box and comes back gives a
  second record, and the secondaries of an excursion outside the box are
  recorded when they enter. Stage 2 kills every particle when it leaves the
  box. So every in-box track segment of stage 1 is replayed or produced
  again in stage 2, and the mean in-box deposit is the same (the ENERGY
  check). Track by track it is not: a recorded re-entry of a secondary born
  in the box is not correlated with the secondary that stage 2 draws itself.
- Because nothing is killed, the boxes do not shadow each other, and a broad
  uniform beam can be replaced by a beam uniform over one cell of a lateral
  Scoring lattice of pitch P, with one box per cell: the fluence is
  N_primaries / P^2. This holds for a laterally uniform broad beam and a
  phantom wider than the lateral reach of the radiation; pitch 0 is a single
  box on the axis under any GPS field (fluence = N_primaries / field area for
  a uniform field). The LATTICE check compares the two.
- A history is the records of one primary in one box; a primary entering two
  boxes gives two histories, so stage 2 never superposes two boxes' tracks.

## Core library (`phspCore`, Geant4-free)

Standard library only, so its tests run without a Geant4 kernel.

- `PhaseSpaceFormat.hh`: the binary format v1 (layout below), its
  little-endian encoders and decoders, validation, FNV-1a 64 and splitmix64.
- `PhaseSpaceWriter.hh`: `WritePhaseSpaceFile` writes `<name>.phsp` and the
  `<name>.json` sidecar from histories sorted by stage-1 event ID.
- `PhaseSpaceReader.hh`: opens and fully validates one file, then reads one
  history's records on demand. Safe to share between threads.
- `HistoryCatalog.hh`: global history numbering over an ordered list of files,
  and range selection.
- `HistorySeed.hh`: the per-history seed of stage 2.
- `LatticeFold.hh`: lattice box index and box-frame position.

Errors are `std::runtime_error` with a message naming the file and the
problem; the Geant4 executables turn them into `G4Exception`.

## Phase-space file, format v1

`<name>.phsp` holds three blocks. Every field is little-endian with a fixed
width and is encoded field by field, not by copying a struct. Units are eV,
nm and ns.

| Block | Size | Content |
|---|---|---|
| File header | 128 bytes | magic `PFPHSP\r\n`, format version, block sizes, file ID, content digest, stage-1 seed, primaries fired, histories written, records written, lattice pitch, box side, depth, header checksum |
| History table | 48 bytes per history | history index, stage-1 event ID, first record, record count, in-box deposit (eV), box ix, box iy |
| Records | 80 bytes per record | PDG, track ID, kinetic energy, position (box frame), direction, time, weight |

The byte offsets are listed in `include/PhaseSpaceFormat.hh`.

- One record is one Inward crossing, in the frame of the box it entered:
  origin at the box centre, the phantom's axes.
- One history holds all records of one stage-1 primary in one box of the
  Scoring lattice, and carries that box's lattice indices (ix, iy), two
  signed 32-bit integers; the box is centred at (ix P, iy P, depth). A
  primary whose tracks enter several boxes gives one history per box, so a
  history never mixes the frames of two boxes, and stage 2, which replays one
  history per event, never superposes crossings of distinct boxes in one box.
  With pitch 0 the box is always (0, 0).
- Histories are written in strictly increasing order of (stage-1 event ID,
  box iy, box ix) (`HistoryOrderKey`; row-major lattice order) and indexed
  0..N-1. A primary with no record is not written; it counts only in
  `primariesFired`.
- `historiesWritten <= primariesFired` holds, and is checked, for pitch 0
  only. On a lattice one primary may give several histories, so the header
  check is not applied.
- `contentDigest` is FNV-1a 64 over the history table and the records, in
  file order. The header has its own checksum (FNV-1a 64 over bytes 0..95),
  because the digest does not cover it.
- `fileId = SplitMix64(stage1Seed ^ SplitMix64(contentDigest))`. For the same
  content, distinct seeds always give distinct IDs. For different contents a
  clash has probability about 2^-64. The same file written twice keeps its ID,
  so a copy is detected as a duplicate.
- The lattice pitch is 0 (single box) or at least the box side, so boxes do
  not overlap. (`phsp-record` requires more: P > box side + 2 x shell.)

The reader refuses a file with a wrong magic, an unsupported version, other
block sizes, a header checksum mismatch, a size that does not match the
header counts (truncated or with trailing bytes), a content digest mismatch,
an inconsistent history table (including entries out of (event ID, box iy,
box ix) order, or a box other than (0, 0) with pitch 0), or an invalid
record. Each refusal has a clear
message and none of them crashes.

The `.json` sidecar is informational: it repeats the header (IDs as
`"0x..."` strings), adds the total in-box deposit and the file size, and
holds what stage 1 passes under `"stage1"` (beam, physics). The reader uses
the binary file only.

## History numbering and ranges

Stage 2 opens an ordered list of files. The global index runs over file
order, then history index. With files A (2 histories), B (0) and C (3), A's
histories are 0-1 and C's are 2-4. The same list in the same order always
gives the same numbering. `Select(first, count)` past the end is an error, and
so is a file ID given twice (the same file listed twice, or a copy of it).

## Per-history seed (ADR 0015)

```
HistorySeed(b, f, i) = S(S(S(b) ^ f) ^ i)        S = splitmix64
```

Here `b` is the stage-2 base seed, `f` the file ID and `i` the history index.
`HistoryEngineSeeds` returns `{bits 32..62, bits 0..30, 0}` (0 replaced by 1),
for `G4Random::setTheSeeds`. MixMax (the Geant4 default) and Ranecu read the
first two values. MixMax keeps the pointer it was given, so the array must
outlive the call. The mix is fixed: `HistorySeedTest` pins its values,
because changing it changes every stage-2 result.

## Lattice fold

With pitch P > 0, box (ix, iy) is centred at (ix P, iy P, zc) and owns the
lateral cell [(ix - 1/2) P, (ix + 1/2) P) x [(iy - 1/2) P, (iy + 1/2) P). A
position on a cell edge belongs to the box on the + side. The box-frame
position is (x - ix P, y - iy P, z - zc), with x and y in [-P/2, P/2). P = 0
is the single box (0, 0).

## phsp-dump

```
phsp-dump <file.phsp> [--section all|header|histories|records]
```

`phsp-dump` validates the file, then prints the header (`field,value`), the
history table and every record as CSV. The history and record tables both
carry `historyIndex,stage1EventId,boxIx,boxIy` first. With `all` (the default), each table
follows a `# <name>` line and a blank line separates them. One section prints
that table alone, ready for a CSV reader. Reals print in the shortest form
that reads back to the same double. Exit status: 0 on success, 1 on a file
error, 2 on a usage error.

## phsp-record (stage 1)

```
phsp-record <macro> --output <dir>/<name> [--threads N] [--seed S]
```

`phsp-record` fires the beam of the macro into a water phantom and writes
every Inward crossing of a scoring box into `<dir>/<name>.phsp` and
`<dir>/<name>.json` (ADR 0014). `<dir>` is created if it is missing, and
existing files of that name are replaced.

- The macro is applied line by line. `/phsp/geom/...` comes before
  `/run/initialize`, then `/gps/...`, then exactly one `/run/beamOn N`.
  `/random/...` commands are refused, because the seed is `--seed` (an
  unsigned 64-bit integer, default 1) and is written in the file header. So
  is every way of running a command that this line-by-line check would not
  see: `/control/execute`, `/control/loop`, `/control/foreach`,
  `/control/macroPath`, `/control/alias` (and `{...}` alias uses),
  `/control/shell`, and a macro argument to `/run/beamOn` (exit status 3,
  `RECORD_ERROR <macro>:<line>: ...`).
- `--threads N` (1..256, default 1) always uses the MT run manager. The master
  draws the seeds of every event in event order, so the file does not depend
  on N.
- On success the last line is `RECORD_DONE primaries=..
  primariesWithRecords=.. histories=.. records=.. meanRecordsPerHistory=..
  hitFraction=.. historiesPerHitPrimary=.. meanInBoxDepositPerHistoryEv=..
  meanInBoxDepositPerPrimaryEv=.. threads=.. wallTimeS=.. timePerPrimaryMs=..
  fileId=.. file=..`. `hitFraction` is primariesWithRecords / primaries, and
  `historiesPerHitPrimary` is histories / primariesWithRecords (above 1 when
  a primary enters several boxes). `timePerPrimaryMs` is thread time (wall
  time x threads / primaries) and includes each worker's start-up.
- Exit status: 0 on success, 2 on bad arguments, 3 on a macro or
  initialization failure, 1 on a failure during the run or the write; a
  failure prints a `RECORD_ERROR` line on stderr. A `G4Exception`
  FatalException aborts.

The default macro is `macros/record.mac`: 500 keV e- along +z from the -z
face of a 5 mm phantom, uniform over one 50 um x 50 um lattice cell,
3 x 10^4 primaries: 311 histories with `--seed 1` (for any thread count),
about 90 thread-seconds (5 s of run time on 18 threads). `macros/replay.mac`
replays the first 100 of them.

### Geometry

| Command (PreInit) | Default | Meaning |
|---|---|---|
| `/phsp/geom/phantomSide` | 5 mm | side of the G4_WATER phantom cube, centred at the origin; the beam enters through its -z face |
| `/phsp/geom/depth` | 1 mm | depth of the box centres below the -z face |
| `/phsp/geom/boxSide` | 4 um | side of a cubic scoring box |
| `/phsp/geom/shell` | 1 um | DNA water on each side of the box (see "Defaults: shell and pitch") |
| `/phsp/geom/latticePitch` | 50 um | 0 = one box on the axis, else P > boxSide + 2 x shell (see "Defaults: shell and pitch") |

The world is G4_Galactic, 0.1 mm wider than the phantom on every side. Each
scoring box sits at the centre of its own DNA shell: a G4_WATER cube of side
boxSide + 2 x shell, a daughter of the phantom; the shells form the region
`DnaRegion`, and the box is the shell's only daughter. The water between the
shells is condensed history. For P > 0 the shells sit at (ix P, iy P) at the
box depth, for |ix|, |iy| <= n with
n = floor((phantomSide - boxSide - 2 x shell) / (2 P)), so every shell is
inside the phantom (99 x 99 by default). For P = 0 there is one shell on the
axis. The copy number of a shell is the box index
(iy + n)(2n + 1) + (ix + n); the box index of a point in a box is the copy
number of its mother shell.

`phsp-record` checks the five values together before `/run/initialize`, and
exits with status 3 for a pitch in (0, boxSide + 2 x shell], a shell that
does not fit in the phantom, or more than 10^6 boxes. With such a pitch
neighbouring shells would overlap or share a face (with shell 0, neighbouring
boxes would share a face and a crossing point on it would belong to two
boxes). CTest checks pitches 4, 5 and 6 um with the default 4 um box and
1 um shell (`RecordRejectsTouchingBoxes`, `RecordRejectsPitch5um`,
`RecordRejectsPitch6um`).

### Physics

`G4EmStandardPhysics_option4` everywhere, plus DNA_Opt2 in `DnaRegion`,
activated with `G4EmParameters::AddDNA("DnaRegion", "DNA_Opt2")` and the
`G4EmDNAPhysicsActivator` constructor, as in the Geant4 example
`dna/microdosimetry`. Checked in the Geant4 11.4.3 source:

- `G4EmDNAPhysicsActivator::ConstructProcess` uses `emaxDNA = 1 MeV` for e-.
  In the region, the standard e- msc, ionisation and bremsstrahlung models
  are active only above 1 MeV. Below 1 MeV,
  `G4EmDNABuilder::ConstructDNAElectronPhysics` (option 2) installs Champion
  elastic (7.4 eV to 1 MeV), Born excitation and ionisation (up to 1 MeV),
  Sanche vibrational excitation, Melton attachment and solvation (below
  7.4 eV). So a 500 keV e- and all its secondaries are on DNA models in the
  region. Gammas keep the option4 models everywhere. The activator also turns
  fluorescence on.
- The constructor of `G4EmStandardPhysics_option4` calls
  `G4EmParameters::SetDefaults`, which clears the DNA region list, so
  `AddDNA` is called after it.
- Known warning: `G4Exception Run0111 ... Duplication of processes`. The
  activator registers `nuclearStopping` for `GenericIon` a second time, and
  Geant4 does not add it again. It does not affect e- or gamma.

Production cuts: 1 um outside the region (990 eV for e- and gamma in water).
The region takes the cuts of the microdosimetry example's DNA region: 0.7 mm
for gamma, e- and e+, and 1 um for protons. The DNA models ignore cuts, so
the region cuts only matter for standard models above 1 MeV. Both can be
changed with `/run/setCut` and `/run/setCutForRegion DnaRegion`; the sidecar
records the cuts used.

### Recording

- One record is one Inward crossing. A step whose pre-step point has status
  `fGeomBoundary` and lies in a scoring box has just entered that box: a box
  has no daughters and is the only daughter of its shell, so the track came
  from the shell. This is the pattern of the Geant4 example `dna/phasespace`
  (`SteppingAction.cc`). The record is that pre-step point: PDG code, track
  ID, kinetic energy, position, direction, global time and weight. Any
  particle type is recorded, nothing is killed, and one track may give
  several records.
- The position is moved into the frame of the box entered by subtracting the
  translation of the touched box (`G4VTouchable::GetTranslation`), not by
  folding the global position, which is ambiguous on a face. Each record
  checks that this translation is the lattice centre of the box index (the
  shell's copy number) and that the point lies on the box; a failed check is
  a FatalException.
- One history per (primary, box): the records and the in-box deposit (energy
  deposited in steps whose pre-step point is in the box) are kept per box,
  and each box that has at least one record gives its own history, carrying
  (ix, iy). One primary entering two boxes gives two histories.
- A primary with no record is not written; it counts in `primariesFired`.
  An (event, box) pair that deposits energy in a box without any record
  (possible only when the beam starts inside a box) is reported by a
  `RECORD_WARNING` line and in the sidecar.
- MT: each worker fills its own `RecordRun`. At the end of the run,
  `G4WorkerRunManager` merges each worker run into the master run under the
  run manager's merge mutex (`RecordRun::Merge`, which moves the histories).
  The master's `RecordRunAction` then sorts them by (stage-1 event ID, box
  iy, box ix). Event IDs and event seeds come from the master in event order,
  so the sorted list is the same for any thread count.

The sidecar's `"stage1"` object holds the macro commands, the Geant4
version, the engine and its master seeds, the geometry, the physics and cuts,
and the recording rule. It holds nothing that depends on the thread count or
on timing, so for the same seed the `.json` is byte-identical between thread
counts, like the `.phsp`. The thread count and the times are only printed.

### Defaults: shell and pitch

The defaults (1 um shell, 50 um pitch) were chosen from the measurements
below: stage 1 only, the default geometry (5 mm phantom, 4 um box at 1 mm
depth, 500 keV e-, beam uniform over one cell), `--seed 1`, 18 threads, stock
Geant4 11.4.3 on a 20-core Windows machine, `DNAFast` off. Times are thread
time of the run (wall time x threads; the start-up before the run is not
included), errors one standard error. Script:
`.scratch/tests/2026-11-26__phase-space-source/05-measure-defaults.py`,
results in `05-defaults/results.jsonl`.

**Shell.** Outside `DnaRegion`, the condensed history produces delta rays
only above the e- production threshold. With the default 1 um cut that
threshold is 990 eV in water (the lower edge of the cut table, from the
cut-table printout), and a 990 eV electron has a range in water of order
0.05 um. So a delta ray that the condensed history does not produce could
not reach the box from beyond even a 0.1 um shell. The shell also makes the
low-energy electrons that the condensed history does produce (a few keV,
ranges of order 1 um) reach the box on DNA models. Pitch 20 um, shell
varied:

| shell | histories (2 x 10^4 primaries) | records per history | deposit per unit fluence (eV um^2) | ms per primary | ms per history |
|---|---|---|---|---|---|
| 0.25 um | 1284 | 1.18 | 21900 +/- 1100 | 8.8 | 138 |
| 0.5 um | 1260 | 1.16 | 22900 +/- 1300 | 10.1 | 160 |
| 1 um | 1294 | 1.21 | 23400 +/- 1300 | 13.9 | 216 |
| 2 um | 1245 | 1.24 | 22400 +/- 1300 | 27.2 | 437 |

The deposit does not depend on the shell at this 5 % precision. 1 um is
kept as the default: it is about 20 times the range at the production
threshold, and it puts the last micrometre of every electron of a few keV
on DNA models, for 1.6 times the stage-1 cost of a 0.25 um shell. A thinner
shell is an open option (see "Limits").

**Pitch.** The cost of a history is the time per primary divided by the
histories per primary. The DNA cost grows with the share of the phantom
area covered by shells, (6 um / P)^2, and the hits with the box share,
about 1.5 (4 um / P)^2 (side faces are crossed too), so it levels off at
small P, where one primary enters several boxes more often. Shell 1 um:

| pitch | primaries | histories | hit fraction | hit primaries in 2+ boxes | deposit per unit fluence (eV um^2) | ms per primary | ms per history |
|---|---|---|---|---|---|---|---|
| 10 um | 4 x 10^3 | 980 | 0.196 | 16 % | 24200 +/- 1600 | 69.6 | 284 |
| 20 um | 2 x 10^4 | 1294 | 0.0608 | 5.4 % | 23400 +/- 1300 | 13.9 | 216 |
| 50 um | 10^5 | 1044 | 0.0103 | 0.9 % | 22400 +/- 1300 | 2.59 | 248 |
| 100 um | 3 x 10^5 | 790 | 0.00263 | 0.1 % | 25500 +/- 1800 | 0.99 | 376 |

The deposit per unit fluence (per primary x P^2) agrees between the
pitches, as the lattice equivalence expects. 50 um costs 15 % more per
history than 20 um, but under 1 % of its hit primaries give two correlated
histories (5.4 % at 20 um), and it has 99 x 99 boxes instead of 249 x 249.
A 10^4-history file at the default geometry then takes about 10^6
primaries, 0.7 thread-hours. (Ticket 02 measured P = 100 um with
2 x 10^6 primaries: hit fraction 0.00260 +/- 0.00004, 0.96 ms per primary.)

The stage-2 cost per history (47 ms with chemistry off on 1 thread, see
"Stage-2 physics") does not depend on these choices.

Lateral reach. On the lattice, box (ix, iy) sits at the offset (ix P, iy P)
from the beam cell, so the deposit per box ring shows how far sideways the
radiation that reaches a box comes from. At 50 um depth (P = 50 um,
2 x 10^5 primaries, 1901 histories, 5.6 ms per primary), the boxes with
max(|ix|, |iy|) <= k collect, of the whole deposit: 84 % for k = 0, 88 % for
k = 1, 90 % for k = 5, 93 % for k = 10 and 99.5 % for k = 20. The tail is
electrons backscattered from deeper in the phantom; it reaches about 1 mm.
A single box under a square beam of half side W = (k + 1/2) P sees exactly
these offsets, so it misses the rest of the broad-beam deposit: about 10 %
for W = 275 um (`05-defaults/lateral-p50.txt`). The LATTICE acceptance check
uses this.

## phsp-replay (stage 2)

```
phsp-replay <macro> --output <dir> [--threads N] [--physics stage1|option2]
```

`phsp-replay` replays Phase-space histories in a water box of the stage-1
box side, one history per event, and writes `<dir>/Histories.csv`,
`<dir>/Species.csv` (chemistry on only) and `<dir>/Manifest.json` (`<dir>`
is created if it is missing, and existing files are replaced). With the
chemistry on (the default), each history runs an IRT_syn chemistry stage
after its physics (see "Chemistry").

- The macro is applied line by line, like `phsp-record`'s. Before
  `/run/initialize` come the `/phsp/source/addFile` commands (one or more, in
  order), `/phsp/source/baseSeed` and the `/phsp/chem/` settings. After it
  come `/scheduler/endTime` (optional) and any number of
  `/phsp/source/first k` + `/run/beamOn N` pairs, each replaying global
  histories k..k+N-1. The default macro is `macros/replay.mac`.
- `--threads N` (1..256, default 1) always uses the MT run manager.
  `--physics` selects the stage-2 physics (below; default `stage1`).
- On success the last line is `REPLAY_DONE histories=.. stage1MeanEv=..
  stage1SeEv=.. stage2MeanEv=.. stage2SeEv=.. diffMeanEv=.. diffSeEv=.. z=..
  tracksKilledOutsideBox=.. physics=.. chemistry=on|off [endTimeNs=..
  preChemOutsideBox=.. moleculesKilledOutsideWater=..] threads=..
  wallTimeS=.. output=..`
  over every history of the process, where diff = stage 2 - stage 1 per
  history and z = diffMean / diffSe.
- Exit status: 0 on success, 2 on bad arguments, 3 on a macro or
  initialization failure, 1 on a failure during the run or the write (a
  `REPLAY_ERROR` line on stderr). A `G4Exception` FatalException aborts.

| Command | State | Meaning |
|---|---|---|
| `/phsp/source/addFile <path>` | PreInit | append a `.phsp` file to the ordered list (path relative to the working directory) |
| `/phsp/source/baseSeed <s>` | PreInit | stage-2 base seed, unsigned 64-bit, default 1 |
| `/phsp/source/first <k>` | PreInit, Idle | global index of the first history of the next `/run/beamOn`, default 0 |
| `/phsp/geom/boxSide <value> <unit>` | PreInit | optional: the expected box side; it must equal the files' |
| `/phsp/geom/chemMargin <value> <unit>` | PreInit | water margin around the box in which the molecules diffuse, >= 0, default 2 um (see "Geometry") |
| `/phsp/chem/enable <bool>` | PreInit | chemistry on (default) or off (physics only, the ticket-03 output) |
| `/phsp/chem/pH <value>` | PreInit | pH of the acid-base buffer, in (0, 14), default 7 |
| `/phsp/chem/o2 <value> <unit>` | PreInit | dissolved O2, unit `M`, `mM`, `uM` or `%` of O2 saturation (kH = 1.3 mM, so 21 % = 0.273 mM); default 0 (absent) |
| `/scheduler/endTime <value> <unit>` | Idle | chemistry end time (Geant4 command, after `/run/initialize`), default 1 us |

These are FatalExceptions, each raised before any event runs:

- at `/run/initialize`: no file, a file that does not open or validate, the
  same file ID twice (the same file listed twice, or a copy of it), files
  with different box sides, or a `/phsp/geom/boxSide` other than the files'
  (relative tolerance 10^-9, because the header stores nm);
- at `/run/beamOn N`: a range past the end, or a range that overlaps one
  already replayed in this process. `Histories.csv` holds one row per
  history, so to replay a history twice, run another process;
- at the command: a `/phsp/chem/pH` outside (0, 14), a negative
  `/phsp/chem/o2` or an unknown unit;
- just before `/run/initialize`, with the chemistry on: a
  `/process/chem/TimeStepModel` other than IRT_syn;
- at the first real `/run/beamOn` (the `BeamOn(0)` inside `/run/initialize`
  is a fake run, which returns before `BeginOfRunAction`), still before
  any event: the chemistry is on but was not built, because
  `/run/initialize` came from a nested macro (`/control/execute`).
  `main()` adds the chemistry just before a `/run/initialize` line of the
  top-level macro.

The files need the same box side but nothing else: the lattice pitch, the
depth and the stage-1 physics may differ. The reader uses the binary file
only, so the stage-1 physics in the `.json` sidecar is not checked. Keep
the stage-2 physics option consistent with the files yourself.

### Geometry

The box is a G4_WATER cube of the files' box side, centred at the origin and
not rotated. This is the stage-1 box frame, so a record's position is used
as it is. The box is the region `DnaRegion`, with the stage-1 region cuts.

Around it is a G4_WATER margin, `ChemMargin`, of thickness
`/phsp/geom/chemMargin` (default 2 um; 0 builds no margin). It is in the
world region, not in `DnaRegion`. It is water for the molecules that diffuse
out of the box: `G4DNABrownianTransportation` kills a molecule whose step
starts outside water (`G4DNABrownianTransportation.cc`, `Diffusion`,
lines 494-521). No physics track ever steps in the margin:

- every track is killed at the end of the step that leaves the box;
- `ReplayTrackingAction` kills a track that starts outside the box (it is
  located before `PreUserTrackingAction`) before its first step;
- either case is counted in `tracksKilledOutsideBox`; there have been none.

The world is a G4_Galactic cube 1 um wider than the margin on every side.
The box needs a world around it, because region-specific DNA models cannot
be attached to the world region.

With the chemistry off, the margin changes nothing: `Histories.csv` is
byte-identical to the ticket-03 output, which had no margin (see
"Chemistry").

### Source and seeding (ADR 0015)

- Event e of a run replays global history first + e. The master assigns
  event IDs in order, so this does not depend on the thread count.
- `GeneratePrimaries` reseeds the engine with `G4Random::setTheSeeds` from
  `HistoryEngineSeeds(baseSeed, fileId, historyIndex)`. It runs after
  `G4WorkerRunManager::GenerateEvent` has seeded the event from the master's
  list (checked in the 11.4.3 source), so the per-history seed replaces it.
  The seed array is a member of the generator, because MixMax keeps the
  pointer it was given. Nothing draws a random number between
  `GeneratePrimaries` and tracking.
- Every record of the history becomes one primary vertex: its box-frame
  position, its time minus the earliest record time of the history (the
  history starts at t = 0), and one primary with the record's PDG code
  (particle table, then ion table), kinetic energy, direction and weight.
- The physical stage of a history therefore depends only on the base seed,
  the file ID and the history index. `/random/` commands have no effect, and
  the manifest records the base seed and the file IDs. `ReplayRepeat` checks
  this (below). The chemistry stage is not bit-reproducible from one process
  to the next (see "Chemistry reproducibility").
- Without the reseed, a repeat, a split into runs in one process, and
  another thread count would still give the same rows. `G4MTRunManager`
  draws the per-event seeds from the master engine in event-ID order, and a
  second run continues the master's seed stream. Only two cases need the
  per-history reseed: another `/random/setSeeds`, and a range started in a
  fresh process at k > 0 (history k would get the seeds of event 0).
  `ReplayRepeat` tests both.

### Scoring and output

The stage-2 deposit of a history is the energy deposited in steps whose
pre-step point is in the box, which is how stage 1 defines its in-box
deposit (`RecordSteppingAction`). The two agree in the mean over the same
histories (ADR 0014). A track that leaves the box and comes back gave a
record of its own in stage 1, which stage 2 replays as a primary. A
secondary born in the box is produced in stage 2 too. So every in-box track
segment of stage 1 is either replayed or produced again. Deposits are not
weighted: stage 1 does not weight them, and every stage-1 record has
weight 1.

Each worker adds one row per event to a mutex-protected store. `main()`
sorts the rows by global index and writes `Histories.csv`:

```
globalIndex,fileId,historyIndex,stage1EventId,boxIx,boxIy,recordCount,stage1InBoxDepositEv,stage2DepositEv,tracksKilledOutsideBox
```

With the chemistry on, five columns follow:
`preChemIonisedH2O,preChemExcitedH2O,preChemAttachedH2O,preChemEaq,chemistryEndTimeNs`
(see "Chemistry"). With it off the file is the ticket-03 file, byte for byte.

Reals are printed in the shortest form that reads back to the same double.
The file holds no thread count or timing, so for the same files, base seed
and histories it is byte-identical for any thread count, range split or
chemistry setting (pH, O2, end time; the end-time column excepted).

`Manifest.json` holds the Geant4 version, the run manager, the thread count,
the engine, the base seed, the macro commands, and the files: position,
path, file ID, content digest, stage-1 seed, counts, first global index, box
side, pitch and depth. It also holds the ranges replayed, the geometry, the
physics option with its cuts and the EM flags that differ between the
options (`dnaFast`, `fluorescence`, `auger`, `deexcitationIgnoreCut`), the
number of histories, `tracksKilledOutsideBox`, and the wall time. The
`chemistry` object holds `enabled` and, when on, the network, the
time-step model, `pH`, `o2` (as given) and `o2_M`, the bulk reservoir,
`endTime_ns` (the master's) and `endTimesUsed_ns` (the values the workers
used, from `Histories.csv`), the species time grid, the counter precision,
`sancheExtendedTo0025eV`, `preChemicalOutsideBox`,
`moleculesKilledOutsideWater`, a `diffusion` object (fastest species, its
diffusion coefficient, sqrt(6 D t_end), the margin) and a reproducibility
note. `geometry` records `chemMargin_nm`.

### Chemistry

With `/phsp/chem/enable true` (the default), every history runs one
chemistry stage after its physics, with all its records' molecules (they
are primaries of the same event). `ReplayChemistryList` builds it like
PFLASH's `DnaChemistryList` and `examples/dose-rate-study`'s
`PureWaterChemistryList`:

- The PureWater network, copied from PFLASH: `src/PureWaterReactions.cc`,
  `include/PureWaterReactions.hh` and `include/ChemistryTypes.hh`, with the
  include paths adjusted (the same copy as dose-rate-study's). Molecules and
  water dissociation from `G4ChemDissociationChannels_option1`.
- IRT_syn (`G4DNAIndependentReactionTimeModel`) only, no mesoscopic stage.
  The partially diffusion-controlled pairs are type 1.
- The bulk reactions (`BuildPureWaterBulkReactions`): the acid-base buffer
  against `H2O`, `H3Op(B)`, `OHm(B)`, and e_aq, H, O- + O2. They act through
  one `G4DNAScavengerProcess` per molecule and one `G4DNAScavengerMaterial`
  per thread. The dissolved O2 is the `O2` configuration, so a bulk reaction
  that produces O2 returns it to the bulk. At 0 it is left out of the
  material, and the O2 reactions are inert.
- The bulk reservoir is a cube of half side 500 um, as in dose-rate-study,
  not the 4 um box. `G4DNAScavengerMaterial` holds floor(N_A c V) molecules
  per species, and `G4DNAScavengerProcess` divides by V again, so V only sets
  the rounding and the depletion. In the box, pH 7 would give 3 H3O+
  instead of 3.85. H2O, H3O+(B) and OH-(B) are never consumed. The O2 at
  21 % is 1.6e14 molecules in the reservoir, so a history uses a negligible
  part of it. `G4Scheduler` resets the material before every chemistry
  stage, so each history starts with the full bulk.
- The end time is the `G4Scheduler`'s: 1 us by default (set in
  `ConstructProcess` on the master and every worker), or
  `/scheduler/endTime` after `/run/initialize`, which every worker replays
  before its next run.
- Space: the molecules diffuse with `G4DNABrownianTransportation` in box +
  water margin (see "Geometry") and are counted wherever they are. IRT_syn
  has no bounding box of its own: the only `G4DNABoundingBox` here is the
  bulk reservoir, which is a volume for the bulk concentrations, not a wall.
  Each Brownian transport carries a `G4BrownianAction`
  (`OutsideWaterAction`), which kills a molecule outside water as Geant4
  does and counts it. The count is `moleculesKilledOutsideWater`. It was 0 in
  every run with the default margin and end time.
- At the start of each run, the master warns (`G4Exception` JustWarning
  `PhspReplay035`) when sqrt(6 D_max t_end) > margin / 3. D_max is the
  fastest tracked species, H3O+ at 9.46e-9 m2/s. That length is 238 nm at
  1 us, against 667 nm for the default margin. At 10 us it is 753 nm, so
  that warning fires.

Why the margin. Without it, the box sat in vacuum, and a molecule that
diffused out of the box, or a dissociation product placed just outside it,
was killed and dropped out of `Species.csv`. Measured on the 7749-history
file (base seed 12345, 18 threads, pH 7, no O2, 1 us;
`evidence/04-margin-gvalues.txt`):

| | no margin (first version) | 2 um margin |
|---|---|---|
| molecules killed outside water | 36433 | 0 |
| G(e_aq) at 1 us | 2.29 | 2.46 |
| G(OH) at 1 us | 2.45 | 2.57 |
| G(H2O2) at 1 us | 0.46 | 0.49 |
| G(H2) at 1 us | 0.42 | 0.45 |
| G(H3O+) at 1 us | 2.69 | 2.98 |
| wall time, 18 threads (physics alone: 32 s) | 56 s | 263 s |

The loss biased the G-values low by 5 to 11 % at 1 us. It would grow with
the end time. At 1 ps the two agree (G(e_aq) = 3.90, G(OH) = 4.72), and
the pre-chemical yields are the same.

The margin costs time. On the `RecordSmoke` file on 1 thread, the run takes
8.1 s without the margin and 12.6 s with it (5 um: 15.4 s). The scheduler
takes 30 % more steps. On the large file the chemistry part is about 10
times slower, because the cost grows with the number of molecules.
`G4DNABrownianTransportation` limits the time step of a molecule near a
volume boundary, and the box/margin interface is one: the box has to stay
its own volume for `DnaRegion`. The UHDR example's IRT_syn transport,
`G4ChemReboundTransportation` with a reflecting `G4DNABoundingBox`, does not
step on geometry boundaries. It would remove this cost, but it is a
different transport model from PFLASH's, so it is not used here.

How it is switched on. The `/phsp/chem/` settings are PreInit, but the
physics list is handed to the run manager before the macro, and that is
when Geant4 builds the particles (`G4RunManagerKernel::SetupPhysics`). So
`main()` calls `ReplayPhysicsList::EnableChemistry` just before
`/run/initialize`, and only if the chemistry is on. That call:

- sets the time-step model to IRT_syn, or stops if the macro chose another
  one;
- builds the molecules (still in PreInit, which `G4ParticleDefinition`
  requires) and gives each its definition ID, as `SetupPhysics` does;
- activates the Geant4-DNA chemistry (`SetChemistryList`), before the DNA
  models are initialised: `G4DNABornIonisationModel1::Initialise` keeps the
  chemistry manager only if the chemistry is active by then;
- adds the chemistry as the last physics constructor, after the EM ones,
  whose e- processes it extends.

With `/phsp/chem/enable false` none of this runs: no molecule and no chemistry
list. The physics is then that of ticket 03. `Histories.csv` was compared,
byte for byte, with the ticket-03 binary on the `RecordSmoke` file (2
threads, both `--physics` options) and on the 7749-history file of
"Stage-2 physics" (18 threads); all three were identical
(`.scratch/tests/2026-11-26__phase-space-source/04-baseline/`,
`04-big/`).

Order within an event (checked in the 11.4.3 source):

- During tracking, the DNA models only create the pre-chemical tracks.
  `G4DNAChemistryManager::CreateWaterMolecule` and `CreateSolvatedElectron`
  build an H2O (ionised, excited or attached) or e_aq track at 1 ps and
  stack it in `G4ITTrackHolder`. They draw no random number.
- `ReplayStackingAction::NewStage` calls `G4DNAChemistryManager::Run` once
  the physics stack is empty (chem1 to chem6, UHDR). The H2O dissociation
  (`G4DNAMolecularDissociation`, `G4DNAWaterDissociationDisplacer`), the
  bulk reactions and the IRT reactions run inside that stage.
- So the chemistry settings (pH, O2, end time) cannot change the physical
  stage of a history. `ReplayChemVariant` checks this.
- Turning the chemistry on does change the physics, because the
  thermalisation model then draws the e_aq displacement
  (`G4DNAOneStepThermalizationModel`, only if the chemistry is active). The
  per-history deposits therefore differ from the chemistry-off run, but the
  mean does not: on the 7749-history file (base seed 12345, 18 threads), the
  stage-2 deposit per history is 589.1 +/- 11.7 eV with the chemistry on,
  597.7 +/- 12.0 eV off, and 595.6 +/- 11.9 eV in stage 1 (paired z =
  -0.54 with the chemistry on).
- The Sanche vibrational-excitation extension to 0.025 eV of
  `G4EmDNAChemistry_option3` is in the code but has no effect in 11.4.3.
  `G4EmDNABuilder` adds the model with `AddEmModel`, so
  `G4VEmProcess::EmModel()` returns null. The same holds in option3, UHDR
  and PFLASH. The manifest records it (`sancheExtendedTo0025eV`).

Pre-chemical counts. A `G4VPhysChemIO` per event (`PreChemicalCounter`)
counts the products that the physics hands to the chemistry. These are
ionised H2O (`preChemIonisedH2O`), excited H2O (`preChemExcitedH2O`), H2O
after dissociative attachment (`preChemAttachedH2O`) and solvated
electrons (`preChemEaq`). They are taken at
`G4UserTimeStepAction::StartProcessing`, so before the first chemistry step
and before any chemistry random number. They are the species at chemistry
start, before the H2O dissociation. A count whose parent track is outside
the box is also counted. That count is printed as `preChemOutsideBox`, and
it was 0 in every run: only the box has DNA models, and tracks outside it
are killed.

`Species.csv`, long layout:

```
globalIndex,timeNs,species,count,gPer100Ev
```

- One row per history, grid time and species that the counter recorded in
  that history. A species that is missing from a history had a count of 0
  throughout.
- The species name is `G4MolecularConfiguration::GetName()`, as in PFLASH's
  `Species.*`: `e_aq^-1`, `°OH^0`, `H3O^1`, `O_2^-1`, ... The file is UTF-8.
- The counts come from one `G4MoleculeCounter` per worker, as in chem4 to
  chem6: H2O ignored, 1 ps time precision, reset before each event.
- The time grid is 1 ps x 10^(k/5) for k = 0, 1, ..., up to the end time,
  plus the end time itself: 5 points per decade from 1 ps, the start of the
  chemistry. The default 1 us gives 31 times, ending at exactly 1000 ns.
  Points below about 10 ps are only resolved to the 1 ps precision of the
  counter.
- G = count per 100 eV of that history's stage-2 deposit. It is empty when
  the deposit is 0.

Pooled over the 7749 histories with the 2 um margin (sum of counts / sum
of deposits, pH 7, no dissolved O2, `evidence/04-margin-gvalues.txt`): at
1 us, G(e_aq) = 2.46, G(OH) = 2.57, G(H3O+) = 2.98, G(H) = 0.65,
G(H2) = 0.45 and G(H2O2) = 0.49 per 100 eV. At 1 ps, G(e_aq) = 3.90 and
G(OH) = 4.72. The pre-chemical yields are 4.77 ionisations, 0.57
excitations and 4.69 e_aq per 100 eV. `ReplayChemVariant` (84 histories)
gives, summed at 1 us, about 1260 e_aq without O2 and 10 with 21 % O2, and
about 1270 O2- with O2. The exact numbers change from run to run; see
"Chemistry reproducibility". At 0.273 mM, k = 1.74e10 /M/s gives e_aq a
lifetime of 210 ns.

### Chemistry reproducibility

Only the physical stage is bit-reproducible. The pre-chemical counts in
`Histories.csv` and its other columns do not depend on the chemistry
settings, the thread count or the range split. `Species.csv` is not
bit-reproducible from one process to the next, even for the same command.
Two 1-thread runs of the `RecordSmoke` file with 21 % O2 differ in 20 of 80
histories, and 1, 2 and 4 threads or a split range give the same picture
(`evidence/04-chemistry-reproducibility.txt`).

The cause is in Geant4. `G4DNAScavengerProcess` keeps the bulk partners of
each molecule in a `std::map` keyed by `G4MolecularConfiguration*`.
`PostStepGetPhysicalInteractionLength` adds their propensities in that map
order and picks the partner with one random number
(`G4DNAScavengerProcess.cc`, lines 141-185). The map order is the order of
the configurations' heap addresses, and the heap layout changes from one
process to the next. A temporary debug print of that order for `H2O`,
`H3Op(B)`, `OHm(B)` and `O2` was run five times. Runs with the same or an
equivalent order gave byte-identical `Species.csv` files, and the others
did not. So with a fixed address order the chemistry stage is
deterministic, and the only problem is the process-dependent order.

Consequences:

- Chemistry variants compared history by history share the same physical
  stage and the same pre-chemical species.
- Their chemistry stages are independent samples, so species yields agree
  between repeats only within their statistical errors.
- Making the chemistry bit-reproducible would need a change in Geant4 (key
  the partner map by molecule ID) or one scavenger process per bulk
  partner, which `G4PhysicsListHelper` refuses for the same molecule (same
  process subtype). Neither is done here.

### Stage-2 physics: stage 1 vs `G4EmDNAPhysics_option2`

The e- processes and models were compared with `/particle/process/dump`,
`/process/em/printParameters` and the `/process/em/verbose 1` model tables.
Stage 1 is `phsp-record`; stage 2 is `phsp-replay` with both options.

| | stage 1 (option4 + `G4EmDNAPhysicsActivator` in `DnaRegion`) | `G4EmDNAPhysics_option2` constructor |
|---|---|---|
| e- processes | Transportation, msc, eIoni, eBrem, ePairProd, CoulombScat, the six `e-_G4DNA*` processes | the same without ePairProd and CoulombScat |
| e- DNA models in the box | solvation (Meesungnoen2002) below 7.4 eV, Champion elastic up to 1 MeV, Born excitation and ionisation up to 1 MeV, Sanche below 100 eV, Melton below 13 eV | the same |
| standard e- models in the box | GS msc, Moller-Bhabha and SB brems active only above 1 MeV; ePairProd (Mephi) and CoulombScat (above 100 MeV) from the world region | GS msc, Moller-Bhabha, SB above 1 MeV |
| gamma below 1 MeV | Livermore photoelectric, LowEP Compton, Livermore Rayleigh, grouped in the general gamma process (`Use general process 1`) | the same models as separate processes |
| `DNAFast` | false | true (set by the constructor) |
| Auger, de-excitation ignores cuts | off, off | on, on |
| fluorescence | on | on |

These differences matter for the energy check:

- `DNAFast`: `G4DNABornIonisationModel1` reads `G4EmParameters::DNAFast()`
  in its constructor. With it on, the energy of the ejected electron is
  sampled from cumulated tables (`sigmadiff_cumulated_ionisation_e_born_hp`).
  With it off, it is sampled by rejection on the differential tables, about
  1 ms per ionisation. So the two stages would use different samplings of
  the same model.
- Auger: `G4DNABornIonisationModel1` calls the de-excitation for oxygen
  K-shell ionisations without cuts. With Auger on, the binding energy left
  over after fluorescence leaves as an Auger electron instead of being
  deposited locally. This is a small effect at the box edges.
- ePairProd and CoulombScat do not act on a 500 keV e- or its secondaries,
  and msc, eIoni and eBrem are inactive below 1 MeV in the box. Inactivating
  each of these five in a replay left every per-history deposit unchanged.

**Choice:** `--physics stage1`, the default, builds stage 2 with the
stage-1 setup applied to the box: `G4EmStandardPhysics_option4`, then
`AddDNA("DnaRegion", "DNA_Opt2")`, then `G4EmDNAPhysicsActivator`. The
constructor order, the default cut and the region cuts are those of
`RecordPhysicsList`, which is compiled into `phsp-replay`. The physics in
the box is then the same by construction. The `/process/em/verbose 1` EM
model tables and the `printParameters` output of stage 1 and stage 2 are
identical, line for line. `--physics option2` is kept for comparison.

Do not change `/process/dna/UseDNAFast` (or other EM parameters) in one
stage only: the physics would differ again. The manifest records
`dnaFast`.

Measured, with stage 1 = the `RecordSmoke` geometry with 10^5 primaries
(seed 20261127, 7749 histories) and stage 2 with base seed 12345 on 18
threads (`.scratch/tests/2026-11-26__phase-space-source/evidence/03-measurements.txt`):

| | `--physics stage1` | `--physics option2` |
|---|---|---|
| stage-1 in-box deposit per history | 595.6 +/- 11.9 eV | 595.6 +/- 11.9 eV |
| stage-2 deposit per history | 597.7 +/- 12.0 eV | 602.3 +/- 12.6 eV |
| stage 2 - stage 1 (paired) | 2.1 +/- 12.3 eV, z = 0.17 | 6.7 +/- 12.2 eV, z = 0.55 |
| time per history (1 thread, `RecordSmoke` file) | 47 ms | 2.8 ms |

Both options pass the energy check at this precision (2 %). The 17-fold
cost of `stage1` is entirely `DNAFast`: with `/process/dna/UseDNAFast true`
added, `stage1` also takes 2.8 ms per history. The step counts of the two
options agree. Stage 1 pays the same price. `RecordSmoke` with
`/process/dna/UseDNAFast true` takes 1.15 ms per primary instead of 13.9 ms
(1 thread). Turning `DNAFast` on in both stages together would keep them
identical and make both about 12 to 17 times faster. It changes the
stage-1 physics of the files, so it stays off; it is an open option (see
"Limits").

## Acceptance

`scripts/acceptance.py` (Python 3, standard library only) runs the built
executables and prints one PASS/FAIL line per check with its numbers. It exits
with 0 only when every check passes:

```
python examples/phase-space-source/scripts/acceptance.py --build examples/phase-space-source/build `
       --out .scratch/tests/2026-11-26__phase-space-source/acceptance [--threads N] [--checks energy,repeat,lattice,chemvariant]
```

It adds `<install>/bin` (from `Geant4_DIR` in `CMakeCache.txt`) to `PATH`, as
the CTests do. Each run's log is in the output directory, and the result lines
are in `acceptance.txt`. ENERGY, REPEAT and CHEMVARIANT use a fresh stage-1
file from `macros/record.mac` (`--seed 1`, 311 histories). All replays use
base seed 20261127.

| Check | Definition | Result (18 threads, 2026-11-27) |
|---|---|---|
| ENERGY | stage 2 with chemistry off vs the stage-1 in-box deposit, per history over the same histories; paired difference d = stage 2 - stage 1, \|mean(d) / SE(d)\| < 3, and no track outside the box | 311 histories: stage 1 872.1 +/- 71.1 eV, stage 2 860.7 +/- 70.1 eV; d = -11.4 +/- 74.9 eV, z = -0.15; 0 tracks outside |
| REPEAT | chemistry on, end time 10 ns; every `Histories.csv` column (stage-2 deposit, pre-chemical counts, end time) against run a (one run, 1 thread): b the same again, c the range split into two runs in one process, g the second half alone in a fresh process, d 2 threads, f another `/random/setSeeds` with the same base seed. `Species.csv` is not compared ("Limits") | 0 differing rows in b (of 311), c (311), g (156), d (311) and f (311) |
| LATTICE | box at 50 um depth (default phantom, box, shell and pitch); recorded and replayed end to end (chemistry off) as a lattice (P = 50 um, beam uniform over one P x P cell, 2 x 10^5 primaries) and as a single box (pitch 0, beam uniform over a 2W x 2W square, W = 5.5 P = 275 um, 10^7 primaries); mean stage-2 deposit per unit fluence within 3 combined SE, for the whole lattice and for the lattice restricted to the field | lattice 15060 +/- 610 eV um^2 (1945 histories), single box 14380 +/- 1050 eV um^2 (697 histories): z = 0.55; restricted lattice 13440 +/- 580 eV um^2, z = -0.78; 10.7 % of the broad-beam deposit is outside the field. Stage-1 deposits: z = 1.93 (whole) and 0.56 (restricted) |
| CHEMVARIANT | chemistry on, 1 us, dissolved O2 0 and 21 % on the same histories: identical `Histories.csv`; G(e_aq) at 1 us lower with O2 by more than 3 combined SE; no molecule killed outside water | 0 differing rows of 311; G(e_aq) = 2.536 +/- 0.070 without O2 and 0.0222 +/- 0.0031 with 21 % O2 per 100 eV (35.9 combined SE apart); 0 molecules killed outside water |

The whole run took 789 s (13 min) on 18 threads. Most of it is the single-box
stage 1 (277 s) and the two 1 us chemistry replays (115 s without O2, 116 s with
O2). Output: `.scratch/tests/2026-11-26__phase-space-source/acceptance/`.

How the LATTICE numbers are computed:

- Fluence. On the lattice, each primary is fired uniformly over one cell of
  area P^2, and every cell holds one box, so the fluence seen by a box is
  N / P^2 (ADR 0014). For the single box, the beam is uniform over a square of
  area (2W)^2 that contains the box, so the fluence is N / (2W)^2. N is the
  number of primaries fired (`primariesFired` in the header), including those
  with no record.
- Deposit per unit fluence = (sum of the deposits / N) x area, in eV um^2.
  The standard error is taken over the N primaries: the deposits of the
  histories of one primary (several boxes) are added first, and primaries
  with no history count as 0.
- With W = (k + 1/2) P, the lattice boxes with max(|ix|, |iy|) <= k see
  exactly the primary-to-box offsets that the single box sees under its
  field (translation invariance). The "restricted" comparison is therefore
  free of the field-size bias. The deposit of the other lattice boxes, 10.7 %
  here, is what a 275 um half field misses from the broad beam: the electrons
  backscattered from deeper in the phantom ("Defaults: shell and pitch").
  The whole-lattice comparison carries that bias (about 1 combined SE). A
  field wide enough to make it negligible (W of about 1 mm) would need about
  14 times as many single-box primaries.

The CTests `ReplayRepeat`, `ReplayEnergy` and `ReplayChemVariant` run reduced
versions of the same checks on the 84-history `RecordSmoke` file.

## Limits

What the results mean:

- Only the physical stage is bit-repeatable: `Histories.csv` (deposit,
  pre-chemical counts) is identical for a repeat, a range split, another
  thread count or another `/random/setSeeds`. `Species.csv` is not, even for
  the same command: `G4DNAScavengerProcess` picks the bulk partner in the
  order of a map keyed by configuration pointers, which changes from process
  to process ("Chemistry reproducibility"). Chemistry variants share their
  physical and pre-chemical stages history by history; their chemistry
  stages are independent samples.
- Chemistry on and chemistry off are two different samples of each
  history's physics: with the chemistry active, the e_aq thermalisation
  draws a displacement during tracking (ADR 0015 addendum). The means agree.
  Compare chemistry variants with the chemistry on in every run.
- The two stages agree in the mean, not track by track (ADR 0014): a
  recorded re-entry of a secondary born in the box is not correlated with
  the secondary that stage 2 draws itself.
- Lattice mode assumes a laterally uniform broad beam and a phantom wider
  than the lateral reach of the radiation (about 1 mm at 500 keV, from
  backscatter). Two histories of one primary in two boxes are correlated
  (0.9 % of the hit primaries at the default pitch); the standard errors
  treat histories as independent.
- A single box (pitch 0) gets the deposit of its beam only: a field narrower
  than the lateral reach misses part of the broad-beam deposit (about 10 %
  for a half side of 275 um at 50 um depth, "Defaults: shell and pitch").
- Stage 2 reads only the binary file: the stage-1 physics in the sidecar is
  not checked. Keep the stage-2 physics option and the EM parameters equal
  to stage 1's (`--physics stage1`, the default, does this).
- The Sanche extension to 0.025 eV has no effect in 11.4.3 (as in PFLASH and
  the Geant4 examples), and stage 1 prints the known `Run0111` "Duplication
  of processes" warning (`nuclearStopping` for `GenericIon`).

Open options, measured but not changed (a decision for the user):

- **`DNAFast`** is off in both stages, as the stage-1 setup (option4 +
  `G4EmDNAPhysicsActivator`) leaves it. With `/process/dna/UseDNAFast true`,
  `G4DNABornIonisationModel1` samples the ejected-electron energy from
  cumulated tables instead of by rejection: stage 2 takes 2.8 ms per
  history instead of 47 ms (1 thread, `RecordSmoke` file, chemistry off)
  and stage 1 1.15 ms per primary instead
  of 13.9 ms (`RecordSmoke`), with the same step counts ("Stage-2 physics").
  It must be switched in both stages together, and it changes the stage-1
  physics of every new file.
- **Water margin.** The 2 um G4_WATER margin around the stage-2 box keeps
  the molecules that diffuse out of the box (without it G at 1 us was 5 to
  11 % low), but it makes the chemistry 2 to 10 times slower:
  `G4DNABrownianTransportation` shortens time steps near the box/margin
  boundary (`RecordSmoke` file, 1 thread: 8.1 s without the margin, 12.6 s
  with it; 7749 histories on 18 threads: 56 s and 263 s). The UHDR
  example's `G4ChemReboundTransportation` with a reflecting
  `G4DNABoundingBox` would avoid the boundary steps, but it is another
  transport model than PFLASH's ("Chemistry").
- **Thinner shell.** A 0.25 um shell costs 37 % less per stage-1 history
  than the default 1 um, with no change of the in-box deposit at the 5 %
  precision of "Defaults: shell and pitch".

Not done: the IAEA phase-space format, recycling or rotating histories, the
mesoscopic stage, Chemistries other than PureWater (+ pH, O2), beam pulse
structure, changes to PFLASH `sim`. The acceptance runs are verification
runs, not Study runs.

## Build and test

Build RelWithDebInfo against the stock Geant4 11.4.3 install: a Debug build
crashes on the first `G4String` against the release install. Configure from
the MSVC x64 environment (`.claude/geant4-instructions.md`). A fresh build
directory needs `-DCMAKE_TOOLCHAIN_FILE=<DEV_DIR>/vcpkg/scripts/buildsystems/vcpkg.cmake`:
the Geant4 11.4.3 config calls `find_dependency(ZLIB 1.3.1)`
(`Geant4Config.cmake`), and without the vcpkg toolchain ZLIB is not found and
the configure fails. Once configured, the toolchain stays in the cache, and a
rebuild needs only `cmake --build`:

```
cmake -S examples/phase-space-source -B examples/phase-space-source/build -G Ninja `
      -DCMAKE_BUILD_TYPE=RelWithDebInfo `
      -DCMAKE_TOOLCHAIN_FILE=$env:DEV_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake `
      -DGeant4_DIR=$env:G4_ROOT/lib/cmake/Geant4
cmake --build examples/phase-space-source/build
ctest --test-dir examples/phase-space-source/build --output-on-failure
```

The tests use `test/TestCheck.hh`, which stays active under `NDEBUG`, unlike
`assert`.

| Test | Checks |
|---|---|
| `PhaseSpaceFormatTest` | byte layout, encode/decode round trips, FNV-1a and splitmix64 reference values, header and record validation |
| `PhaseSpaceFileTest` | writer + reader round trip, sidecar, writer refusals, truncated and corrupted files; the reader's own table refusals on re-sealed files (valid digest, file ID and header checksum): entries out of (event ID, box iy, box ix) order, a duplicate key, a box other than (0, 0) at pitch 0; the stage-2 start times (`ReplayTimesNs`: record time minus the earliest of the history); writes `box-5um.phsp` for `ReplayRejectsBoxSideMismatchFiles` |
| `HistoryCatalogTest` | global numbering, ranges past the end, duplicate file ID, concurrent reads |
| `HistorySeedTest` | pinned seed values, engine-seed range |
| `LatticeFoldTest` | cell edges, negative cells, rounding, pitch 0 |
| `PhspDump*` | `phsp-dump` output, a corrupted file and the usage message |
| `RecordSmoke` | `phsp-record` on `test/RecordSmoke.mac` (1 mm phantom, box at 0.2 mm, P = 20 um, 1000 primaries) with 1 and 2 threads and the same seed: byte-identical `.phsp` and `.json`; `phsp-dump` reads the file; records = sum of the history record counts; primaries with records (distinct event IDs in the history table) <= primaries, and = the `RECORD_DONE` count; prints the run figures (about 30 s) |
| `RecordUsage`, `RecordRejectsTouchingBoxes`, `RecordRejectsPitch5um`, `RecordRejectsPitch6um` | the usage message; pitches of 4, 5 and 6 um with a 4 um box and a 1 um shell (in (0, box + 2 x shell]) are refused before `/run/initialize` |
| `RecordRejects{Execute,Foreach,Loop,MacroPath,Alias,BeamOnMacro}` | `/control/execute` (of a macro holding `/random/setSeeds`), `/control/foreach`, `/control/loop`, `/control/macroPath`, `/control/alias` and `/run/beamOn 1 <macro>` are refused at their line |
| `ReplayRepeat` | `phsp-replay` on the histories of the `RecordSmoke` file (84), physics only (`/phsp/chem/enable false`), seven times: (a) one run, (b) the same again, (c) split into two runs in one process, (d) on 2 threads, (f) another `/random/setSeeds`, (g) the second half alone in a fresh process, (e) another base seed. b, c, d and f give the same `Histories.csv` rows as a, and g gives a's rows 42..83. e changes stage-2 deposits (82 of 84) and no stage-1 column. f and g fail without the per-history reseed; b, c and d would pass without it (about 40 s) |
| `ReplayEnergy` | the same histories once, physics only: prints the stage-1 and stage-2 mean deposit per history with their SE and the z score of the paired difference; passes for \|z\| < 3 and no track found outside the box (84 histories: 709.5 +/- 171.0 eV vs 802.2 +/- 187.0 eV, z = 0.87) |
| `ReplayChemVariant` | the same histories with the chemistry on and the default end time: (a) no O2 on 1 thread, (b) `/phsp/chem/o2 21 %` on 2 threads, (c) no O2 with the range split into two runs in one process. `Histories.csv` of b and c identical to a in every column (stage-2 deposit, pre-chemical counts, end time 1000 ns); pre-chemical products present (4988), none outside the box; no molecule killed outside water; the manifest records the O2, the 2 um margin and the kill count; e_aq at 1 us summed over the histories lower with O2 (about 10 vs 1260). About 40 s |
| `ReplayMultiFile` | two files, [`RecordSmoke` file, round-trip file]: the second file's 3 histories alone (global 84..86), then that file alone, then the same content under another file ID (`other-seed.phsp`). The rows carry the second file's ID and history indices 0..2; they equal the single-file run in every column but `globalIndex` (the seed comes from the file ID and history index, not the global index or the file's position); the other file ID changes stage-2 deposits (2 of 3); `Manifest.json` lists both files in order with their IDs and first global indices (0, 84), the range, the base seed (about 7 s) |
| `ReplayRejects*`, `ReplayUsage` | FatalExceptions before any event: a duplicate file, a range past the end, a range overlapping an earlier run, a `/phsp/geom/boxSide` other than the file's, two files with different box sides (4 and 5 um, `PhspReplay004`), an unknown `/phsp/chem/o2` unit, a time-step model other than IRT_syn, `/run/initialize` from a nested macro with the chemistry on; the usage message |

The `Replay*` tests read the 1-thread file of `RecordSmoke` (a CTest fixture),
so `ctest -R Replay` also runs `RecordSmoke`. With 84 histories the energy
check resolves about 15 % of the mean deposit; the 7749-history measurement in
"Stage-2 physics" resolves 2 %.

`RecordSmoke` and the other `Record*`/`Replay*` tests need the Geant4 DLLs
(`<install>/bin`, which CTest adds to `PATH`) and the Geant4 data variables
(`G4LEDATA`, ...) in the environment.
