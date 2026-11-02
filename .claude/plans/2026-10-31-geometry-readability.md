# Geometry readability pass

Standing approval covers this concern. Apply only after physics verification
and commit. Keep homogeneous geometry, process/scorer order and all defaults.

## Clarity

- src/geometry/DetectorConstruction.cc:62 and header:47: OtherMaterial is a
  private nonvirtual method with no callers (only declaration, definition and
  commented call). Remove it and its stale alternative-material example.
- DetectorConstruction.cc:149: SetMaterial mixes material updates and lookup
  failure diagnostics (~55 lines). Pure-move the existing scoped else body
  into ReportMissingMaterial(const G4String&) const. Keep both warning/fatal
  guards, exception-origin strings, messages and formatting exactly.
- DetectorConstruction.cc:83: remove stale code and redundant argument-label
  comments; preserve construction and registration order and all values.
- DnaChemistryWorld.cc:85: name 55.3 waterConcentration_M. Preserve the exact
  arithmetic expression and the pKw value.
- ScavengerSpec.cc:10: Parse (~65 lines) could extract concentration/unit
  validation. Defer that extraction: current early-return error ordering is
  clear and this pass can improve its local names without moving error paths.

## Comments

- ScavengerMessenger.cc:23: missing required tokens can be rejected by Geant4
  before the parser. Narrow the current all-malformed-input claim.
- ScavengerMessenger.cc:29: world is shared with workers; command configuration
  is on the master with broadcast disabled, not master-only world storage.
- All eight .cc/.hh files: purpose, dependency and thread/lifetime headers.
- Preserve SetHalfBox's borrowed-boundary warning. Explain scorer registration
  order and the shared geometry/per-thread scorer lifecycle.

## Naming

- DetectorConstruction.cc:92-94/header:55-57: private side lengths gain _mm;
  mat, primitivSpecies and sstr become material, speciesScorer and message.
- DnaChemistryWorld: fHalfBox and halfBox gain _mm; table/conf become
  moleculeTable/configuration. Public method names and UI strings stay fixed.
- ScavengerSpec.cc:12,29,45: iss/used/factor become input/parsedCharacters/
  molarityPerUnit. ScavengerMessenger's local err becomes error.

## Documentation

- All four public headers: parameter units, ownership, initialization rules,
  preconditions and bool+string versus G4Exception failure behavior.
- create-a-macro-command.md:71-80: replace stale geometry line anchors.
- macro-command-reference.md:48-63,164-177: correct parser rejection and
  shared-world/broadcast claims; retain actual command names/defaults.

## Source evidence

Local Geant4 11.4.1 .cc sources reviewed: DNA management
G4DNABoundingBox.cc:43-53 copies the initializer list into contiguous fields;
its header declares xhi,xlo,yhi,ylo,zhi,zlo. G4DNAScavengerProcess.cc:44-54
borrows the box; G4DNAScavengerMaterial.cc:43-47 borrows the world.
G4MoleculeTable.cc:106-124 implements missing-configuration failure behavior.
UHDR ChemistryWorld.cc:81-99,125-141 supplies the concentration convention,
Henry-law constant and water/pKw constants. G4UIcommand.cc:125-223 validates
required tokens before messenger dispatch. G4WorkerRunManager.cc:146-177
shares the world and invokes ConstructSDandField; G4RunManager.cc:653-670
invokes the master/Serial hook.

## Deferred issues and risk

- SetHalfBox accepts NaN; SetpH has no finite/range validation; existing bulk
  entries are not cleared by ConstructChemistryComponents. No fixes here.
- Global collection ID used as primitive index remains the scoring issue
  already reported. Existing console diagnostics remain byte-for-byte to
  preserve verbosity/output; no ad hoc logging is added.
- Public getter names and manifest/UI keys stay unchanged.
- Extraction changes only a diagnostic call boundary. Removal is limited to
  proven uncalled private code, with no virtual table/class-layout change.

## Verification required

Debug build/17 ctest tests with debug HDF5 PATH, RelWithDebInfo build, then
the frozen 63-invocation/252-event SBS/IRT/mesoscopic matrix against 1fcf8ba.
Audit tokens after reversing names/constant, removing the dead function from
the old tokens, and inlining the new helper. Record actual results before commit.

## Verification in progress

- Debug and RelWithDebInfo builds passed; ctest 17/17 passed in 1.03 s with
  vcpkg debug/bin first on PATH. All eight token audits and git diff --check pass.
- The 63-invocation matrix is running against the original baseline.
- Normal mode examples do not exercise the unknown-material diagnostic branch;
  its extraction was checked by source review and token inlining, not a dedicated
  invalid-material runtime test. No such runtime test is claimed.

## Completed verification and directory deliverable

- Debug and RelWithDebInfo builds passed; ctest 17/17 passed, 1.03 s, with
  the required debug DLL path. All eight token audits and git diff --check pass.
- All 63 invocations/252 events passed against original baseline 1fcf8ba:
  matching structures/time grids, 228 scalar observables, 3307 descriptive
  curve points, zero Holm-adjusted flags at alpha 0.01. Minimum raw p =
  0.09381174; adjusted p = 1. Only the known WrongResolution warning occurred.
- H2O2 endpoints (baseline -> after; 99% CI of difference): SBS G-value
  0.521750 -> 0.525750, [-0.027721, 0.035721]; IRT 0.770250 -> 0.751750,
  [-0.056808, 0.019808]; mesoscopic mean count 82.7875 -> 82.1375,
  [-3.958150, 2.658150].
- Exact TrackLengths.csv/PhysicsInteractions.csv matches, each file:
  24/42 SBS, 16/42 IRT and 25/42 mesoscopic. Exact equality was not required.
- Evidence: .scratch/readability-2026-10-29/geometry/{comparison.json,
  statistics.csv,curves.csv,matrix-status.json}, geometry-ctest.log and
  geometry-token-audit.json. The retained geometry-matrix-sim.exe is the build
  used for these runs. No C++ changes followed the validated build.

Files: DetectorConstruction, DnaChemistryWorld, ScavengerMessenger and
ScavengerSpec .cc/.hh pairs; create-a-macro-command and macro-command-reference
recipes; this plan. Categories: one diagnostic helper, proven dead private
method removal, units/local names, source-checked lifecycle rationale, API
contracts and related documentation corrections.
Risk: diagnostic call boundary changes, with guards/lifetimes/messages/order
preserved. Its failure branch was audited rather than runtime-exercised.
Statistical non-detection supports preservation, not proof of equality.
All suspected bugs/public renames above remain deferred.
