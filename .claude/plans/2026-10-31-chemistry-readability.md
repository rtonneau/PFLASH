# Chemistry readability pass

Status: findings reported before edits; covered by the user's approval of all
remaining readability modifications and commit/push/PR authorization.
Base: `7be9998`. Scope: `src/chemistry` (including catalog), matching headers,
and directly related documentation. Unrelated working-tree files remain unstaged.

## Clarity

- `src/chemistry/TimeStepAction.cc:527`: hand-over orchestration includes two
  long diagnostic blocks. Move those scoped bodies into
  `CountAndReportParticleTracks` and `ReportInitializedMesh`, at the same guarded
  call sites. Keep timer, counter muting, scavenger merging, mesh initialization,
  sampling and output operations in their original order.
- `src/chemistry/TimeStepAction.cc:388`: initialization spans SBS stepping,
  record-grid setup and timing. Proposed further helpers: `ConfigureSbsSteps`
  and `ConfigureRecordTimes`; defer in this pass because their conditions and
  lifetimes are already explicit and the hand-over extraction gives the more
  useful separation without moving initialization across scopes.
- `src/chemistry/DnaChemistryList.cc:383`: lengthy process construction. Proposed
  `RegisterMolecularTransport` helper; defer moving Geant4 process registration
  in this pass. Tighten ordering comments and local names instead.
- `src/chemistry/catalog/PureWaterReactions.cc:40`,
  `BoscoloChemReactions.cc:45`, `Tonneau2025Reactions.cc:187,245`: long builders.
  The first two are ordered declarative networks; retain their data together.
  Proposed Tonneau routing helpers are deferred to keep route/error paths fully
  visible. No rate, product, reaction type or placement changes.
- `src/chemistry/MesoSettings.cc:53`: name the dimensionless endpoint tolerance,
  preserving its type and expression. No new validation.

## Comments

- All 14 chemistry `.cc` and 16 `.hh`: add accurate purpose, dependency and
  threading notes, retaining useful existing explanations.
- `src/chemistry/TimeStepAction.cc:433`: RecordTime actually checks `end()`;
  replace the unconditional-dereference claim with the checked ResetCounter
  warning/LastRegisterForCounter behavior. Preserve the current nonempty grid.
- `src/chemistry/DnaChemistryList.cc:564`: above the diffusion limit can produce
  a negative activation rate; equality is a singular denominator, not <= 0.
  Correct the comment only; preserve the warning condition and message.
- `header/chemistry/ChemistryTypes.hh:47`: retain SetReactionType(1)
  irreversibility, explaining that setting type 0 does not restore derived
  radii/rates. Remove the unsupported blanket claim that Geant4 allows only one
  scavenger process per molecule; this driver registers one per list entry.
- `header/chemistry/MesoSettings.hh:16`: retain ConvertIndex's integer-overflow
  cap and qualify it for the scheduler's halving coarsening operation.
- Catalog source files: correct nonexistent `G4DNAMolecularReactionData.cc`
  references to `G4DNAMolecularReactionTable.cc`; qualify dependency claims.
- `src/chemistry/catalog/Tonneau2025Table.cc:1`: repair control characters in
  Doxygen commands. Historical paper-transcription claims are not re-certified
  by a readability review; retain the scientific uncertainties explicitly.

## Naming

- Catalog builders: `rd` -> `reactionData`, `p` -> `moleculeConfiguration`,
  lambda `a/b/k` -> `reactant1/reactant2/rate_M_inv_s_inv` where applicable;
  bulk `M/cW` -> `secondOrderRateUnit/waterConcentration_M`. Chemical-symbol
  variable names stay recognizable and match the surrounding network.
- `header/chemistry/TimeStepAction.hh:93`: private wall time gets `_s`.
- MesoSettings pixel-search temporaries get descriptive local names.
- Public `MesoSettings::Values::handOverTime/voxelSize` unit-suffixed renames
  would require changes in other concerns; defer them, document ns/mm instead.
- No Geant4 API, macro command, output name, diagnostic string, or manifest key
  is renamed. Existing direct G4cout diagnostics are preserved, not expanded.

## Documentation

- Public chemistry headers: describe parameters/units, initialization/thread
  preconditions, return lifetimes and failure mechanisms (exceptions versus
  false + error string). Delete comments that only describe C++ syntax.
- `header/chemistry/ReactionTableDump.hh:10`: it depends on Geant4, DnaLogger and
  ScavengerReactionAccess, so is not standard-library-only portable.
- `header/chemistry/catalog/Tonneau2025Table.hh:9,20`: the builder exists;
  `bulk` is historical metadata, while Placements controls actual routing.
