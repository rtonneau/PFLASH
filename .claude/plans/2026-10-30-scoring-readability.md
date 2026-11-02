# Scoring readability pass

Status: approved by the user on 2026-10-30; implementation and verification complete.

Authorization update: the user subsequently approved all remaining modifications
within the original readability scope and requested commits, a push and a PR when
finished. Further directory findings will still be reported before edits, but
no additional per-directory approval is required. Suspected bugs remain deferred.

Branch: `readability/behaviour-preserving-pass` (no worktree).
Review revision: `7f75daa` (completed core pass).
Original simulation baseline: `1fcf8baff854664b24ea8f7e28d8342af21bd2e1`.
Parent plan: `2026-10-29-behaviour-preserving-readability.md`.

## Scope and gate

One commit for the fourteen `src/scoring/*.cc` files and their fourteen
`header/scoring/*.hh` counterparts, this plan, and directly related documentation
corrections listed below. No implementation changes in another concern. Existing
unrelated hook-log and literature-comparison changes remain untouched.

The parent plan explicitly limits its initial approval to baseline and core;
this document is the next concrete directory plan. Approval authorizes the
scoring work and verification below, followed by its commit. Chemistry receives
its own findings before implementation, under the authorization update above.

No changed logic, defaults, arithmetic order, loop order, RNG calls, diagnostics,
serialization, output names, command names or manifest keys. Extractions are
pure moves. Preserve conditions, references, copies, object lifetimes, exception
boundaries and reset order. Do not replace existing unconditional diagnostics
with filtered logging; any genuinely new logging would use DnaLogger, but none
is planned. Keep each component's existing dependency boundary.

Line numbers below refer to the review revision, before scoring edits.

## Clarity

| Location | Finding | Proposed change |
|---|---|---|
| `src/scoring/JsonWriter.cc:60` | WriteNode spans roughly 55 lines; child selection is embedded in a lambda. | Move the existing lambda body into an anonymous-namespace WriteChild helper, forwarding the existing stream, node, array flag, index and indentation. Preserve recursion, formatting and insertion order. |
| `src/scoring/PreChemicalFiles.cc:82` | MoveStaged combines discovery, numeric sorting and publication. | Move the final loop at line 116 into MoveEntries, passing the existing entries, result, staging directory and resolver by reference. Move the local Entry type into the implementation namespace if needed. Keep scan/sort/error paths and result ordering unchanged. |
| `src/scoring/MesoSpatialFile.cc:139` | AppendEvent spans roughly 86 lines and mixes file setup, metadata and records. | Extract the already scoped new-file attribute block at line 164 into WriteRootAttributes. Extract the record loop at line 194 into WriteSnapshotRecords, passing the existing firstGroup vector and handles by reference. Keep their declarations and destruction points in AppendEvent; preserve explicit h5.close(), validation-before-lock and the existing catch boundary. |
| `src/scoring/RunAccumulatorMessenger.cc:123` | WriteAllAndReset spans 153 lines with several output responsibilities. | Extract the species lookup/write block, the body of the already scoped mesoscopic-output branch at line 210, and the already scoped spatial-publication block at line 243 into named helpers. Keep conditions and calls at their original positions. Leave the unscoped output streams and their explicit closes in the caller so their lifetimes do not change. |
| `src/scoring/RunManifest.cc:157` | Write mixes runtime sampling, ordered field construction and index publication. | Extract contiguous chemistry-field and output-field Add calls into AppendChemistryFields and AppendOutputFields. Pass existing values by reference; sample runtime getters inside the helpers in the original order. Retain timestamp calls, gPreviousDump update, temporary ownership and publication order. Do not cache or combine reads. |
| `src/scoring/PrimaryKiller.cc:80` | ProcessHits mixes cut decisions with two lengthy verbose messages. | Move the existing message blocks at lines 113 and 135 into private helpers, invoked inside the same verbosity conditions. Preserve exact stream operations, event lookup timing, casts, cut checks, AbortEvent ordering and return values. |
| `src/scoring/ScoreSpecies.cc:192` | AbsorbResultsFromWorkerScorer mixes aggregate and optional per-event merges. | Extract the aggregate merge loop into a private MergeSpeciesTotalsFrom helper. Preserve by-value iteration, additions, source clearing at the original later position, and conditional compilation. |
| `src/scoring/ScoreSpecies.cc:347` | WriteWithAnalysisManager combines booking and two row formats. | Extract the aggregate row loop into WriteAggregateSpeciesRows with the same ntuple ID and manager; keep bookings, writes and closes in their existing order. Retain by-value map iteration. Defer restructuring the disabled per-event branch because it contains suspected compile errors. |
| `src/scoring/ScoreSpecies.cc:103` | EndOfEvent mixes counter preparation and accumulation and exceeds 50 lines. | A future AccumulateRecordedSpecies helper would be a useful boundary, but defer this extraction: the disabled per-event branch crosses it and is already inconsistent. This pass removes stale debug comments and improves names and lifecycle rationale without altering that control flow. |
| `src/scoring/ScoreSpecies.cc:52`, `:164` | Record-grid and G-value constants lack descriptive names. | Name the existing 60-step count, base 10.0, last record 999999 ps and 100-eV normalization factor. Preserve literal types and every multiplication/division position; names distinguish numeric values in ps/eV from Geant4 internal-unit values. |
| `src/scoring/MesoSpatialFile.cc:80`, `:82`, `:102`; `src/scoring/JsonWriter.cc:43`, `:103`; `src/scoring/TrackLengthTable.cc:104` | Chunk limit, compression level, spatial coordinate count, JSON indentation/precision and table precision are unexplained formatting/layout constants. | Introduce local named constants with the same types and values where they clarify use. These are counts/format settings, not physical units. Do not alter dimensions or on-disk layout. |
| `src/scoring/ScoreSpecies.cc:129`, `:175`, `:184`, `:206`, `:224`, `:254`, `:349` | Stale commented-out debug statements obscure active code. | Remove these dead comments, retaining non-obvious lifecycle, compatibility and physics notes. Do not delete live conditional compilation. |

