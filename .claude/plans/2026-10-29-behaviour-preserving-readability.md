# Behaviour-preserving readability pass

Status: approved by the user on 2026-10-29; baseline and core verification complete.

Authorization update (2026-10-30): after approving scoring, the user approved all
remaining modifications within the original readability scope and requested
directory commits, pushing the branch, and opening a PR when finished. This
supersedes the repeated approval gates below. Continue to report concrete findings
before each directory's edits; retain all behavior constraints and bug deferrals.

Branch: `readability/behaviour-preserving-pass` (dedicated branch, no worktree).
Baseline revision: `1fcf8baff854664b24ea8f7e28d8342af21bd2e1`.

The session cannot switch its formal collaboration mode to Plan. This document
implements the requested planning gate: stop after writing it; do not edit code,
existing documentation or macros until the user approves. Branch creation and
this new plan are the only intentional repository changes before approval.

## Scope and invariants

Review clarity, comments, naming and documentation, preserving simulation
behaviour and all public contracts. No physics changes, bug fixes, changed
defaults, reordered statements, RNG calls, changed arithmetic, changed output
formatting, dependency expansion, voxelized geometry or UHDR-specific code.
Keep logging through DnaLogger; preserve existing diagnostics in this pass.
Match local indentation, prefixes, encoding and line endings; no whole-file
formatting. Extractions must move existing operations without changing their
order, conditions, ownership, reference identity or object lifetimes.

Keep every portable component's existing dependency boundary. Distinguish
standard-library-only, Geant4-type-dependent but kernel-free, and Geant4 runtime
code accurately; do not call all three standard-library portable. For example,
ArgParser and OutputDir include globals.hh, whereas MacroResolver uses std only.
Audit PureWaterReactions' actual Geant4 dependencies rather than relabelling it
standard-library-only; preserve its existing portability to other projects.

Do not rename Geant4 APIs, macro commands, filenames, Manifest.json keys or other
serialized fields. Defer public C++ renames needing caller edits in other concern
directories; this keeps commits independently buildable and directory-scoped.
Use CONTEXT.md vocabulary; preserve every non-obvious warning, especially
ConvertIndex overflow, irreversible SetReactionType(1), GetValueOf memory
corruption and benign WrongResolution.

Unrelated working-tree changes to preserve and never stage:

- `.claude/hooks/hook-posttooluse.log`
- `docs/literature-comparison/simulation_vs_literature_comparison_meesungnoen.csv`
- `docs/literature-comparison/simulation_vs_literature_comparison_meesungnoen.md`

## Prior context

Read CLAUDE.md, CONTEXT.md, .claude/geant4-instructions.md, project configuration,
the existing named-chemistries/source-layout/manifest plans and relevant portions
of the O2-scavenger and portable-config plans. Historical plans are evidence of
past work, not current instructions to implement those features. Leave all five
existing plans unchanged. In particular their byte-identical-output assumptions,
old test counts and EnergyDeposit.Txt references do not govern this pass.

## Sequence and approval gates

For each row: inspect that concern and matching headers; report findings with
file:line, issue and proposed change under all four axes; obtain approval of its
concrete plan before editing; implement; verify; report actual results; commit.
Approval of this document authorizes the baseline and the core changes below.
Later concerns receive their own concrete findings and approval gate.

| Order | One implementation commit | Paired scope / review focus |
|---|---|---|
| 1 | src/core | header/core; parsing, path resolution, logging and UI wrappers |
| 2 | src/scoring | header/scoring; counters, serialization, dump/reset and units |
| 3 | src/chemistry including catalog | header/chemistry including catalog; stage transitions, reaction builders, settings, Geant4 rationale |
| 4 | src/actions | header/actions; event/run ownership, merge order, lifecycle |
| 5 | src/physics | header/physics; physics registration and initialization commentary |
| 6 | src/geometry | header/geometry; homogeneous box, boundary/scavenger units |
| 7 | sim.cc | setup order, CLI and macro resolution, cleanup and timing |

