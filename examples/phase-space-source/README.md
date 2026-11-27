# phase-space-source

Standalone Geant4-DNA example: simulate the physical stage of an electron beam
in a water phantom once, save the particles that enter a small scoring box at
depth in a reusable **Phase-space file**, and replay that file in several
Geant4-DNA physics + chemistry runs of the box. The same histories are
replayed in the same order with the same physics in every run, so chemistry
variants can be compared history by history and a run can be repeated exactly.

Design records: `docs/adr/0014-phase-space-crossing-rule.md` (every inward
crossing is recorded, stage 1 kills nothing, the Scoring lattice) and
`docs/adr/0015-per-history-seeding.md` (stage 2 reseeds per history). Glossary:
Phase-space file, Phase-space history, Inward crossing, Scoring lattice in
`CONTEXT.md`.

The example copies the code it needs (`DataNode`/`JsonWriter` from PFLASH
`src/scoring/`, `PureWaterReactions` and `ChemistryTypes` from PFLASH
`src/chemistry/`) and links nothing from PFLASH `sim` or the other examples;
a change there does not reach this example.

Status: ticket 04. The Geant4-free core, `phsp-dump`, stage 1
(`phsp-record`) and stage 2 (`phsp-replay`: physics, then an IRT_syn
chemistry stage) are complete. The acceptance runs and the defaults come in
ticket 05.

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
  unsigned 64-bit integer, default 1) and is written in the file header.
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
face of a 5 mm phantom, uniform over one 100 um x 100 um lattice cell,
10^5 primaries: 259 histories with `--seed 1` (for any thread count), about
100 thread-seconds. `macros/replay.mac` replays the first 100 of them.

### Geometry

| Command (PreInit) | Default | Meaning |
|---|---|---|
| `/phsp/geom/phantomSide` | 5 mm | side of the G4_WATER phantom cube, centred at the origin; the beam enters through its -z face |
| `/phsp/geom/depth` | 1 mm | depth of the box centres below the -z face |
| `/phsp/geom/boxSide` | 4 um | side of a cubic scoring box |
| `/phsp/geom/shell` | 1 um (provisional) | DNA water on each side of the box |
| `/phsp/geom/latticePitch` | 100 um (provisional) | 0 = one box on the axis, else P > boxSide + 2 x shell |

The world is G4_Galactic, 0.1 mm wider than the phantom on every side. Each
scoring box sits at the centre of its own DNA shell: a G4_WATER cube of side
boxSide + 2 x shell, a daughter of the phantom; the shells form the region
`DnaRegion`, and the box is the shell's only daughter. The water between the
shells is condensed history. For P > 0 the shells sit at (ix P, iy P) at the
box depth, for |ix|, |iy| <= n with
n = floor((phantomSide - boxSide - 2 x shell) / (2 P)), so every shell is
inside the phantom (49 x 49 by default). For P = 0 there is one shell on the
axis. The copy number of a shell is the box index
(iy + n)(2n + 1) + (ix + n); the box index of a point in a box is the copy
number of its mother shell.

`phsp-record` checks the five values together before `/run/initialize`, and
exits with status 3 for a pitch in (0, boxSide + 2 x shell], a shell that
does not fit in the phantom, or more than 10^6 boxes. With such a pitch
neighbouring shells would overlap or share a face (with shell 0, neighbouring
boxes would share a face and a crossing point on it would belong to two
boxes).

The shell and the pitch are provisional; ticket 05 settles them from
measurements. Why 1 um is enough for the shell for now: outside the region,
condensed-history delta rays are produced only above the e- production
threshold. With the default 1 um cut, that threshold is 990 eV in water (the
lower edge of the cut table, read from the cut-table printout). A delta ray
below it, which the condensed history does not produce, has a range in water
of order 0.05 um, so it could not reach the box from beyond the shell.

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

### Measured (default geometry, box at 1 mm, P = 100 um and 20 um)

`macros/record.mac` (5 mm phantom, 4 um box at 1 mm, 1 um shell, 500 keV
e-), seed 1, 18 threads, stock Geant4 11.4.3 on a 20-core Windows machine.
For P = 20 um the macro's pitch and the beam's `halfx`/`halfy` (10 um) were
changed. Errors are one standard error.

| Quantity | P = 100 um | P = 20 um |
|---|---|---|
| primaries fired | 2 x 10^6 | 10^5 |
| hit fraction (primaries with a record) | 0.00260 +/- 0.00004 | 0.0585 +/- 0.0007 |
| histories (one per primary and box) | 5213 | 6243 |
| histories per hit primary | 1.003 (16 primaries in 2 boxes) | 1.067 (304 in 2 boxes, 39 in 3 to 5) |
| records per history | 1.22 | 1.22 |
| records by particle | 6217 e- (4944 of them the primary), 118 gamma | 7470 e- (5941 the primary), 140 gamma |
| histories by record count | 4878 with 1, 335 with 2 to 37 | 5839 with 1, 404 with 2 to 35 |
| mean in-box deposit per history | 909 eV | 909 eV |
| mean in-box deposit per primary | 2.37 +/- 0.06 eV | 56.8 +/- 1.4 eV |
| deposit per unit fluence (per primary x P^2) | (2.37 +/- 0.06) x 10^4 eV um^2 | (2.27 +/- 0.06) x 10^4 eV um^2 |
| time per primary (thread time) | 0.96 ms | 13.6 ms |
| primaries for 10^4 histories | 3.8 x 10^6 (about 1 thread-hour) | 1.6 x 10^5 (about 0.6 thread-hour) |

