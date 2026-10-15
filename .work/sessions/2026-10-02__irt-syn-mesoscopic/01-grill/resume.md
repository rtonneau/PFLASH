# Session: irt-syn-mesoscopic

**Date:** 2026-10-02T10:34:37.552Z
**Status:** Grill phase complete

## Problem Statement

dnachem-min runs each event's chemistry with the step-by-step (SBS) model only, up to 1 µs. SBS is hard-coded in `DnaChemistryList::ConstructTimeStepModel`, `DnaChemistryList::ConstructProcess` (`G4DNABrownianTransportation`) and `PhysicsList`, and the end time is hard-coded in `ActionInitialization::Build()`. We want long-time yields (µs to s) with O2 as a consumable pool, and later (stage 2) dose-sized multi-track runs without rework. The plan replaces SBS with the Geant4-DNA UHDR-example scheme: an IRT_syn particle-based stage up to a hand-over time, then the compartment-based mesoscopic stage (`G4DNAEventScheduler`, Gillespie on a cell mesh). Classic IRT was explored and dropped: `G4DNAIRT` builds fake bulk-partner tracks that the molecule counter counts as species, and it can't consume a scavenger.

## Context & Constraints

Geant4 11.4.1 facts, checked in the source:
- **UHDR example.** `TimeStepAction::UserPostTimeStepAction` hands over when the global time reaches T1 (5 ns, or pulse end + 5 ns). It then calls `G4DNAEventScheduler::SetStartTime`, `SetChangeMesh(true)`, `Initialize(boundingBox, pixel)` and `Run()`. `ParticleBasedCounter()` is called in `UserPreTimeStepAction`. One scheduler is owned by each (thread-local) `TimeStepAction`.
- **UHDR mesh.** `SetInitialPixel` hard-codes the pixel count per box size: 3.2 µm / 512 gives about 6.25 nm per cell. Any other box is fatal.
- **Mesh storage and coarsening.** The mesh (`G4DNAMesh`) is sparse: only occupied cells are stored, under integer x/y/z indices. It coarsens by halving the pixel count once the cell transfer time drops below the time step.
- **Overflow risk.** `G4DNAEventScheduler::Voxelizing` computes `numberOfBoxes = fPixel*fPixel*fPixel` as a 32-bit `G4int`. That overflows above about 1290 pixels, and a 1 mm box at 6.25 nm needs about 131072.
- **Bulk reactions in Gillespie.** `G4DNAGillespieDirectMethod` reads bulk reactions from the **reaction table**. `FindScavenging` takes the partner's count per cell from `G4DNAScavengerMaterial` (water as 55.3 M). Reaction types 6, 7 and 8 are equilibria (`G4ChemEquilibrium`). UHDR therefore registers every bulk reaction both as `G4DNAScavengerProcess` and in the reaction table (`ChemPureWaterBuilder::WaterScavengerReaction`).
- **Mesoscopic outputs.** Species counts come only at times registered with `AddTimeToRecord` (`GetCounterMap()`), plus `G4UserMeshAction` callbacks. There is no hook that says which reaction fired.
- **Diffusion class.** IRT_syn needs partially diffusion-controlled reactions marked `SetReactionType(1)`, as option3's Type II/IV reactions and UHDR's `ChemOxygenWaterBuilder::SetReactionType` do. `SetReactionType(1)` recomputes the reaction radii.

Project facts: Chemistries supply `buildReactions` and `buildBulkReactions` (plain-data `ChemistryTypes::BulkReactionList` with `partner`, `rate`, `products`, `reactionType`). `DnaChemistryList::RegisterBulkReactionProcesses` registers them as `G4DNAScavengerProcess`, and `G4DNAScavengerMaterial` is installed in `ConstructProcess`. `ScoreSpecies` uses `G4MoleculeCounter`, `ReactionCounter` is fed by `TimeStepAction::UserReactionAction`, and `RunManifest` is the only place listing manifest keys. `macro/beam.in` contains `/process/chem/TimeStepModel SBS`, and `beam_02.in` is an "SBS variant".

Constraints:
- **Simulation runs** may use `--threads N`. MT is supported and must be validated.
- **Build and run** follow `.claude/geant4-instructions.md`. The app is built in `build/` (RelWithDebInfo), tests in `build-ninja/` (Debug).
- **Geometry** stays one homogeneous water box: no G4Vox or voxel geometry. The cell mesh exists only inside the chemistry.
- **No stage-2 features now:** pulses and multi-track dose runs are out.

## Success Metrics

- Every event runs IRT_syn up to the hand-over time (default 5 ns, `/chem/meso/handOverTime`), then the mesoscopic stage up to the end time (default 1 s, still overridable with `/scheduler/endTime` after `/run/initialize`). No SBS code path is left.
- Molecules are conserved through the hand-over and through every mesh coarsening on the default 1 mm box, or on the box the spike settles on.
- `Species.*` holds the particle-stage counts. A new `SpeciesMeso.*` holds the mesoscopic counts on a log grid (default 10 per decade, `/chem/meso/timesPerDecade`). `Reactions.*` holds the particle-stage reactions only, and the docs say so.
- With O2 set, O2⁻ forms and bulk O2 is consumed in both stages. Bulk species never appear in either species output.
- `Manifest.json` records the chemistry model, hand-over time, voxel size, pixel count, time-grid density and end time.
- **Validation (analysis script):**
  - G(≤1 µs) for e_aq, °OH, H2O2 and H2 is within 10% of SBS reference dumps recorded before SBS is removed (10 keV, PureWater with and without O2).
  - A Serial run and a `--threads 4` run agree within statistics.
  - Long-time G values are compared by eye with the literature and the UHDR example in a docs report.
