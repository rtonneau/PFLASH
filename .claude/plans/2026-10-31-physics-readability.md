# Physics readability pass

Standing user approval covers this concern, its verification and commit.
Apply after actions is verified and committed; do not change physics choices.

## Clarity

- src/physics/PhysicsList.cc:17-18: stale commented constructor option; remove.
- No function exceeds the extraction threshold or needs a responsibility split.
  Preserve the unused legacy include to avoid incidental dependency changes.

## Comments

- src/physics/PhysicsList.cc:48: explain why Chemistry follows transport and
  EM registration: DnaChemistryList inspects/extends existing EM processes.
- Both .cc/.hh: short purpose, dependency and shared-list lifecycle headers.
- Preserve the current time-step model explanation; no model/default edits.

## Naming

- src/physics/PhysicsList.cc:22: defaultCut -> defaultCut_mm, reflecting
  Geant4 internal length units. Keep the exact 1 * nanometer expression.
- Public types, methods, commands and output contracts remain unchanged.

## Documentation

- header/physics/PhysicsList.hh:26-38: document all public construction,
  lifecycle and accessor contracts, ownership, preconditions and failures.
- CLAUDE.md:37: constructor is G4EmDNAPhysics_option2, not G4EmDNAPhysics.

## Evidence and constraints

Geant4 11.4.1 .cc source reviewed in the local source installation:
G4EmDNAPhysics_option2.cc:79-86 (standard EM and option2 DNA setup),
G4VUserPhysicsList.cc:349-372 (default range cut), G4ProductionCutsTable.cc:398
(energy range), and run-manager shared physics-list/worker initialization.
Project DnaChemistryList::ConstructProcess supplies the ordering rationale.
No logic, arithmetic, registration order, dependency or default change.

## Verification and deliverable

Run Debug build and ctest with vcpkg debug/bin first on PATH, then
RelWithDebInfo build. Run the same frozen 63-invocation, 252-event matrix
(three examples plus 20 replicates per mode, 10 keV electrons, beamOn 2)
against original baseline 1fcf8ba. Reverse the sole local rename and audit
source/header tokens. Record actual results before this concern's commit.

## Completed verification and directory deliverable

- Debug and RelWithDebInfo builds passed. ctest: 17/17 passed in 1.03 s,
  with vcpkg debug/bin prepended. Both source/header token audits passed.
- Release PE .text is byte-identical to the pre-physics build.
- All 63 invocations/252 events passed against original baseline 1fcf8ba:
  matching structures/time grids, 226 scalar observables and 3271 descriptive
  curve points, zero Holm-adjusted flags at alpha 0.01. Minimum raw p =
  0.02092222; adjusted p = 1. Only the known WrongResolution warning appeared.
- H2O2 endpoints (baseline -> after; 99% CI of difference): SBS G-value
  0.521750 -> 0.528875, [-0.021300, 0.035550]; IRT 0.770250 -> 0.759500,
  [-0.047636, 0.026136]; mesoscopic mean count 82.7875 -> 82.9000,
  [-3.282943, 3.507943].
- Exact TrackLengths.csv/PhysicsInteractions.csv matches, each file:
  12/42 SBS, 16/42 IRT, 30/42 mesoscopic. Exact equality was not required.
- Evidence: .scratch/readability-2026-10-29/physics/{comparison.json,
  statistics.csv,curves.csv,matrix-status.json}, physics-ctest.log,
  physics-token-audit.json and physics-code-sections.json. git diff --check passes.

Files: src/physics/PhysicsList.cc, header/physics/PhysicsList.hh, CLAUDE.md,
and this plan. Categories: one local unit-bearing name, source-checked rationale,
public API/lifetime comments and corrected constructor documentation.
Risk: no executable operations changed beyond an identifier; statistical
non-detection supports preservation but does not prove distributional equality.
No physics bug fixes or public-contract renames; unused legacy include retained.