Treat chemistry/catalog as part of the chemistry concern, as requested. Headers
travel with their implementation rather than a later all-header refactor.
Header-only components belong to their matching concern. Include only directly
related doc corrections in each concern commit. After the seven concerns, finish
any unowned documentation and macro-comment work in separate docs-only and
macro-only commits, with findings/approval first; no cross-directory refactors.
The plan can be included in the first concern commit after approval.

## Concrete core findings (line numbers at baseline)

### Clarity

| Location | Issue | Proposed change |
|---|---|---|
| src/core/ArgParser.cc:23 | Parse spans 64 lines and mixes traversal, conversion, validators and storage; nested validation obscures traversal. | Move the integer and string branch bodies into private ParseIntValue / ParseStringValue helpers. Preserve positive-integer check, strtol base, casts, validator calls, storage and exact error strings. Keep argv traversal, boolean handling and missing-value handling in Parse. |
| src/core/OutputDir.cc:148 | ConfigureSubdir mixes path policy and filesystem work, though below the approximate 50-line threshold. | Retain its structure for now: a helper is not clearly beneficial enough to justify touching the state/error path. Clarify the contract in its header. |
| src/core/OutputDir.cc:15 | Lazy default directory creation has non-obvious locking and fallback behaviour. | Preserve the logic and rationale; explain the mutex's limited coverage rather than implying all globals are synchronized. |

No dimensional magic numbers, dead code or substantial duplicate blocks were
identified in core. Decimal radix 10 and argv index 1 are not physical constants
and do not need invented unit-bearing constants.

### Comments