Extraction is not a requirement to force every function below 50 lines. If a
proposed move would alter a lifetime, introduce a new dependency, or require
logic changes, retain the original block and report the deferred extraction.
Avoid a generic output framework: each helper stays in its existing component.

## Comments

| Location | Finding | Proposed change |
|---|---|---|
| All fourteen scoring `.cc` and `.hh` pairs, line 1 | File headers are inconsistent about purpose, dependencies and concurrency. | Add short purpose/portability/thread-expectations headers, retaining attribution. Distinguish std-only components, the existing DataNode/JsonWriter pair, std+HDF5, Geant4 types and Geant4 runtime services. No new dependencies. |
| `src/scoring/PrimaryKiller.cc:93` | Comment says secondary electrons, but the next guard selects track 1 with PDG 11. | Correct to primary electron; explain that the preceding box cut applies to every track. Retain event-local reset explanation. |
| `src/scoring/ReactionCounter.cc:19` | Default bin edges are called the same fixed checkpoints as ScoreSpecies. | Describe the coarser decade bin edges; ScoreSpecies has a denser logarithmic record grid. Preserve values. |
| `src/scoring/ReactionCounter.cc:30`; `header/scoring/ReactionCounter.hh:4`, `:43` | Important GetValueOf memory-corruption warning is mixed with an overly broad kernel-free claim. | Keep the observed memory-corruption warning. Limit the kernel-free claim to parsing/binning/counting logic; WriteCsv uses the analysis manager and OutputDir. Identify the warning as an observed integration limitation, not an unproven universal Geant4 defect. |
| `header/scoring/ReactionCounter.hh:70`; `src/scoring/ReactionCounter.cc:155` | Stable reaction IDs can be read as stable across dumps. | Explain that IDs are assigned from sorted labels observed in the current accumulated data; consumers resolve IDs through each dump's metadata. |
| `src/scoring/RunAccumulatorMessenger.cc:173` | The Clear-versus-Reset rationale must remain, but its precise claims need implementation evidence. | Keep the ordering constraint. Tighten the explanation to what the configured Geant4 implementation actually clears; avoid claiming Reset clears bookings if it only resets contents. |
| `src/scoring/RunManifest.cc:187` | Says actual cell size equals the requested size unless capped. | Account for power-of-two pixel-count selection as well as the cap. Preserve the overflow-cap/ADR rationale and all manifest values. |
| `src/scoring/MesoSpatialFile.cc:75` | Compression rationale makes a broad silent-no-op assertion. | Keep the reason for conditional chunking/compression; qualify build-dependent behavior using the available HDF5 implementation. Preserve the nonzero chunk-dimension explanation. |
| `src/scoring/ScoreSpecies.cc:137`, `:375`, `:420` | Useful hand-over/counter-lifecycle notes coexist with comments that merely label obvious column writes. | Retain and tighten the lifecycle notes; remove redundant WHAT comments. Use Particle-based stage, Mesoscopic stage and Hand-over time consistently. |