- All unit tests pass in `build-ninja/`. Project `CLAUDE.md` and ADR 0006 record the scope change.

## Architecture & Approach

1. **Spike (ticket 1):** a minimal IRT_syn + mesoscopic run on the 1 mm box (10 keV, a few events) using a fixed voxel size, logging the molecule totals before and after the hand-over and at each mesh change. If the counts aren't conserved (overflow) or the run fails, the session stops and decides between a smaller default box and a mesh sub-domain.
2. **Reference capture:** while SBS still exists, run the validation macros under SBS and save the dumps as reference data (location decided in the plan).
3. **Particle-based stage:** `DnaChemistryList` registers `G4DNAIndependentReactionTimeModel` and keeps `G4DNABrownianTransportation`. SBS is removed from `DnaChemistryList`, `PhysicsList` (which sets IRT_syn) and the macros.
4. **Bulk reactions:** `DnaChemistryList` registers each Chemistry's `BulkReactionList` both as `G4DNAScavengerProcess` (as today) and as reaction-table entries against the bulk species, with rates and reaction types unchanged. `G4DNAScavengerMaterial` stays per event.
5. **Diffusion class:** the PureWater and BoscoloChem catalogs call `SetReactionType(1)` directly on their partially diffusion-controlled reactions, checked against option3 and UHDR.
6. **Mesoscopic stage:** a per-thread `G4DNAEventScheduler` owned by `TimeStepAction`.
   - It hands over at `/chem/meso/handOverTime` (PreInit, default 5 ns).
   - The initial pixel count is a power of 2 chosen so the cell size is close to `/chem/meso/voxelSize` (default 6.25 nm) for the current box.
   - The record times form a log grid from the hand-over to the end time (`/chem/meso/timesPerDecade`, default 10).
   - The end time default changes to 1 s in `ActionInitialization`.
7. **Scoring:** a new mesoscopic species scorer (`SpeciesMeso.Txt` / `.csv`) accumulates the scheduler's counter map per event, merges across workers, and flushes and resets with `/run/dumpDataAndReset` like the other outputs. `Reactions.*` stays fed by `UserReactionAction`, which only fires in the particle stage.
8. **Manifest and docs:**
   - `RunManifest` gains the new keys.
   - CLAUDE.md (scope rules, key files, macro commands, end-time note), the sim-output skill and the catalog headers are updated.
   - ADR 0006 becomes *accepted* after validation.
9. **Validation:** validation macros, an analysis script (Python, conda env `GEANT4_py311`) that compares against the SBS reference, the Serial vs MT check, and a docs report.

## Assumptions & Trade-offs

- **SBS is replaced, not kept selectable:** the code is simpler, and reference dumps preserve a regression baseline.
- **The O2 pool stays per event:** stage-2 depletion will come from several tracks in one event, never from state shared across events.
- **Reactions after the hand-over aren't counted** (no Geant4 hook).
- **Species output is split into two files:** analyses join them.
- **Assumed (not separately asked):** manifest keys as listed, command names under `/chem/meso/`, nothing built for stage 2 beyond the existing configurable box, BoscoloChem on the same paths.
- `G4DNAEventScheduler` coarsening and equilibria come from Geant4 as they are. The UHDR README calls the example "a prototype", so validation carries the weight.

## Open Questions

- Does the mesoscopic stage handle the 1 mm box (int overflow above ~1290 pixels)? The spike answers this. If not: a smaller default box or a sub-domain mesh.
- Exactly which reactions are partially diffusion-controlled in each catalog (decided against option3 and UHDR in the catalog ticket).
- How many events the validation needs for about 3% statistical error at 1 µs, and how long a 1 s run takes in wall time (measured in the spike and validation tickets).

## Notes

- **Rollback:** the earlier classic-IRT session (`2026-10-01__irt-time-step-model`) was rolled back. Its branch was deleted, and the code is back to `1d047c4` on `feat/irt`.
- **CONTEXT.md is updated:** new terms **Particle-based stage**, **Mesoscopic stage** and **Hand-over time**. The **Scavenger** entry now says it is consumed in both stages, per event. **Bulk species** now says it is never an individual molecule nor in the species output. **Bulk reaction** now says it applies in both stages and is not counted.
- **ADR** `docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md` is written with status *proposed*.
- The project `CLAUDE.md` rules on voxelization and UHDR models are amended as part of this feature.

## Token Usage

- **Input:** 64
- **Output:** 27724
- **Cache read:** 10589415
- **Cache creation:** 43930
- **Total:** 10661133