- `header/chemistry/catalog/BoscoloChemReactions.hh:34`: its bulk list has no
  acid-base types 6/7/8. `Farokhi2023Reactions.hh:15`: O2 is not produced by its
  tracked network, but the driver still installs bulk pairs in the mesoscopic
  reaction table. Correct both without modifying data.
- `CLAUDE.md:39`: retain the known benign WrongResolution note, but remove the
  unsupported attribution to zero-diffusion bulk partners. The baseline warning
  is for O^-1 + H3O^1; no claim that every possible resolution warning is benign.
- `CLAUDE.md:79`: document conditional end-time defaults and the serial timing
  distinction; /scheduler/endTime belongs after /run/initialize.

## Dependency evidence (Geant4 11.4.1 source)

Root: `C:/DEV/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1`.
Under `source/processes/electromagnetic/dna/`:

- `utils/src/G4DNAMolecularReactionTable.cc:275-313`: SetReactionType(1)
  changes derived quantities; other types only assign fType. Lines 395-412:
  map lookup keeps the latest pair entry, vectors retain all insertions.
- `utils/src/G4DNAMesh.cc:214-225`: integer product precedes division.
  `models/src/G4DNAEventScheduler.cc:259`: coarsening uses fPixel / 2.
- `models/src/G4DNAEventScheduler.cc:58-136`: scavenger configurations skipped
  without killing; other tracks killed, pending reactions cleared.
- Same file 370-382: mesh callbacks surround steps; 546-571: at most one
  record time per call; 645-664: last-record fallback; 710-719: empty-grid
  warning. 613-638: WrongResolution checks radius >= resolution / pi.
- `models/src/G4DNAIndependentReactionTimeStepper.cc:274-286`: killed
  reactants continue without removing the first pending reaction.
- `processes/src/G4DNAScavengerProcess.cc:99-118`: one stored entry per
  configuration pair; 145-161: H2O rate used as first-order propensity;
  283-290: products belonging to scavenger material join its bulk pool.
- `utils/src/G4DNAScavengerMaterial.cc:99-143`: water/buffer counts not
  incremented/decremented; other scavenger counts change.
- `utils/src/G4DNAChemistryManager.cc:354-425`: master reaction-table setup,
  per-thread initialization, serial thread initialization deferred until Run.
- Earlier scoring evidence verifies Gillespie propensities use rate/count/
  volume, not the radius implicated in the known warning.

## Deferred behavior issues

- `DnaChemistryList.cc:198-225`: nonpositive fixed bin width is not validated;
  zero/negative values can make the edge loop nonterminating. Do not fix here.
- Equality of observed/diffusion rates is not caught by the <= 0 activation
  warning; no change to warning logic or text.
- `MesoSettings::LogTimeGrid` does not validate finite inputs or protect its
  integer loop counter for extreme density; document usable-input preconditions.
- Tonneau duplicate channels/water routing and rate-unit uncertainties already
  documented in its header remain physics questions, not changes for this pass.
- ReactionTableDump caches the first table forever and uses bounds-checked
  reaction IDs; document the frozen-table requirement rather than add resets.
- ReactionTableDump displays all rates with a second-order conversion/label,
  including first-order H2O entries. Formatting is a public output contract;
  the mismatch is documented rather than fixed.
- The old add-a-new-chemistry recipe claimed all builders run exactly once and
  worker replay harmlessly assigns the same registry selection. Bulk builders
  also run during per-thread process construction. Same-valued assignments
  are not synchronization; actual command dispatch uses each receiving thread's
  UI tree. Corrected those claims without changing dispatch/locking. No race
  is asserted as reproduced by this review.

## Verification required before commit

Run Debug build + all ctest tests with vcpkg debug DLL path, RelWithDebInfo build,
the existing 63-invocation/252-event SBS/IRT/mesoscopic matrix against the original
baseline, and supplemental spatial/MT runs. Audit C++ tokens by reversing local
renames/constants and inlining moved blocks; retain all literal tokens. Record
actual results below before the chemistry commit.

## Additional source checks and documentation changes

- G4MoleculeCounterManager.cc:45 defines its singleton thread-local.
- G4Timer.cc:113-121 returns chrono duration<double> seconds.
- G4GenericMessenger.cc:245-266 preserves a property string but tokenizes method
  arguments, supporting the timeBinsList DeclareProperty caveat.
- G4MoleculeIterator::operator() is a template implemented inline in
  G4MoleculeIterator.hh:105-116, with no corresponding .cc implementation;
  its first call returns true without testing an empty map. This is an explicit
  exception to .cc-only verification, based on the actual implementation.
- G4MTRunManagerKernel.cc:187-202 shares the physics list, builds worker actions
  and initializes the worker. G4WorkerRunManager.cc:754-759 replays UI commands;
  G4UImanager.cc:539 onward resolves the receiving command tree before dispatch.
