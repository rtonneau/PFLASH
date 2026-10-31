# Behaviour-preserving readability review — 2026-10-31

Completed on `readability/behaviour-preserving-pass`, a dedicated branch without a worktree.
Baseline: `1fcf8baff854664b24ea8f7e28d8342af21bd2e1`. The seven source concerns have separate commits, in the requested order.
No intended physics, logic, RNG-order, default, macro-command, output-name or manifest-key change.
Suspected defects and public-contract renames remain deferred below.

## Changes by directory

Paired names below mean `.cc` under `src/<concern>/` and `.hh` under `header/<concern>/`.
Header-only names are listed separately. Related documentation and detailed findings are in each linked plan.

### core

Parser branch helpers; descriptive locals; output-path/logging contracts.

Files: `ArgParser.{cc,hh}`, `DnaLogger.{cc,hh}`, `DnaLoggerMessenger.{cc,hh}`, `MacroResolver.{cc,hh}`, `OutputDir.{cc,hh}`, `OutputDirMessenger.{cc,hh}`.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-29-behaviour-preserving-readability.md).

### scoring

Dump/setup helpers; units and local/private names; writer/counter ownership and failure contracts.

Files: `DataNode.{cc,hh}`, `JsonWriter.{cc,hh}`, `MesoSpatialFile.{cc,hh}`, `MesoSpeciesCounter.{cc,hh}`, `PhysicsInteractionCounter.{cc,hh}`, `PreChemicalFiles.{cc,hh}`, `PrimaryKiller.{cc,hh}`, `ReactionCounter.{cc,hh}`, `ResultsIndex.{cc,hh}`, `RunAccumulator.{cc,hh}`, `RunAccumulatorMessenger.{cc,hh}`, `RunManifest.{cc,hh}`, `ScoreSpecies.{cc,hh}`, `TrackLengthTable.{cc,hh}`.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-30-scoring-readability.md).

### chemistry

Two diagnostic helpers; unit-bearing locals/constants; reaction/stage rationale and catalog/API documentation.

Files: `BuiltInChemistries.{cc,hh}`, `ChemUtils.{cc,hh}`, `ChemistryRegistry.{cc,hh}`, `ChemistrySelectMessenger.{cc,hh}`, `DnaChemistryList.{cc,hh}`, `MesoMessenger.{cc,hh}`, `MesoSettings.{cc,hh}`, `ReactionTableDump.{cc,hh}`, `TimeStepAction.{cc,hh}`, `catalog/BoscoloChemReactions.{cc,hh}`, `catalog/Farokhi2023Reactions.{cc,hh}`, `catalog/PureWaterReactions.{cc,hh}`, `catalog/Tonneau2025Reactions.{cc,hh}`, `catalog/Tonneau2025Table.{cc,hh}`.
Header-only: `header/chemistry/ChemistryTypes.hh`, `header/chemistry/ScavengerReactionAccess.hh`.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-31-chemistry-readability.md).

### actions

Local/private names and energy units; event/run/reset/merge lifecycle documentation.

Files: `ActionInitialization.{cc,hh}`, `EventAction.{cc,hh}`, `PrimaryGeneratorAction.{cc,hh}`, `Run.{cc,hh}`, `RunAction.{cc,hh}`, `StackingAction.{cc,hh}`, `SteppingAction.{cc,hh}`, `TrackingAction.{cc,hh}`.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-31-actions-readability.md).

### physics

Local cut name with units; initialization ordering and option2 documentation.

Files: `PhysicsList.{cc,hh}`.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-31-physics-readability.md).

### geometry

One pure-move material-error helper; unused private method removal; dimensions/concentration names and boundary contracts.

Files: `DetectorConstruction.{cc,hh}`, `DnaChemistryWorld.{cc,hh}`, `ScavengerMessenger.{cc,hh}`, `ScavengerSpec.{cc,hh}`.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-31-geometry-readability.md).

### sim

CLI registration and lazy-root helpers; descriptive locals; startup/timing/macro-order documentation.

Files: `sim.cc` and its directly related `CLAUDE.md` reference.
[Findings, source evidence, verification and deferrals](../../.claude/plans/2026-10-31-sim-readability.md).

The non-obvious notes are retained: ConvertIndex integer-overflow cap, irreversible SetReactionType(1) derived-state changes, the observed GetValueOf Debug-library memory-corruption warning, and the known baseline WrongResolution warning (with its source-checked scope).

## Verification

For each source concern: MSVC Debug build in `build-ninja/`, all 17 ctest tests with vcpkg `debug/bin` first on PATH, then RelWithDebInfo `sim` build in `build/`.
All seven Debug and release builds succeeded. The frozen original baseline and every concern ran the three exact mode examples plus 20 seeded repetitions per mode: 63 invocations / 252 events per phase, 10 keV electrons, two `/run/beamOn 2` sub-runs per invocation. These examples use PureWater with dissolved O2 at 21 %; the matrices do not cover every catalog Chemistry.
All runs returned 0, printed the completion/dump markers, and passed structure/time-grid validation. The only observed Geant4 warning was the baseline `WrongResolution` warning.