Thread expectations must be specific: ordinary counter/tree instances require
caller coordination when shared; RunAccumulator/RunManifest globals rely on
master/serial lifecycle coordination; AppendEvent serializes its HDF5 operations
with its mutex, but MoveStaged requires writers to have stopped. Do not imply
that a mutex in one function makes an entire API thread-safe.

Source evidence inspected in Geant4 11.4.1:

- `source/digits_hits/detector/src/G4SDManager.cc:104` PrepareNewEvent initializes
  the sensitive-detector tree; `G4SDStructure.cc:178` initializes active detectors;
  `G4MultiFunctionalDetector.cc:97` initializes each primitive scorer.
- `source/analysis/management/src/G4VAnalysisManager.cc:383` Reset delegates to
  ResetImpl; Clear at line 395 clears implementation data, booking data, ntuple
  manager and file manager. `source/analysis/factory/src/G4GenericFileManager.cc:301`
  forwards Clear to its file managers. Complete the concrete CSV file-manager
  chain before asserting details about filename registry reuse.
- Project `src/chemistry/TimeStepAction.cc:577` and `:724` deactivate and restore
  molecule counting around the hand-over; `src/chemistry/MesoSettings.cc`
  UncappedPixelCount chooses a power of two by minimum cell-size difference.
- Local HDF5 1.14.6 `src/H5Pocpl.c:1044` implements H5Pset_deflate and line 1065
  marks its filter optional. Verify the filter application path before retaining
  the stronger claim about a missing filter; otherwise state uncertainty.

Any additional Geant4 behavior claim will be checked against implementation
source before writing it. Header declarations alone are not evidence.

## Naming

| Location | Finding | Proposed change |
|---|---|---|
| `src/scoring/MesoSpatialFile.cc:100`, `:107`, `:192`, `:195`, `:207` | s/n/rec/src/ds hide shape and link roles. | speciesCount, cellCount, record, sourceSnapshotGroup and datasetName; snap becomes snapshot. Keep HDF5 attribute/dataset names unchanged. |
| `src/scoring/PreChemicalFiles.cc:22`, `:40`, `:103` | s/v/c/r/e are vague parsing locals. | digits/parsedValue/digit/parsedRunId/parsedEventId; name the existing nine-digit limit without widening accepted input. |
| `src/scoring/TrackLengthTable.cc:51`, `:73`, `:100` | n/r/sq/d/s/sep obscure statistical and formatting roles. | rowCount/row/sumSquaredDeviations/deviation/summary/separator. Keep summation order and calculations exactly as written. |
| `src/scoring/RunManifest.cc:97`, `:161`, `:162`, `:197` | Short statistic locals and duration names without units. | summary/columnStat/outputDir and elapsedSinceStart_s/elapsedSincePreviousDump_s. No manifest key changes. |
| `src/scoring/ResultsIndex.cc:17`, `:27` | manifest and picked obscure path versus selected run fields. | manifestPath and selectedRunFields. |
| `src/scoring/ScoreSpecies.cc:147`, `:149`, `:154`, `:351`, `:372` | idx/time_mol/n_mol/fNtupleID/G/G2 obscure roles or local/member status. | moleculeIndex/recordTime/moleculeCount/ntupleId/sumG/sumG2; use a unit suffix only after confirming the stored unit. Rename map-entry locals descriptively without changing copy/reference semantics. |
| `src/scoring/MesoSpeciesCounter.cc:14`; matching header | Add's time argument omits its ns unit. | time_ns in declaration/definition and documentation. Preserve the public function name and numeric value. |
| `src/scoring/PrimaryKiller.cc:99`, `:101` | kineticE/eLoss do not state unit convention. | Prefer kineticEnergy/energyLoss with explicit Geant4 internal-unit documentation; do not label internal energies as eV. Preserve existing member/global prefix styles. |