- Fixed the skill's last-command-wins time-binning claim; both commands cause
  FatalException in ApplyReactionTimeBinning.
- Corrected split header/source catalog paths and lifecycle claims in
  docs/recipes/add-a-new-chemistry.md. Removed stale chemistry line ranges there.
- Corrected MesoSettings versus MesoMessenger portability and mesh-action alias
  use in docs/recipes/mesoscopic-3d-species-counting.md.
- Checked RunInMesh in G4DNAEventScheduler.cc: Begin/EndOfMesh bracket one
  mesh-resolution interval, while InMesh follows each step. Corrected the recipe.
- The same unsupported same-messenger/harmless-worker-replay claim remains in
  docs/recipes/create-a-macro-command.md:205-230. Track it for the final general
  documentation sweep; the chemistry recipe no longer endorses that claim.
- Paper-specific provenance, numerical comparisons and historical validation
  claims were not independently re-derived from the publications in this pass.
  Existing scientific uncertainties remain explicit; no physics assertion is
  used to justify modifying a rate, species, route or reaction type.

## Verification in progress

- Initial Debug build and RelWithDebInfo build succeeded; 17/17 ctest passed
  (1.93 s). Final comment-correction build also succeeded; 17/17 passed (0.71 s).
- Token audit passes all 14 .cc and 16 .hh files against 7be9998 after inlining
  the two helpers and reversing listed renames/the endpoint constant.
- Before/after supplemental spatial and 2-thread IRT runs succeeded (4 invocations,
  16 events total). Each spatial side has 220 valid snapshots; file attributes,
  units, shapes, types and record grids agree. These are structural checks, not
  a statistical equivalence claim for MT/spatial data.
- The additional Debug-verbosity smoke initially failed to locate its macro
  because the retained baseline executable resides in .scratch, and macro lookup
  follows the executable location. That failed harness invocation is retained;
  the corrected check passes an absolute macro path and uses separate outputs.
- Corrected Debug-verbosity mesoscopic checks passed before and after (2 invocations,
  8 events): each side exercised both moved diagnostic bodies four times, reported
  all four hand-over totals conserved, no DRIFT, and two completed dumps. Wall
  times were 210.17 s before / 207.83 s after; these are not performance benchmarks.

## Completed verification and directory deliverable

- Debug build-ninja and RelWithDebInfo build succeeded after the final comment
  edits. Final ctest: 17/17 passed, 1.35 s, with vcpkg debug/bin prepended.
- All 63 matrix invocations passed (252 events): the three example macros plus
  20 seed pairs per mode, each with two /run/beamOn 2 sub-runs, 10 keV electrons.
  Only the baseline WrongResolution Geant4 warning appeared.
- Against original revision 1fcf8ba: 228 scalar observables, 3308 descriptive
  curve points, matching output structures/time grids, zero Holm-adjusted
  flags at alpha 0.01. Minimum raw p = 0.00152563, adjusted p = 0.347844.
  The observable count includes stochastic channels absent in one sample.
- H2O2 examples (baseline -> after; 99% CI of the difference): SBS endpoint
  G-value 0.521750 -> 0.532375, CI [-0.022251, 0.043501]; IRT endpoint
  0.770250 -> 0.751125, CI [-0.056820, 0.018570]; mesoscopic endpoint mean
  count 82.7875 -> 82.9000, CI [-3.210639, 3.435639].
- Exact TrackLengths.csv and PhysicsInteractions.csv matches: 12/42 SBS,
  16/42 IRT, 24/42 mesoscopic for each file. Exact equality was not required.
- Final comment/whitespace rebuild has the same PE .text machine-code hash as
  the executable used by the matrix and supplemental checks. All 30 source/
  header token audits pass after reversing the approved names/constant and
  inlining the two pure-move helpers. git diff --check passes.
- Evidence: .scratch/readability-2026-10-29/chemistry/{comparison.json,
  statistics.csv,curves.csv,matrix-status.json}, chemistry-commit-*.log,
  chemistry-final-code-sections.json, chemistry-token-audit.json,
  chemistry-extra-validation.json and chemistry-debug-checks.log.

Files: all 14 chemistry/catalog .cc and 16 paired/header-only .hh files;
CLAUDE.md, sim-output/SKILL.md, the two chemistry recipes, and this plan.
Categories: local/private naming and units, two diagnostic helper extractions,
source-checked rationale, API/lifetime/failure contracts and documentation fixes.
Risk: diagnostic extraction adds calls but preserves guards, scope and operation
order; no rates, products, routes, RNG calls, defaults or public output contracts
changed. Statistical non-detection is supporting evidence, not proof of equality.
Deferred items are listed above; none of the suspected behavior issues was fixed.