| Location | Issue | Proposed change |
|---|---|---|
| src/core/*.cc:1 and header/core/*.hh:1 | All twelve files lack at least one of purpose, accurate portability or thread-safety expectations. | Add short file headers; preserve existing useful rationale and local comment style. |
| header/core/MacroResolver.hh:4 | 'standard library only (portable like ArgParser)' conflates different dependency boundaries. | State MacroResolver is std-only and stateless; remove the misleading comparison. Filesystem state can change externally. |
| header/core/OutputDir.hh:5 | 'Pure' and unchanged-filename description conflict with filesystem side effects, lazy default, prefix and subdir. | Describe effective-root selection and side effects, including silent cwd fallback if default creation fails. |
| src/core/DnaLogger.cc:80 | 'Should not happen' overlooks direct calls to public LevelFromString. | Limit the candidate-validation claim to the UI command; document the direct-call diagnostic and Quiet fallback without changing them. |
| src/core/DnaLogger.cc:9 | Atomic verbosity is distinct from synchronization of output destinations. | Explain relaxed atomic access and delegation to Geant4 streams; do not promise globally ordered or indivisible log lines. |

For OutputDir, document caller coordination for configuration/prefix/subdir
mutation versus worker reads, and the existing mutex for lazy default creation.
For ArgParser, mutable instances require caller synchronization; for messengers,
describe UI dispatch and owned command lifetimes without inventing a thread-safe
public mutation API.

Geant4 evidence already inspected in the configured 11.4.1 source tree
`C:/DEV/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1`:

- `source/intercoms/src/G4UIcmdWithAString.cc:53`: SetCandidates forwards the candidate list.
- `source/intercoms/src/G4UIparameter.cc:135` and `:150`: candidate checking rejects unmatched values.
- `source/global/management/src/G4ios.cc:158`: MT streams/buffers use thread-local instances; output routing depends on the destination.
- `source/run/src/G4MTRunManager.cc:264`: Initialize calls BeamOn(0) to set up workers; `:419` starts workers in InitializeEventLoop. Preserve the PreInit ordering note in OutputDirMessenger.hh:13.

Complete any further call-chain verification before adding new claims. For later
concerns record the actual .cc function/line supporting each Geant4 claim. Keep
historical empirical warnings identified as observed limitations; source alone
cannot prove a prior mixed-build memory-corruption incident. Flag uncertainty.

### Naming

| Location | Issue | Proposed change |
|---|---|---|
| src/core/MacroResolver.cc:13, :23, :27 | p, r and c hide path/result/candidate roles. | Rename locals to path, result and candidatePath; optionally arg to macroArgument in the signature and documentation, without changing the function name. |
| src/core/ArgParser.cc:49 | end and parsed obscure integer parsing roles. | Rename to parseEnd and parsedValue within the extraction; retain types and casts. |
| src/core/OutputDir.cc:156 | sub is less explicit than the surrounding path names. | Rename local to subdirPath; retain gPrefix/gSubdir and existing public names. |

No physical quantity in core needs a unit suffix. Preserve f/fp/g member/global
prefix conventions and PascalCase files/classes.

### Documentation

| Location | Issue | Proposed change |
|---|---|---|
| header/core/ArgParser.hh:19, :25, :34, :36 | Class, registration functions, error getter and individual value getters lack complete contracts. | Brief docs for each: inputs, validator rejection, positive integers, defaults, ignored tokens, retained state and partial updates on failure; no units. |
| header/core/DnaLogger.hh:20 | Most public functions lack docs, including invalid enum/name fallback. | Document filtering, conversion, borrowed candidate string, direct-call fallback and output routing. |
| header/core/DnaLoggerMessenger.hh:12 and header/core/OutputDirMessenger.hh:18 | Public class/constructors/destructors/callbacks lack individual contracts. | Document ownership, command arguments, unknown-command handling and actual failure behaviour; OutputDir wrapper raises FatalException. |
| header/core/MacroResolver.hh:15, :22 | Result type and Resolve need complete parameter/failure contracts. | Explain ordered candidates, existing regular files, found=false and ordinary filesystem errors treated as missing. Do not promise noexcept. |
| header/core/OutputDir.hh:44, :57, :72 | Prefix/subdir effects, default-use conflict and default root are incompletely described. | Make every public operation's parameters, side effects, required coordination and bool+error contract explicit; distinguish messenger G4Exception from library failures. |
| CLAUDE.md:36 | Says absent --dir leaves filenames unchanged, inconsistent with lazy results root and macro configuration. | Correct only the core-owned description to match OutputDir and sim.cc:104-116. |

### Deferred core concerns (do not fix)

- `src/core/ArgParser.cc:50`: strtol overflow/errno is unchecked. Potential acceptance of an out-of-range CLI integer; retain existing behaviour and report separately.
- `src/core/ArgParser.cc:23`: repeated Parse calls retain prior values/error, and failed parses leave partial updates. Document current behaviour; do not add resets or rollback.
- `src/core/DnaLogger.cc:81`: existing direct G4cerr fallback bypasses verbosity. Keep it: routing it through filtered Print would change behaviour. No new ad hoc output.
- `src/core/OutputDirMessenger.cc:11`: user-visible guidance refers to per-thread/per-event dumps. Record for the scoring/pre-chemical audit; do not change executable strings in a comment-only correction.

## Documentation and macro audit

Audit CLAUDE.md, docs/, docs/output/, ADRs and local skills against current code
as each owning concern is reviewed. Preserve historical experiment results and
plans as historical. Never rewrite an ADR decision; append an addendum only for
verified changed facts. Read applicable agent-writing skill instructions before
editing CLAUDE.md or skills. Give every project .cc/.hh in scope a short purpose,
portability and thread-expectations header; document each public header API with
parameters/units, preconditions and failure behaviour (including exceptions
where applicable), without blanket no-throw claims.

Inventory every macro/*.in, not just the three examples. Add demonstration
headers and explain non-obvious initialization/command ordering. Change comments
only: keep all executable command lines, values and order identical.

Already observed conflicts to assign to the appropriate concern or final docs audit:

- `.claude/.claude-project.json:43-44` says 25 keV / 10 events; user and CLAUDE.md say 10 keV / 2. Follow the user; list configuration conflict rather than changing run defaults.
- `.claude/.claude-project.json:70` has an empty benignWarnings list despite the documented WrongResolution warning. Verify exact warning in source and logs before proposing a documentation-only clarification.
- `CLAUDE.md:79` shows an unconditional SetEndTime(1.*s), but ActionInitialization.cc:66 and DnaChemistryList.cc:344 select 1 s or 1 us. Correct under the owning concern.
- The inspected `.claude/geant4-instructions.md` does not contain the HDF5 debug-bin PATH note mentioned by the user. Follow the explicit instruction regardless; propose adding the missing procedural note.

## Baseline and verification protocol

After approval and before the first implementation edit:

1. Save revision, Git diff/status, compiler/build configuration, Geant4 version,
   macro copies/hashes and invocation environment under ignored
   `.scratch/readability-2026-10-29/`. Preserve original baseline throughout.
2. Load MSVC x64 with the documented vcvars64 wrapper. Build all Debug targets:
   `cmake --build build-ninja --config Debug`; list and run CTest:
   `ctest --test-dir build-ninja -N` and
   `ctest --test-dir build-ninja --output-on-failure`.
   Prepend `C:/DEV/vcpkg/installed/x64-windows/debug/bin` to PATH for CTest
   (existence verified), ahead of release HDF5 DLLs. Capture complete logs/counts.
3. Build `cmake --build build --config RelWithDebInfo --target sim` under MSVC.
   Run from build/, with required runtime DLL PATH, isolated output directories
   and captured exit codes/logs. Do not reuse Debug DLL PATH for release runs.
4. Run unchanged example_sbs.in, example_irt.in and example_meso.in. Each already
   has 10 keV e-, two `/run/beamOn 2` sub-runs, two seeds and two dumps. Preserve
   both sub-runs (four events per invocation), O2 21%, Ritchie1994 and end times.
   Pass an absolute --dir whose parent exists. Retain all baseline outputs.
5. Gather an ensemble for statistical comparisons: 20 serial invocations per
   mode, each using scratch copies of the respective example with a recorded,
   predetermined seed schedule (only the two seed commands vary). Use the same
   schedule after every concern; every beamOn remains 2. Retain the original
   unchanged-example smoke run separately. Freeze scripts, seed schedule,
   observables and comparison rules before examining post-change output.

After EACH concern, repeat the Debug build/CTest, release build, all three exact
example smoke runs and the ensemble against the original baseline. Do not build
the next concern before these results are resolved. Run long commands through
tool sessions with logs, provide progress, and do not claim a timeout is a pass.

Comparison rules:

- Check exit status, both configured success markers, every warning/fatal and
  expected output presence/content. WrongResolution is allowed only after its
  actual context is checked; never ignore all G4Exceptions. Expect no mesoscopic
  species files in SBS/IRT-only outputs. Inspect manifests, schemas, dump/run/event
  counts, seed/beam/mode settings and stage record grids exactly.
- Compare physics interaction totals, deposited energy, track-length summaries,
  reaction counts and particle/mesoscopic species yields. Treat each invocation
  as the replicate (time bins and species within an event are correlated).
  Report means, between-invocation SD, absolute/relative shifts and confidence
  intervals for differences at preselected common endpoints (particle-stage end,
  hand-over where applicable, mesoscopic end). Inspect complete time curves for
  missing bins/species or systematic shifts; do not interpolate across stages.
- Use two-sample Welch comparisons with Holm-adjusted 1% significance level over
  the preselected scalar observables as a regression screen. Report sample sizes,
  zero-variance/zero-baseline cases explicitly. A detected shift or structural
  mismatch blocks the concern commit pending investigation; do not tune thresholds
  after seeing a failure. Do not remove inconvenient outputs from the comparison.
- Wall-clock measurements, timestamps and isolated output roots legitimately
  differ; record these exclusions explicitly, retaining originals. Preserve
  exact checks for deterministic fields and physics observables when baseline
  repeats establish determinism. No requirement for chemistry byte equality.
- Finite ensembles cannot prove statistical identity. Report 'no detected
  regression at this sample size' with interval widths, not proof of equivalence.
  Wide intervals are an explicit limitation. Behaviour preservation also requires
  a line-by-line diff audit of statement order, arithmetic, RNG, initialization,
  lifetimes, dependencies, diagnostics and output contracts.

Use existing tests; add focused tests only if a nontrivial extraction exposes an
uncovered behavioural risk. No tests mirroring comments or renames. Stop and
report baseline failures before refactoring, without fixing unrelated bugs.

## Per-concern deliverable and completion

Report files touched, changes grouped by the four axes, Geant4 source evidence,
behaviour risks, exact commands and actual test/run/statistical results, deferred
bugs/public-contract renames/doc conflicts, and commit hash. Explicitly list any
verification not run. Stage only reviewed concern files and directly related
documentation; never use blanket git add. No push requested.

Final audit: all seven concern commits present in order, required file/API/macro
comments covered, no unapproved logic changes or new dependencies, no accidental
user-file edits, all deferred items collected, and final working-tree status
reported. The user approved this plan on 2026-10-29.

## Execution notes: core

- Baseline: both builds succeeded, Debug CTest 17/17 passed, all 63 simulation
  invocations (252 events) passed output validation. Evidence is in
  `.scratch/readability-2026-10-29/`; the original executable and build caches
  are retained there. Access to the configured Anaconda Qt dependency required
  sandbox escalation for builds and simulation launches.
- Core patch applied after the complete baseline audit. Debug and release
  builds succeeded; Debug CTest 17/17 passed again. All six core .cc files pass
  an executable-token audit against the baseline after reversing approved
  local renames and restoring the moved parser branch bodies. All 63 post-change
  invocations passed, as did the final structural/statistical comparison below.
- Comparator correction, recorded rather than hidden: the initial structural
  check incorrectly required identical reaction-ID maps and species presence
  in each paired dump. ReactionCounter.cc:155 builds IDs only from observed
  reaction labels; ScoreSpecies.cc:119 iterates recorded molecule indices.
  Unchanged baseline dumps already show different maps and 13 versus 14 species.
  The corrected check validates each local reaction map and each present
  species' full time grid, compares the fixed time grids and manifests, and
  retains all reaction/species observations (absent = zero) in the same
  statistical comparisons. No thresholds or statistical observables changed.
  Original comparator retained as `compare-v1.py`; original hashes retained in
  `frozen-inputs.json`. This was a harness correction, not a simulation change.
- Further deferred documentation conflict: the mesoscopic baseline warning
  reports O^-1 + H3O^1, effective radius 6.3164 nm versus resolution 15.258789 nm.
  The specific existing explanation attributing WrongResolution to D=0 bulk
  partners is not corroborated by this log. Preserve the warning note and
  revisit its explanation during chemistry review. G4DNAEventScheduler.cc:613
  compares radius with resolution/pi; G4DNAGillespieDirectMethod.cc:96 computes
  propensity from observed rate, volume and counts (radius only in an error
  diagnostic).
- Further deferred docs conflict: `.claude/skills/sim-output/SKILL.md:120`
  describes cwd fallback when no output directory is configured, whereas its
  later paragraph correctly describes the lazy executable-relative results root.

### Core delivery

Files: all six source/header pairs under core (ArgParser, DnaLogger,
DnaLoggerMessenger, MacroResolver, OutputDir, OutputDirMessenger), CLAUDE.md,
the two core source citations in docs/recipes/dnachem-min-macro-command-reference.md,
and this plan. No other concern's implementation changed.

- Clarity: Parse reduced from 64 to 38 lines by moving conversion/validation/
  storage into two private helpers. Branch-body tokens are identical after
  reversing the approved local renames. No logic, ordering, defaults or
  executable strings changed.
- Comments: purpose/dependency/thread-expectations headers in all twelve core
  files; accurate lazy-root and logger fallback explanations.
- Naming: parseEnd/parsedValue, result/candidatePath/path, subdirPath. No public
  API, macro, output filename or manifest key renamed.
- Documentation: public APIs describe parameters, state retention, defaults,
  caller coordination and error behaviour; corrected the stale CLAUDE output
  fallback description and replaced fragile core line citations with constructor
  names. The core scope has no physical quantities needing unit suffixes.

Commands actually run (MSVC vcvars64 wrapper for builds):

```text
cmake --build build-ninja --config Debug
ctest --test-dir build-ninja -N
ctest --test-dir build-ninja --output-on-failure
cmake --build build --config RelWithDebInfo --target sim
sim.exe example_sbs.in --dir <isolated-directory>
sim.exe example_irt.in --dir <isolated-directory>
sim.exe example_meso.in --dir <isolated-directory>
python .scratch/readability-2026-10-29/run_matrix.py baseline
python .scratch/readability-2026-10-29/run_matrix.py core
python .scratch/readability-2026-10-29/compare.py core
python .scratch/readability-2026-10-29/audit_core_diff.py
git diff --check
```

Builds succeeded before and after; CTest passed 17/17 before (3.05 s) and after
(2.83 s), using vcpkg debug/bin first in PATH. No build warnings or errors in
the successful core build logs. All three exact example macros and 20 additional
seeded repetitions per mode ran before and after: 126 invocations, 504 events
total. Each beamOn remains 2, two dumps per invocation. All invocations returned
0, printed both success markers and passed file/manifest/grid validation. Only
the recognized WrongResolution G4Exception warning occurred in mesoscopic runs.

Statistical result: 227 scalar observables, 3,272 full-curve points; zero flagged
scalar differences at Holm-adjusted alpha=0.01. Minimum raw p=0.0133063; minimum
adjusted p=1.0. Twenty invocation-level replicates (80 events) per mode per side.
Twenty-two scalar pairs have zero observed variance on both sides, with equal
means; these are explicitly recorded, not evidence of zero population uncertainty.
Mean deposited energy was 10,000 eV/event in every mode on both sides.

| H2O2 observable | Baseline mean | Core mean | Difference, 99% CI |
|---|---:|---:|---|
| SBS last particle record, molecules/100 eV | 0.521750 | 0.529375 | 0.007625 [-0.024452, 0.039702] |
| IRT-only last particle record, molecules/100 eV | 0.770250 | 0.755625 | -0.014625 [-0.053667, 0.024417] |
| Mesoscopic end, molecules/event | 82.7875 | 83.0750 | 0.2875 [-2.810315, 3.385315] |

The largest absolute mean shift among mesoscopic curve points was 3.8 molecules/
event for O2- at 1 ms (231.1375 to 234.9375); its endpoint is included in the
scalar screen. PhysicsInteractions.csv and TrackLengths.csv were exactly equal
for 18/42 SBS dump pairs, 16/42 IRT pairs and 24/42 mesoscopic pairs; their
numerical observables remain included in the statistical comparison for all pairs.

Evidence: `.scratch/readability-2026-10-29/core/comparison.json`, `statistics.csv`,
`curves.csv`, per-invocation logs/status/manifests, core build/CTest logs and the
retained baseline. No detected regression at this sample size; finite sampling
does not prove identity, especially for rare reactions. The executable-token
audit independently checks the behaviour-preserving scope. MT/GUI and production
macros were not run; they are outside the requested three-mode verification matrix.

Deferred: unchecked strtol overflow, retained parser state/partial updates, the
existing unconditional logger fallback, outdated pre-chemical UI guidance, and
the documentation conflicts recorded above. No suspected bug was fixed. Existing
unrelated hook-log and literature-comparison changes remain untouched.