No public command, filename, manifest key, Geant4 API, file or class rename is
proposed. Public C++ renames requiring edits in other concerns are deferred.

## Documentation

| Location | Finding | Proposed change |
|---|---|---|
| Every public class/function in `header/scoring/*.hh` | API comments vary in completeness. | Briefly document purpose, inputs/units, caller preconditions, ownership, mutation and actual failures. Avoid boilerplate claims of thread safety or noexcept. |
| `header/scoring/DataNode.hh`; `header/scoring/JsonWriter.hh` | Typed getters do not check kind; string-pointer construction needs a valid pointer; writer failures are stream-based. | Document matching-kind/non-null preconditions, Add/Push null promotion and wrong-kind logic_error, insertion order, stream fail bits/configured exceptions and caller-owned units. No new validation. |
| `header/scoring/TrackLengthTable.hh:80` | Wording can imply both mean and SEM are zero for one sample. | State that one sample retains its mean; SEM is zero with fewer than two samples. |
| `header/scoring/MesoSpatialFile.hh:62` | Failure guarantees may imply transactional writes. | Distinguish pre-write validation/duplicate rejection from HDF5 failures that can leave a partial file. Document only actual shape checks; caller supplies meaningful times/geometry. MoveStaged can remove an existing destination before a later rename failure. |
| `header/scoring/PreChemicalFiles.hh` | Parsing and publication constraints need explicit contracts. | Document exact accepted filename grammar and nine-digit nonnegative identifiers, unchanged outputs on parse failure, partial move results, propagated resolver exceptions and filesystem coordination. |
| `header/scoring/RunAccumulator.hh:62` | Clear comment mentions prefix reservations but omits subdirectory reservations. | Document both retained reservation sets and master/serial ownership, alongside counter units. |
| `header/scoring/RunAccumulatorMessenger.hh:38` | Safety-net flush says its path must never fail. | Explain that it bypasses prefix uniqueness checks; output failures remain possible. Document existing G4Exception severity rather than promising successful publication. |
| `docs/output/README.md:95`; `.claude/skills/sim-output/SKILL.md:18` | Species.Txt is described as yields/G values, but ASCII writes accumulated molecule counts. | Correct the description; distinguish raw counts from CSV sumG/sumG2 and derived per-event G values. Preserve output bytes. |
| `CLAUDE.md:44`, `:56`; `.claude/skills/sim-output/SKILL.md:11` | ReactionCounter portability and the accumulator's two-counter list are stale. | Qualify the portable logic boundary and describe all current counters plus TrackLengthTable. Keep the memory-corruption warning. |
| `CLAUDE.md:53`; `docs/recipes/dnachem-min-macro-command-reference.md:131` | Documents minKineticE as an effective UI setting despite the missing dispatcher branch. | Document the current command limitation separately from the working C++ setter. Do not wire up or delete the command. |
| `CLAUDE.md:27`; `.claude/skills/sim-output/SKILL.md:122` | Nothing-written-before-dump wording ignores staging; cwd fallback description ignores the lazy results root. | Narrow publication wording and align output-root description with OutputDir. Do not alter defaults or unrelated chemistry documentation. |
| `docs/adr/0005-manifest-per-dump.md:11` | JsonWriter depends on DataNode, despite wording suggesting no project-class dependency. | Preserve the ADR decision; if needed add a dated factual addendum identifying the existing portable pair. No rewrite of the decision. |
| Scoring references in the macro command reference | Source line citations will move. | Replace affected numeric citations with stable constructor/function names. |

Read the current writing-for-agents skill before editing CLAUDE.md or the local
sim-output skill. If a referenced skill resource cannot be located, record that
limitation; do not silently invent its contents. Macro comment changes and
unowned build-instruction corrections remain in their later planned concerns.