| Concern | Debug ctest | Sim invocations | Scalar observables | Descriptive curve points | Minimum raw / Holm p | Flagged |
|---|---|---|---:|---:|---:|---:|
| core | 17/17 (2.83 s) | 63/63 | 227 | 3272 | 0.0133063 / 1 | 0 |
| scoring | 17/17 (1.03 s) | 63/63 | 227 | 3270 | 0.0441308 / 1 | 0 |
| chemistry | 17/17 (1.35 s) | 63/63 | 228 | 3308 | 0.00152563 / 0.347844 | 0 |
| actions | 17/17 (0.87 s) | 63/63 | 229 | 3309 | 0.0708275 / 1 | 0 |
| physics | 17/17 (1.03 s) | 63/63 | 226 | 3271 | 0.0209222 / 1 | 0 |
| geometry | 17/17 (1.03 s) | 63/63 | 228 | 3307 | 0.0938117 / 1 | 0 |
| sim | 17/17 (1.20 s) | 63/63 | 229 | 3310 | 0.0168136 / 1 | 0 |

Statistics compare each concern with the original baseline, using 20 invocation-level replicates (80 events) per mode per side. Scalar differences use Welch tests and unadjusted 99% confidence intervals; p-values use Holm correction at familywise alpha 0.01. Full-curve points are descriptive, not independent significance tests. Counts can vary because stochastic channels absent in one sample are included with zero counts.
No regression was detected at this sample size. This is not a formal proof of identical distributions or a guarantee for rare channels. Independent token audits reverse approved names/constants and inline extracted helpers; all source/header audits pass. Actions and physics additionally produced byte-identical release `.text` sections relative to their preceding builds.

Supplemental scoring/chemistry checks covered spatial output and two-thread IRT structurally. Chemistry also ran before/after Debug-verbosity hand-over checks: all hand-over totals conserved, no DRIFT. The chemistry plan records an initial failed harness macro lookup and the successful absolute-path rerun. These checks are not MT statistical equivalence tests.

Final comment/documentation verification: Debug/release builds passed; ctest 17/17 (2.11 s). All three exact examples ran again into fresh directories and matched baseline output structures. All 54 macro active-command sequences and all 17 test token sequences are unchanged. Local Markdown file-target checks and git diff --check passed.
The final release build was already up to date, and its complete executable SHA-256 matches the retained sim-matrix binary. No extra full statistical matrix was run for prose, macro comments or test headers. GUI, production-sized macros, and a dedicated unknown-material error-branch runtime test were not run. The extracted material diagnostic was audited as a pure move.

Raw local evidence is retained in `.scratch/readability-2026-10-29/`: baseline and per-concern logs, statuses, manifests, `statistics.csv`, `curves.csv`, `comparison.json`, token audits, build/ctest logs and final smoke results. The scratch data are ignored by Git.

## Documentation and macro completion

Maintained docs now distinguish command replay from dispatch, shared ownership from safe concurrent mutation, and consumable scavengers from buffers. Recipes use current names and file/method references. Output docs describe the existing cwd fallback. Build instructions include the Debug HDF5 DLL path. ADR decisions are unchanged; ADR 0004 has a factual source-scoped addendum.
All 54 `macro/*.in` files have scenario/order comments, with active commands unchanged. All 17 `test/*.cc` files have dependency/thread expectations, without changed assertions or test logic.
[Detailed documentation findings and verification](../../.claude/plans/2026-10-31-documentation-readability.md).

## Deferred decisions and behavior risks

- Core: unchecked integer overflow, retained/partial parser state, existing logger fallback and stale pre-chemical UI guidance.
- Scoring/actions: unhandled `/primaryKiller/minKineticE`, its command lifetime issue, disabled all-events scorer branch inconsistencies, collection-ID/primitive-index coupling, unchecked/nontransactional I/O, misleading public `GetSumDose` name (energy in MeV), multi-primary gun reporting and unused state.
- Chemistry: nonpositive fixed-bin width can fail to terminate; kobs == kdiff singularity; finite/extreme-grid validation; reaction-dump first-order units/cache lifetime; public time/size naming and deeper routing extractions.
- Geometry: NaN half-side acceptance, unchecked pH, bulk-map entries not cleared on reconstruction, existing console diagnostics and the shared scorer-index issue. No voxelized geometry was introduced.
- sim: argv[1] macro restriction, ignored ApplyCommand status, early-return cleanup, unused object lifetimes and missing GUI macro. Larger lifecycle extractions remain deferred.
- Macro discrepancies: historical validation macros inherit IRT_syn plus mesoscopic instead of explicitly selecting SBS; the meesungnoen and 0pct_elossmin_2p5pct electron macros have identical commands. Changing these setups requires a separate behavior decision.
- `reactions.in` passes `10,` through `/control/foreach` into `/gun/energy`; Geant4 rejects that as a double. This was source-verified, not runtime-reproduced; its command remains unchanged and its comment now identifies the issue.
- Configuration remains unchanged: JSON smoke 25 keV/10 versus this task's 10 keV/2; missing TrackLengths outputs and WrongResolution entry; unverified >=11.0 compatibility and overly narrow macroArgIsFilenameOnly description.
- Documentation left for decision: two app-workflow SVGs still name the older EM constructor; an analysis-script comment has a stale numeric ScoreSpecies anchor; the external g4run tool status is unverified. Historical review/literature/reference numerical claims were not re-certified.
- New helpers preserve existing scope, guards, messages and statement order. Their call boundaries are the principal structural risk; the only code deletion is an uncalled private material helper. No suspected bug, public contract, dependency boundary, reaction rate/product or default was changed.

The pre-existing hook log and two untracked Meesungnoen literature-comparison files were preserved and excluded from the commits.
