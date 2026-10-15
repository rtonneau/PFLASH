# Create a species counter

## What this is

How to count molecular species over time during the chemistry stage. This
recipe covers the Geant4-DNA-specific layer on top of the generic
`G4VPrimitiveScorer` mechanics (registration, `ProcessHits`/`EndOfEvent`,
MT merge) — see [`create-a-scorer.md`](create-a-scorer.md) for those; this
page only covers what is specific to counting species: the `G4MoleculeCounter`
hook, how `ScoreSpecies` reads it, and how the result is normalised and
exported.

## Generic pattern

Geant4-DNA's chemistry stage advances molecules through `G4ITStepManager`/
`G4Scheduler`; a `G4MoleculeCounter` (registered on a per-thread
`G4MoleculeCounterManager`) observes every molecule creation/destruction and
can answer "how many molecules of species X existed at time T" after the
fact (`GetNbMoleculesAtTime`), for whichever `G4MolecularConfiguration`
indices it has recorded (`GetMapIndices`). A species counter does not drive
the chemistry itself — it is a passive scorer that reads the
`G4MoleculeCounter`'s already-recorded history once per event, at a fixed
set of time points, and folds the result into a per-event G-value
(radiochemical yield) normalised by that event's energy deposit.

## dnachem-min reference implementation

- `src/actions/ActionInitialization.cc:80-98` (`BuildMoleculeCounters`) is
  where the counter itself is set up, once per thread (called from `Build()`
  and `BuildForMaster()`): it registers one `G4MoleculeCounter` named
  `"BasicCounter"` on `G4MoleculeCounterManager::Instance()`, ignoring
  `G4H2O` and using a fixed 10 ps time-comparison precision
  (`G4MoleculeCounterTimeComparer::CreateWithFixedPrecision`). It also sets
  `SetResetCountersBeforeEvent(true)` and `SetResetCountersBeforeRun(true)`
  (both default `false`) — required because `ScoreSpecies::EndOfEvent`
  normalises by that event's own energy deposit; without a per-event reset,
  event *k* would see the cumulative count of events 1..k and the summed
  G-values would be inflated by `(nEvents+1)/2` (comment at
  `ActionInitialization.cc:83-86`).
- `src/scoring/ScoreSpecies.cc` (+ `header/scoring/ScoreSpecies.hh`) is the
  `G4VPrimitiveScorer` that reads the counter. Verified from its actual
  source, not inferred from the class name:
  - The constructor (`ScoreSpecies.cc:44-61`) builds a fixed
    `fTimeToRecord` set: 10 log-spaced points per decade from 1 ps to 1 us
    (60 points, exponents 0.0 to 5.9), plus one extra point at 999999 ps as
    a safety margin just under the scheduler's default 1 us end time.
  - `ProcessHits` (`ScoreSpecies.cc:72-85`) only accumulates `fEdep`, the
    step's energy deposit (weighted) — it has nothing to do with species
    counting; species counts come entirely from the `G4MoleculeCounter`,
    read once per event in `EndOfEvent`, not per step.
  - `EndOfEvent` (`ScoreSpecies.cc:103-188`) is the real hook: it fetches
    counter index 0 via
    `G4MoleculeCounterManager::Instance()->GetMoleculeCounter<G4MoleculeCounter>(0)`
    (fatal `G4Exception` if absent), then for each recorded species index
    and each time in `fTimeToRecord` calls
    `counter->GetNbMoleculesAtTime(idx, time_mol)` and folds the count into
    `fSpeciesInfoPerTime[time_mol][idx.Molecule]` (`SpeciesInfo::fNumber`,
    plus the running sums `fG`/`fG2` of `(n_mol / (fEdep/eV)) * 100` — the
    G-value in molecules per 100 eV). Aborted events are skipped entirely
    (`fEdep` reset, no counter read, `ScoreSpecies.cc:105-109`).
  - The loop is capped at the **hand-over time** when the mesoscopic stage
    is active (`ScoreSpecies.cc:142-152`): `TimeStepAction` mutes the
    molecule counter from hand-over to end-of-event (see below), so later
    record times would read stale, frozen hand-over counts; `SpeciesMeso.*`
    (a different counter, `MesoSpeciesCounter`) covers the mesoscopic stage
    instead. With no mesoscopic stage (SBS, or `/chem/meso/enable false`,
    or hand-over at/after the end time), the whole `fTimeToRecord` range is
    scored.
  - `AbsorbResultsFromWorkerScorer` (`ScoreSpecies.cc:192-273`) is the MT
    merge hook — see Serial vs MT below.