## Suspected bugs and decisions deferred unchanged

Additional documentation findings before their edits (covered by the user's
approval of all remaining readability work):

| Location at review revision | Issue | Correction |
|---|---|---|
| `docs/recipes/add-a-per-run-counter.md:13`, `:21`, `:69`, `:149` | Overstates dependency freedom and stream-only I/O of ReactionCounter. | Distinguish pure counting/parsing from its analysis-manager/OutputDir CSV writer; retain the GetValueOf warning. Replace scoring line citations with function names. |
| `docs/recipes/create-a-species-counter.md:43`, `:55`, `:92` | Scoring line citations and example local names become stale after extraction/renaming. | Cite methods/helpers and update example names; leave action/chemistry explanations for those concerns. |
| `docs/recipes/export-data-as-hdf5.md:83`, `:100` | Line citations point at moved writer/publication bodies. | Cite AppendEvent, Validate, WriteSnapshotRecords and MoveSpatialOutput by name. |
| `docs/recipes/dnachem-min-macro-command-reference.md:30`, `:127` | Bare scoring citation missed the first replacement; default UI states are described as every state. | Name the methods and list the six states initialized in G4UIcommand.cc, rather than implying Quit is included. |

Historical `docs/reviews/2026-09-29-whole-project-review.md` line references are
retained as a dated snapshot. The scoring formula citation in the literature
plotting script is recorded for the final documentation sweep; its code is not
part of the scoring refactor.

1. `PrimaryKiller.cc:40` creates fpMinKineticE, but SetNewValue at line 58 has no
   matching branch and the destructor at line 48 does not delete it. This pass
   documents the discrepancy without adding dispatch or changing ownership.
2. `ScoreSpecies.cc:169` references undefined molecule inside the disabled
   `_ScoreSpecies_FOR_ALL_EVENTS` branch; `header/scoring/ScoreSpecies.hh:125`
   uses a value key where the output branch treats the key as a pointer.
   Do not enable, remove or fix the optional path in this pass.
3. `RunAccumulatorMessenger.cc:141` uses a global collection ID as the detector's
   primitive index at line 142. This may be fragile if registration ordering
   changes; it is not a demonstrated failure in the current configuration.
   Preserve lookup behavior and report the concern.
4. Some writers do not check stream failure before resetting accumulated data;
   HDF5/file moves are not transactional. No new error handling, rollback or
   changed G4Exception severity in a readability-only commit.
5. Public-contract renames: none proposed. Optional-path repairs, validation
   tightening and any runtime guidance-string changes require separate work.

## Verification and delivery

Before editing scoring, retain the core executable as the immediate pre-scoring
reference. Keep the original baseline and frozen seeds/statistical protocol.
Add a small pre-change spatial-output run using a scratch copy of example_meso,
10 keV electrons and beamOn 2, with the spatial command before initialization;
repeat it after the change. Inspect HDF5 structure, units, record ordering and
hard-link relationships; stochastic cell arrays need not match bit-for-bit.

After implementation:

1. Audit the diff for pure moves, local renames, identical literal types,
   arithmetic/loop order, preserved conditional branches and object lifetimes.
   Run git diff --check and a token-level comparison where applicable.
2. Build Debug in build-ninja using the MSVC environment. Prepend vcpkg's
   debug/bin to PATH and run all CTest tests (currently 17, including HDF5 and
   portable serializer/counter tests). Report actual pass/fail counts.
3. Build RelWithDebInfo in build. Restore the release DLL PATH; never run the
   release executable against the debug HDF5 DLLs.
4. Run the exact example_sbs.in, example_irt.in and example_meso.in plus the
   frozen 20 additional repetitions per mode using run_matrix.py scoring.
   Each existing example has two beamOn 2 calls; preserve both dump cycles.
   Compare with the retained original baseline using the corrected frozen
   comparator, including label-resolved reactions and absent-species zeros.
   Report 227-observable statistical screening and full-curve diagnostics as
   available, without equating a nonsignificant result with proof of identity.