- The hit fraction is about 1.5 times the box's share of the lattice cell,
  (4 / P)^2, because the side faces are crossed too.
- The deposit per unit fluence agrees between the two pitches (1.1 standard
  errors apart), as the broad-beam equivalence of ADR 0014 expects. Ticket 05
  runs the full lattice-vs-single-box check.
- The time per primary scales with the number of DNA shells, that is with the
  share of the phantom area they cover, (6 / P)^2. Before the per-box shells
  (one full-width DNA layer) it was 107 ms per primary at P = 100 um, almost
  all of it in DNA steps in the layer.
- One primary enters more than one box in 0.3 % (P = 100 um) and 5.9 %
  (P = 20 um) of the hit primaries. Each of those boxes gives its own history.

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
identical and make both about 12 to 17 times faster. It changes the stage-1
physics of the files, so it is left to ticket 05.

## Build and test

Build RelWithDebInfo against the stock Geant4 11.4.3 install: a Debug build
crashes on the first `G4String` against the release install. Configure from
the MSVC x64 environment (`.claude/geant4-instructions.md`), with the vcpkg
toolchain, which provides the ZLIB and HDF5 that the Geant4 config looks for:

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
| `PhaseSpaceFileTest` | writer + reader round trip, sidecar, writer refusals, truncated and corrupted files |
| `HistoryCatalogTest` | global numbering, ranges past the end, duplicate file ID, concurrent reads |
| `HistorySeedTest` | pinned seed values, engine-seed range |
| `LatticeFoldTest` | cell edges, negative cells, rounding, pitch 0 |
| `PhspDump*` | `phsp-dump` output, a corrupted file and the usage message |
| `RecordSmoke` | `phsp-record` on `test/RecordSmoke.mac` (1 mm phantom, box at 0.2 mm, P = 20 um, 1000 primaries) with 1 and 2 threads and the same seed: byte-identical `.phsp` and `.json`; `phsp-dump` reads the file; records = sum of the history record counts; primaries with records (distinct event IDs in the history table) <= primaries, and = the `RECORD_DONE` count; prints the run figures (about 30 s) |
| `RecordUsage`, `RecordRejectsTouchingBoxes` | the usage message; a pitch equal to the box side (so <= box + 2 x shell) is refused before `/run/initialize` |
| `ReplayRepeat` | `phsp-replay` on the histories of the `RecordSmoke` file (84), physics only (`/phsp/chem/enable false`), seven times: (a) one run, (b) the same again, (c) split into two runs in one process, (d) on 2 threads, (f) another `/random/setSeeds`, (g) the second half alone in a fresh process, (e) another base seed. b, c, d and f give the same `Histories.csv` rows as a, and g gives a's rows 42..83. e changes stage-2 deposits (82 of 84) and no stage-1 column. f and g fail without the per-history reseed; b, c and d would pass without it (about 40 s) |
| `ReplayEnergy` | the same histories once, physics only: prints the stage-1 and stage-2 mean deposit per history with their SE and the z score of the paired difference; passes for \|z\| < 3 and no track found outside the box (84 histories: 709.5 +/- 171.0 eV vs 802.2 +/- 187.0 eV, z = 0.87) |
| `ReplayChemVariant` | the same histories with the chemistry on and the default end time: (a) no O2 on 1 thread, (b) `/phsp/chem/o2 21 %` on 2 threads, (c) no O2 with the range split into two runs in one process. `Histories.csv` of b and c identical to a in every column (stage-2 deposit, pre-chemical counts, end time 1000 ns); pre-chemical products present (4988), none outside the box; no molecule killed outside water; the manifest records the O2, the 2 um margin and the kill count; e_aq at 1 us summed over the histories lower with O2 (about 10 vs 1260). About 40 s |
| `ReplayRejects*`, `ReplayUsage` | FatalExceptions before any event: a duplicate file, a range past the end, a range overlapping an earlier run, a `/phsp/geom/boxSide` other than the file's, an unknown `/phsp/chem/o2` unit, a time-step model other than IRT_syn, `/run/initialize` from a nested macro with the chemistry on; the usage message |

The `Replay*` tests read the 1-thread file of `RecordSmoke` (a CTest fixture),
so `ctest -R Replay` also runs `RecordSmoke`. With 84 histories the energy
check resolves about 15 % of the mean deposit; the 7749-history measurement in
"Stage-2 physics" resolves 2 %.

`RecordSmoke` and the other `Record*`/`Replay*` tests need the Geant4 DLLs
(`<install>/bin`, which CTest adds to `PATH`) and the Geant4 data variables
(`G4LEDATA`, ...) in the environment.
