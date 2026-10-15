# Session: Verify feature combinations and add three example macros with standard dir tree

**Date:** 2026-10-08T18:45:36.106Z
**Status:** Grill phase complete

## Problem Statement

dnachem-min runs only IRT_syn followed by a mesoscopic hand-over (ADR 0006 removed SBS). The user needs every chemistry mode usable and configurable from a macro, with and without the O2 scavenger: SBS, IRT_syn only, and IRT_syn + mesoscopic. They also need three example macros, a self-contained directory tree (`sim.exe`, optional `macro/`, `results/` with a manifest and one folder per sub-run), and a check that the features do not interfere with each other.

## Context & Constraints

- **Current behavior:** `PhysicsList` fixes IRT_syn; `DnaChemistryList::ConstructTimeStepModel` is fatal for any other `/process/chem/TimeStepModel`. `TimeStepAction` always hands over to the mesoscopic stage; the end time defaults to 1 s. `sim.cc` prepends `macro/` to the macro argument (cwd-relative). Each dump writes its own `Manifest.json` (ADR 0005); there is no results-level manifest. Default output dir is the cwd.
- **Pain point:** SBS cannot be selected; mesoscopic stage cannot be switched off; the macro must live in `macro/`; no results index; no examples covering the modes.
- **Dependencies:** keep the default behavior (IRT_syn + meso, 1 s) unchanged; keep ADR 0005 per-folder manifests; keep `--dir` / `/run/outputDir` overrides; SBS reference data in `validation/reference/sbs_{water,o2}` (old SBS code at `b26a262^`); G4DNAScavengerProcess bulk reactions must work under SBS.
- **Tech stack:** Geant4-DNA 11.x, C++20, CMake, plain `assert` + CTest unit tests in `build-ninja/` (Debug), runs from `build/` (RelWithDebInfo).

## Success Metrics

- `/process/chem/TimeStepModel SBS|IRT_syn` and `/chem/meso/enable true|false` give three working modes; mesoscopic true with SBS is a fatal error with a clear message; default behavior unchanged.
- All 3 modes x {with O2 21 %, without} run in Serial and `--threads 4` (10 keV e-, 2 events, short end time) with no crash or unexpected warning; `Species.*`/`Reactions.*` always, `SpeciesMeso.*` only with meso on; manifests record the mode.
- `sim.exe <name>` resolves the macro as given, then `<exedir>/<name>`, then `<exedir>/macro/<name>`, error listing the three paths otherwise; default output is `<exedir>/results`; `results/Manifest.json` indexes every dump while each `sub_NN/` keeps its `Manifest.json`.
- `macro/example_sbs.in`, `example_irt.in`, `example_meso.in` run end to end (O2 21 %, 2 sub-runs x 2 events, different seeds, dumps to `sub_01`, `sub_02`).
- SBS regression: water N=40 at 1 us within 3x statistical error of `validation/reference/sbs_water`; O2 at N=300 compared with the N=1500 reference, error bars noted.

## Architecture & Approach

1. **Mode selection.** Re-add `G4DNAMolecularStepByStepModel` registration next to IRT_syn in `DnaChemistryList::ConstructTimeStepModel`, driven by `G4EmParameters::GetTimeStepModel()` (IRT classic stays unsupported/fatal). New `/chem/meso/enable <bool>` (PreInit, default true) in `MesoMessenger`/`MesoSettings`. `TimeStepAction` skips `CompartmentBased()` when meso is off or under SBS. Fatal check at `/run/initialize` for SBS + meso enabled explicitly. `ActionInitialization::Build()` sets end time 1 s with meso, 1 us without. Manifest gets the mode (model, meso on/off); `SpeciesMeso*` not written when meso is off. ADR 0008 supersedes the "SBS removed" part of ADR 0006; CONTEXT.md term **Chemistry mode** (written).
2. **Macro lookup.** Portable pure-logic `MacroResolver` (standard library only) in `core/`, unit-tested, used by `sim.cc`; `RunManifest::SetMacroName` gets the resolved path.
3. **Results layout.** Default output dir is `<exedir>/results` when neither `--dir` nor `/run/outputDir` is given (`OutputDir`). New results index writer (`ResultsIndex`, using `DataNode`/`JsonWriter`) rewrites `<outdir>/Manifest.json` at each dump, listing folder/prefix, events, seed, beam; unit-tested. Per-dump manifests unchanged.
4. **Examples and docs.** Three macros plus updates to CLAUDE.md, `.claude/skills/sim-output`, README for SBS support, macro lookup and results layout.
5. **Verification.** Smoke matrix, then SBS regression against the recorded reference.

## Assumptions & Trade-offs

- Mesoscopic stage is only available after IRT_syn (as in Geant4's UHDR example); no SBS-to-mesoscopic hand-over.
- Scavenger and Chemistry are fixed per process, so with/without O2 are separate runs; examples use O2 and say which line to comment out for the pure-water case.
- Not doing: new physics validation of IRT_syn vs SBS (documented difference stays), pulse structure, other Chemistries beyond a smoke check, changes to existing `beam*.in`.
- SBS regression for O2 uses N=300 (reference N=1500 took 87 min), so its error bars are wider.

## Open Questions

- Whether `G4DNAScavengerProcess` bulk reactions behave under SBS exactly as in the old code (expected, checked by the smoke and regression runs).
- Whether anything in `TimeStepAction`/`StackingAction`/`ReactionCounter` assumes IRT_syn (to be found while implementing).

## Notes

Design settled in the grill: 3 modes (not meso after SBS), G4 command + meso switch, 3 example macros with O2, per-folder manifests plus a results index, exe-dir anchoring, full SBS re-validation. Old macros stay untouched. The `.work/GLOSSARY.md` duplicate of CONTEXT.md was flagged by gps and is left alone.