5. Repeat the spatial-output smoke run and existing MesoSpatialFile tests.
   If extracting worker-merge code, also capture a two-thread smoke baseline
   before edits and repeat it after; report this as lifecycle coverage, not a
   statistically powered MT-equivalence claim. Do not alter production macros.
6. Report files touched, four change categories, build/test/run results,
   behavior risks, validation limits and every deferred item. Commit this
   concern alone after checks pass, then review chemistry without editing it.

The findings above were recorded before implementation. The user approved this
plan on 2026-10-30; results and any implementation deviations are recorded below.

## Implementation and evidence

All fourteen scoring implementation/header pairs received purpose, dependency
and concurrency headers and public API documentation. No includes, public
function names, serialized fields, output strings or conditions were changed.
The original optional per-event code remains disabled and unrepaired.

Implemented the proposed helper moves in JsonWriter, PreChemicalFiles,
MesoSpatialFile, RunAccumulatorMessenger, RunManifest, PrimaryKiller and
ScoreSpecies. Existing handles, lookup vectors, streams and worker-reset order
retain their original lifetimes/positions. Scalar helper arguments either bind
existing values or copy inert scalar values; no new runtime getters are evaluated
at the call sites. ScoreSpecies::EndOfEvent and the disabled per-event path retain
their control-flow structure, as planned. Local names and formatting/grid constants
were clarified without changing types or arithmetic order. Internal Geant4 time
and energy values retain that unit convention; duration values in seconds and
MesoSpeciesCounter's ns parameters are explicit in names.

Additional source verification:

- Geant4 `source/analysis/csv/src/G4CsvFileManager.cc`, CreateNtupleFile:
  existing registered names receive an `_bis` suffix; CloseNtupleFile leaves
  registration with the file manager. `source/analysis/hntools/src/G4ToolsAnalysisManager.cc:259`
  ResetImpl resets histograms/ntuples; ClearImpl at line 277 clears histogram
  data. The actual template implementations in G4TNtupleManager.icc:222/240
  distinguish resetting ntuple objects from clearing their descriptions;
  G4VTFileManager.hh's inline Clear calls G4TFileManager::ClearData, whose
  implementation at G4TFileManager.icc:152 deletes and clears the file map.
  The updated comment preserves the Clear ordering and avoids overclaiming
  what Reset does.
- HDF5 `src/H5Z.c:1464` skips an unavailable optional filter during writes;
  `src/H5Pocpl.c:1065` marks deflate optional. The compression guard explanation
  now cites this behavior without assuming compressed storage is always smaller.
- Geant4 `source/digits_hits/hits/src/G4HCofThisEvent.cc:44` deletes the owned
  event hit collections; ScoreSpecies's destructor does not own their deletion.
- Geant4 `source/intercoms/src/G4UIcommand.cc:49` initializes six default states,
  excluding Quit. The PrimaryKiller command reference now lists those states.
- Geant4 `source/global/management/src/G4UnitsTable.cc:291` confirms the local
  time-unit spellings. CLHEP's actual SystemOfUnits.h definition sets nanosecond
  to 1; ScoreSpecies's unscaled serialized times are therefore ns.
- Project Run's constructor borrows action-owned counters where available and
  supplies owned fallbacks otherwise; RunAccumulator's header no longer implies
  that all live counters are owned by Run.

The token audit in `.scratch/readability-2026-10-29/audit_scoring.py` inlines moved
helper bodies, reverses the approved local renames/constants, and compares against
7f75daa. All fourteen `.cc` files and fourteen header declaration streams passed.
String literals are kept as indivisible tokens; conditional compilation remains
in the comparison. This complements runtime tests rather than replacing them.

Initial scoring verification passed both builds and CTest 17/17 (1.88 s), with
vcpkg debug/bin first for CTest and release DLL paths restored for simulation.
No warnings/errors appeared in those build logs. Supplemental spatial and MT
runs passed before and after (four invocations, sixteen events total). HDF5
inspection via installed h5dump checked 220 snapshots per side, ordered grids
from 5 ns to 1 ms, matching root attributes/species, dataset types/dimensions
and positive cell sizes. These simulation snapshots did not reuse states;
MesoSpatialFileTest separately passed explicit hard-link object-identity checks.
Both two-thread IRT dump cycles contain two events, two track rows, complete
manifest-listed files and species rows whose nEvent is 2. Species presence may
differ stochastically; this is lifecycle coverage, not a powered MT-equivalence
claim. Evidence: `pre-scoring-extra/`, `scoring-extra/` and
`scoring-extra-validation.json` under the scratch evidence directory.

