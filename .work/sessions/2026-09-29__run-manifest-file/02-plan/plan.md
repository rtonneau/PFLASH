# Implementation Plan

**Session:** run-manifest-file
**Date:** 2026-09-29T09:16:10.130Z
**Estimated effort:** 1 day

## Strategy

Write one `Manifest.json` per Dump (explicit dumps and the `EndOfRun_` safety-net flush), replacing `EnergyDeposit.Txt`. Build it bottom-up: a portable, std-only JSON writer with unit tests first; then per-run records in `RunAccumulator`; then capture of the real gun state and seed on the worker `Run`, merged on the master; then a thin Geant4-facing collector wired into `RunAccumulatorMessenger::WriteAllAndReset`; last, the docs. The full step-by-step plan with code is `.claude/plans/2026-09-29-run-manifest-file.md`; each ticket names its Task there.

## Tickets Overview

1. `01-manifest-writer`: portable `ManifestData` + `ManifestWriter` + test; commit `CONTEXT.md` and ADR 0005.
2. `02-run-records`: `RunAccumulator::AddRunRecord` / `GetRunRecords` + test.
3. `03-beam-capture`: beam and seed on `Run`, snapshot in `PrimaryGeneratorAction`, `RunRecord` built in `RunAction::EndOfRunAction`.
4. `04-manifest-dump`: `OutputDir::GetDirectory`, `RunManifest` collector, dump integration, `EnergyDeposit.Txt` removed, smoke run Serial and MT.
5. `05-manifest-docs`: `.claude-project.json`, `sim-output` skill and `CLAUDE.md` updated.

## Sequencing Rationale

Tickets 1 and 2 are pure logic with Debug unit tests and no Geant4 kernel. Ticket 3 needs the `RunRecord` type from 1 and the accumulator API from 2. Ticket 4 needs everything before it and is the first one verified end to end by a smoke run. Ticket 5 documents the final behaviour, so it goes last.

## Risks & Mitigation

- `G4Random::getTheEngine()->getSeed()` may not reflect `/random/setSeeds`: ticket 4's smoke issues `/random/setSeeds 111 222` between runs and checks the recorded seed changes; fallback is `G4Random::getTheSeeds()[0]`.
- Beam sampled on the first `GeneratePrimaries` of a run: a per-event randomised gun would be under-reported. Out of scope (fixed pencil-beam gun).
- Removing `EnergyDeposit.Txt` breaks anything that parsed it; accepted in ADR 0005, references updated in ticket 5.
- Output is still not bit-reproducible, so the recorded seed documents a run rather than guaranteeing a replay.

## Assumptions

- One manifest per dump, always on, no macro command; JSON with `schemaVersion: 1`, units in keys.
- No git commit hash and no application version field.
- `Run` on the master is created after the master engine state is set, so its constructor sees the run's seed.
- Old `.claude/plans/*` keep their historical `EnergyDeposit.Txt` mentions.

## Token Usage

- **Input:** 26
- **Output:** 24476
- **Cache read:** 1651416
- **Cache creation:** 58581
- **Total:** 1734499
