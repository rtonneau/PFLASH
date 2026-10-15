# Session: run-manifest-file

**Date:** 2026-09-29T08:06:09.039Z
**Status:** Grill phase complete

## Problem Statement

Output data (species, reactions, energy deposit, physics-interaction counts) is written at dump time with nothing recording what produced it: beam particle/energy/position, Chemistry, environment, seed, threads, output folder. A macro can run several simulations, and each one needs its own structured description next to its data. Total energy deposit today lives only in a one-line human-readable `EnergyDeposit.Txt`.

## Context & Constraints

- The unit of "one simulation" for output is a **Dump** (`/run/dumpDataAndReset`, `/run/dumpDataAndResetToDir`, or the exit-time `EndOfRun_` flush; `RunAccumulatorMessenger::WriteAllAndReset`). A dump can span several `/run/beamOn` calls, possibly with different beams.
- The `G4ParticleGun` is private to `PrimaryGeneratorAction` and `/gun/*` commands act on worker-thread copies; the master thread (where dumps run) has no beam knowledge today. `RunAction::EndOfRunAction` (master) calls `RunAccumulator::Accumulate` once per run after `Run::Merge`; nothing there carries the beam yet.
- Chemistry, scavengers and pH are `PreInit`-only, so constant per process; only the beam (and seed) vary between `beamOn` calls.
- New logic classes must be copy-paste portable (no dependency on other project classes; rooted includes under `header/`), like `PhysicsInteractionCounter`. Unit tests are plain `assert` + CTest, built in `build-ninja/` (Debug), kernel-free.
- Output is not bit-reproducible run to run (see project notes), so the manifest documents a run, it does not guarantee a replay.
- `EnergyDeposit.Txt` is referenced in `.claude/.claude-project.json` (expected files), `.claude/skills/sim-output/SKILL.md`, `CLAUDE.md` and the messenger guidance text; older `.claude/plans/*` mentions are historical and stay.
- Already written during the grill (uncommitted): `CONTEXT.md` terms **Dump** and **Manifest**, and `docs/adr/0005-manifest-per-dump.md`.

## Success Metrics

- Every dump (explicit, subfolder, and the `EndOfRun_` safety-net flush) produces exactly one `Manifest.json` (or `<prefix>Manifest.json`, or `<subdir>/Manifest.json`) beside its data files.
- A macro with several `/run/beamOn` at different `/gun/energy` values inside one dump yields one manifest whose `runs[]` lists each with its own beam, events, energy deposit and seed; two dumps in one macro yield two independent manifests.
- `totalEnergyDeposit_eV` equals the value `EnergyDeposit.Txt` gave before; that file is no longer written.
- Serial and MT (`--threads N`) runs report the same beam fields.
- `ManifestWriter` unit test passes in Debug; the `run` smoke test (10 keV e-, `/run/beamOn 2`) finds `Manifest.json` in the output.

## Architecture & Approach

- **One manifest per dump**, always on, no opt-out, no new macro command. Filename/location via `OutputDir::Resolve` (prefix or subfolder applied). `EnergyDeposit.Txt` is removed.
- **JSON**, `schemaVersion: 1` (bumped only when an existing field's meaning changes). Units in keys (`beamEnergy_keV`, `position_um`, `energyDeposit_eV`). Directories stored both as configured (`outputDirAsConfigured`) and absolute (`outputDirAbsolute`), prefix and subfolder as separate fields.
- **Contents.** Top level: schema version, timestamp, Geant4 version, application version (no git commit unless CMake injects it), macro filename, Chemistry, scavengers, pH, chemistry end time, run mode and thread count, output dir/prefix/subdir, `totalEvents`, `totalEnergyDeposit_eV`, `files` (data files written, names relative to the manifest folder, manifest itself excluded, MT per-event files not listed). `runs[]`, one per `/run/beamOn` in the dump: run ID, events, particle, beam energy, position `[x,y,z]`, direction unit vector, that run's `energyDeposit_eV`, seed (recorded per run since `/random/setSeeds` can be issued between runs; documented as seed-as-configured, not a replay guarantee).
- **Beam capture (Q5a):** a worker (thread 0 only) samples the real gun state at run start; it travels in the run record, is merged in `Run::Merge`, and is stored per run in `RunAccumulator` alongside energy. Not mirrored from `/gun/*` on the master.
- **Code shape (Q6b):** portable, stream-only `ManifestWriter` (plain-data `ManifestData` struct in, JSON stream out, no Geant4 dependency, hand-written) + a thin Geant4-facing collector that fills the struct at dump time from `ChemistryRegistry`, `DnaChemistryWorld`, run manager, `OutputDir`, `RunAccumulator`. Called from `WriteAllAndReset`.
- **Failure:** a manifest that can't be opened/written raises a `JustWarning` `G4Exception` (data files are already written); not fatal.
- **Docs:** update `.claude/.claude-project.json`, `sim-output` skill, `CLAUDE.md` (drop `EnergyDeposit.Txt`, add manifest).

## Assumptions & Trade-offs

- Per-dump (not per-`beamOn`) because data files only exist at dump time; the price is a `runs[]` array instead of a flat beam block.
- Removing `EnergyDeposit.Txt` breaks anything that parsed it; accepted, and recorded in ADR 0005. No human-readable total is kept.
- Sampling the gun at run start reports the value at that moment; a per-event randomised gun (energy/position spread) would need a caveat flag. Out of scope: the project has a fixed pencil-beam gun.
- Hand-written JSON avoids a dependency at the cost of writing string escaping ourselves.
- Old `.claude/plans/*` files keep stale `EnergyDeposit.Txt` mentions on purpose.

## Open Questions

- Application version: whether to inject a git commit via CMake (default: leave out).
- Exact carrier for the beam in the run record (new field on `Run` vs. a small struct passed to `RunAccumulator::Accumulate`); implementation detail for the plan.

## Notes

Session: `2026-09-29__run-manifest-file`. Grill decisions Q1-Q16 all agreed by the user. Not a bounded change (several files, new class, tests, docs), so it goes on to `/gps plan`.

## Token Usage

- **Input:** 32
- **Output:** 12427
- **Cache read:** 1177964
- **Cache creation:** 28506
- **Total:** 1218929