- `src/chemistry/TimeStepAction.cc:565-577` is where the counter gets muted:
  right before handing the remaining molecules to the mesoscopic
  `G4DNAEventScheduler`, it calls
  `G4MoleculeCounterManager::Instance()->SetIsActive(false)`
  (thread-local; restored at `EndProcessing`, `TimeStepAction.cc:724`).
  Comment at `TimeStepAction.cc:565-573` explains why: two downstream
  effects would otherwise trip the counter's time-consistency check
  (`TIME_DONT_MATCH`) once the mesoscopic stage runs past the hand-over
  time while the scheduler's own global time stays frozen there.
- `DetectorConstruction::ConstructSDandField()`
  (`src/geometry/DetectorConstruction.cc:124-146`) registers
  `ScoreSpecies("Species")` as a primitive on the shared
  `G4MultiFunctionalDetector` — the generic registration mechanics
  `create-a-scorer.md` covers.

## Output

`Species.Txt` (ASCII, via `ScoreSpecies::ASCII()`) and
`Species_nt_species.csv` (via `ScoreSpecies::OutputAndClear()` →
`WriteWithAnalysisManager`, columns `speciesID,number,nEvent,speciesName,
time,sumG,sumG2`) are written by `RunAccumulatorMessenger`'s
`/run/dumpDataAndReset` handler (`src/scoring/RunAccumulatorMessenger.cc`,
around line 142-151), which looks the `ScoreSpecies` primitive up directly
on the `G4MultiFunctionalDetector` rather than through `RunAccumulator`
(species data is already persistent inside the scorer itself, across
`/run/beamOn` calls, until a dump clears it). File formats, dump/prefix
rules, the hand-over-time cutoff and the per-dump `Manifest.json` entry are
covered by `docs/output/README.md` and the `sim-output` skill — see those
rather than this recipe for the output-file conventions.

## Serial vs MT

Same `Run::Merge`/`AbsorbResultsFromWorkerScorer` mechanism described in
[`create-a-scorer.md`](create-a-scorer.md#serial-vs-mt): under MT, each
worker thread owns its own `ScoreSpecies` instance (and its own
`G4MoleculeCounterManager`/`G4MoleculeCounter`, since both are thread-local);
`Run::Merge` (`src/actions/Run.cc:84-111`) `dynamic_cast`s the master's and
the worker's scorer and calls `AbsorbResultsFromWorkerScorer`, which sums
`fNumber`/`fG`/`fG2` per `(time, species)` pair and `fNEvent`, then clears
the worker's map so a later run doesn't double-merge it
(`ScoreSpecies.cc:265-268`). As with the generic case, this merge happens
inside the Geant4 kernel's `G4MTRunManager::MergeRun`, before the master's
`RunAction::EndOfRunAction` runs — see `create-a-scorer.md`'s fuller
explanation rather than re-deriving it here. In Serial mode there is one
scorer instance and no merge step.

## See also

- [`create-a-scorer.md`](create-a-scorer.md) — the generic
  `G4VPrimitiveScorer` mechanics (registration, lifecycle hooks, MT merge)
  this recipe builds on.
- `docs/output/README.md` and the `sim-output` skill — `Species.Txt` /
  `Species_nt_species.csv` file formats, dump/prefix rules and the
  hand-over-time cutoff.
- [`mesoscopic-3d-species-counting.md`](mesoscopic-3d-species-counting.md) —
  the counterpart that covers species counts after the hand-over time, on
  the mesoscopic mesh.