### Final scoring verification and delivery

Commands actually run, with the configured MSVC environment and DLL paths:

```text
python .scratch/readability-2026-10-29/scoring_extra.py pre-scoring
cmake --build build-ninja --config Debug
ctest --test-dir build-ninja --output-on-failure
cmake --build build --config RelWithDebInfo --target sim
python .scratch/readability-2026-10-29/run_matrix.py scoring
python .scratch/readability-2026-10-29/scoring_extra.py scoring
python .scratch/readability-2026-10-29/check_scoring_extra.py
python .scratch/readability-2026-10-29/compare.py scoring
python .scratch/readability-2026-10-29/audit_scoring.py
git diff --check
```

The matrix ran the exact example_sbs.in, example_irt.in and example_meso.in,
plus twenty frozen-seed repetitions per mode: 63 invocations, 252 events after
scoring. Every invocation returned 0 and produced both expected dump cycles;
file lists, manifests and grids passed the comparator's structural checks.
Only the recognized WrongResolution warning occurred. The original 63-invocation
baseline was reused without rerunning it or changing seeds/thresholds.

Statistical screen: 227 scalar observables, 3,270 curve points, no scalar flagged
at Holm-adjusted alpha 0.01. Minimum raw p = 0.0441308; minimum adjusted p = 1.0.
There are twenty invocation-level samples (80 events) per mode per side; nineteen
scalar pairs have zero observed variance and equal means. Mean deposited energy
is 10,000 eV/event in each mode on both sides. Curve diagnostics are descriptive,
not an additional multiple-testing claim.

| H2O2 observable | Baseline mean | Scoring mean | Difference, 99% CI |
|---|---:|---:|---|
| SBS last particle record, molecules/100 eV | 0.521750 | 0.536750 | 0.015000 [-0.019078, 0.049078] |
| IRT-only last particle record, molecules/100 eV | 0.770250 | 0.758500 | -0.011750 [-0.057495, 0.033995] |
| Mesoscopic end, molecules/event | 82.7875 | 83.2000 | 0.4125 [-2.933343, 3.758343] |

TrackLengths.csv and PhysicsInteractions.csv were exactly equal for 16/42 SBS,
10/42 IRT and 23/42 mesoscopic dump pairs; all numerical values remain in the
statistical comparison. Finite samples do not prove population identity,
particularly for rare reactions. Spatial and MT checks validate structure and
lifecycle only. GUI and production macros were not run; the broken optional
per-event compile mode was not enabled.

After final comment-only corrections, both builds succeeded again, without
warnings/errors; CTest passed 17/17 in 1.03 s. The final release executable's
`.text` section is byte-identical to the retained executable used for the matrix
(verified by pe_sections.py; scoring-final-pe-audit.json). No extra simulation
matrix was needed for those comment-only corrections. All 28 source/header
token audits and git diff --check pass.

Touched files: all fourteen scoring source/header pairs; CLAUDE.md; the local
sim-output skill; docs/output/README.md; the three counter/HDF5 recipes and macro
command reference; a factual addendum to ADR 0005; this plan and the parent
plan's authorization update. No other concern's implementation changed.

Categories: pure-move helpers and named constants; verified WHY/lifecycle
comments; descriptive local names with explicit numeric units where applicable;
public contracts and corrected output/portability documentation. The main
behavior risks were output-handle lifetimes, floating-point accumulation order,
worker clearing and repeated dump cycles; these were preserved and covered by
token review plus the checks above. All suspected bugs in this plan remain
unfixed. EndOfEvent/disabled-path decomposition, runtime guidance strings and
historical/literature citations remain deferred as described above.

Evidence is retained under `.scratch/readability-2026-10-29/`: scoring matrix
logs/status/manifests, statistics.csv, curves.csv, comparison.json, both build/
CTest log sets, supplemental HDF5/MT reports, executable copies and token/PE audits.
