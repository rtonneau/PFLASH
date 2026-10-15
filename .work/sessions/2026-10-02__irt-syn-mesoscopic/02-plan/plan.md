# Implementation Plan

**Session:** irt-syn-mesoscopic
**Date:** 2026-10-02T11:17:21.448Z
**Estimated effort:** 4–5 days (about a third of it unattended simulation runs)

## Strategy

First capture the SBS reference while SBS still exists (01). Then de-risk the one unknown that can sink the design, the mesoscopic mesh on the 1 mm box, with a spike that also lands the IRT_syn switch and a bare hand-over (02). The bulk reactions and diffusion classes come next (03), so both stages run the full chemistry. The configurable pieces go in as a pure, unit-tested helper plus a messenger (04), and the new species output as a pure, unit-tested accumulator wired into the dump cycle and MT merge (05). Manifest keys, macro cleanup and the documented scope change follow (06). Validation against the SBS reference and Serial vs MT ends the session (07). Every Geant4-facing behaviour is checked by real runs, Serial or `--threads N`. Pure logic gets ctest tests in `build-ninja/`.

Every ticket follows `.claude/geant4-instructions.md`. The app is built in `build/` (RelWithDebInfo) and the tests in `build-ninja/` (Debug), with MSVC loaded through vcvars64 in the PowerShell tool. `sim.exe` takes the macro filename only, from `build/`. Scratch artifacts go in `.scratch/tests/2026-10-02__irt-syn-mesoscopic/`. Verify Geant4 behaviour against `${DEV_DIR}/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1` (the `.cc` files) and the UHDR example there.

## Tickets Overview

| # | Ticket | Model | Delivers |
|---|---|---|---|
| 01 | sbs-reference-capture | sonnet | `macro/validate_water.in`, `macro/validate_o2.in`; SBS reference dumps committed under `validation/reference/sbs_{water,o2}/` |
| 02 | irtsyn-meso-spike | opus | IRT_syn replaces SBS; per-thread `G4DNAEventScheduler` hand-over at 5 ns with ~6.25 nm cells; molecule conservation checked on the 1 mm box; findings |
| 03 | bulk-reactions-both-stages | opus | Bulk reactions also in the reaction table; `SetReactionType(1)` in PureWater/BoscoloChem; O2 scavenging works in both stages |
| 04 | meso-settings-and-commands | sonnet | Pure `MesoSettings` (pixel count, log time grid) + test; `/chem/meso/{handOverTime,voxelSize,timesPerDecade}`; end time default 1 s |
| 05 | species-meso-output | opus | Pure `MesoSpeciesCounter` + test; `SpeciesMeso.Txt/.csv` per dump, merged across workers, listed in the manifest |
| 06 | manifest-macros-docs-scope | sonnet | Manifest keys; SBS lines removed from macros; CLAUDE.md scope amendment and key files; sim-output skill; catalog headers |
| 07 | validation | sonnet | `analysis/compare_reference.py`; new-model runs vs SBS reference (10%); Serial vs `--threads 4`; `docs/irt-syn-mesoscopic-validation.md`; ADR 0006 accepted |

## Sequencing Rationale

01 must come first: it is the only point where the SBS code still exists to produce the reference. 02 comes next because its result (whether the mesh works on the 1 mm box) decides whether the rest of the plan stands. If the spike blocks, the session stops before 03–07. 03 gives both stages the full chemistry before anything is scored or validated. 04 comes before 05 because the scorer's record times come from `MesoSettings::LogTimeGrid`. 06 needs the final command and key names from 04 and 05. 07 needs the whole feature.

## Risks & Mitigation

- **int overflow in `G4DNAEventScheduler::Voxelizing`** (`fPixel³` in a 32-bit `G4int` above ~1290 pixels) on the 1 mm box: ticket 02 checks molecule totals through every mesh change and ends BLOCKED with numbers if they drift.
- **Bulk reactions in the reaction table disturbing the particle stage:** there are no bulk-species tracks, so IRT_syn can't pair with them. Ticket 03 checks that bulk species never appear in `Species.*` or `SpeciesMeso.*` and that `Reactions.*` has no bulk partner.
- **Long 1 s runs:** ticket 02 measures wall time per event. Validation compares at ≤1 µs against the reference with `/scheduler/endTime 1 us`, plus one long run per pair for the report.
- **MT:** the scheduler is per thread (owned by the thread-local `TimeStepAction`, as in UHDR). Counts are merged in `Run::Merge` like the other counters, and ticket 07 checks Serial vs `--threads 4`.
- **Statistics vs the 10% tolerance:** ticket 01 sizes the event count for about 3% error on G(1 µs). The new runs use the same macros.
- **The UHDR example is a prototype:** validation, not the example, is the acceptance gate.

## Assumptions

- Reference dumps are small (`Species_nt_species.csv` + `Manifest.json`) and are committed under `validation/reference/`.
- The mesoscopic record times include the hand-over time and the end time.
- `ParticleBasedCounter()` is not needed: `Species.*` keeps using `G4MoleculeCounter` for the particle stage.
- `RunAccumulator` owns the new `MesoSpeciesCounter` the same way it owns `PhysicsInteractionCounter`.
- The session branch `feat/irt-syn-mesoscopic` is created from `feat/irt` at `1d047c4`. The grill docs commit lands on it first.

## Token Usage

- **Input:** 8
- **Output:** 15704
- **Cache read:** 1483056
- **Cache creation:** 24799
- **Total:** 1523567
