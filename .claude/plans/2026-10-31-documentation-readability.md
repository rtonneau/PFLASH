# Documentation, macros and test-header completion

Standing approval covers these changes. Apply after all seven source concerns
are verified and committed. Use separate documentation, macro and test-header
commits; no executable defaults, commands, assertions or analysis logic edits.

## Clarity

- docs/recipes/add-a-reaction-or-molecule.md:45-143: examples use old local
  names and line anchors. Copy the current helper snippet and unit-bearing
  bulk constants; retain rate values and reaction-type gating.
- docs/recipes/create-a-macro-command.md:169-240 and command-reference:153-177:
  replace the misleading replay narrative with registration, replay and
  receiving-tree dispatch, based on Geant4 .cc sources.
- Test files: no function extraction or assertion change is proposed; this
  completion is limited to the requested file headers.

## Comments

- macro/beam_02.in:1-3,33: SBS is supported again; chemOFF is not a CLI flag.
- macro/reaction_counter.in:4-15 and reactions.in:4-15: obsolete log/default
  claims; explain dump/exit publication and correct 1 s / 100 ps to 1e10 bins.
- macro/beam_tonneau2025.in:12-19: hand-over threshold does not guarantee an
  exact Species sample at 100 ns; remove the obsolete analysis script path.
- macro/validate_water.in:1-4 and validate_o2.in:1-4: active commands do not
  select SBS. Label historical references accurately, leave commands unchanged.
- Farokhi electron headers: threshold is not an exact-loss guarantee (a step
  can overshoot). The meesungnoen and 0pct_elossmin_2p5pct macros currently
  have identical active commands, so cannot demonstrate a solvation contrast.
- Farokhi proton headers: describe actual beam/concentration/box settings;
  retain escaping-secondary caveat. Do not present historical LET/range/runtime
  estimates or unverified paper claims as current guarantees.
- Every macro/*.in gets a purpose header and non-obvious initialization/endTime
  ordering notes. Do not alter even one active command or its order.
- All 17 test/*.cc headers gain dependency/thread notes; filesystem tests are
  not pure. Preserve the GetValueOf memory-corruption warning and all tests.
- test/Tonneau2025TableTest.cc:5-6: describe the need for a Chemistry-specific
  integration run rather than imply the default smoke run exercises HO3 creation.
  Portable-source test headers distinguish their source requirements from the
  existing CMake targets that still link Geant4 (no link changes).

## Naming

- Use the project glossary in new prose. Preserve every public API, macro
  command, output filename, manifest key, test target and macro filename.
- Replace stale project numeric source anchors in maintained recipes with
  stable file/method references. Keep versioned Geant4 .cc source evidence.

## Documentation

- .claude/geant4-instructions.md:31,69,113: tool-neutral long-run guidance,
  current Qt DLL location, Debug vcpkg HDF5 PATH-before-ctest note and PATH
  restoration before release runs. Task-specific smoke settings take precedence.
- CLAUDE.md:13,27,35-36,53,65: distinguish tested Geant4 11.4.1 from unverified
  older compatibility; output fallback; MT override condition; filesystem
  effects; stale geometry anchor and spatial-size estimate.
- docs/output/README.md:25, layouts/cwd.md:3 and sim-output/SKILL.md:127,150-167:
  normal exeDir/results root has an existing cwd fallback; macro must be argv[1].
- docs/output/layouts/*.md: before/after initialization annotations in command
  snippets need # so copied macro lines remain valid.
- create-a-macro-command.md:15,26,91-94: UI tree is per receiving manager;
  GetCurrentValue is not an environment-variable query; cite GenericMessenger's
  constructor for its directory allocation, not G4UImessenger::CreateDirectory
  (which checks for an existing directory).
- add-a-reaction-or-molecule.md:23,97,184-239: distinguish consumable scavengers
  from buffers; fix nonexistent class name; Extra-molecule hooks need not run
  exactly twice; bulk builders are called during per-thread process setup.
- recipes/README.md:17-18: species recipe is Geant4-specific; mesoscopic
  aggregate counts and HDF5 spatial snapshots have different recipes.
- export-data-as-hdf5.md:74-82: stale TimeStepAction line anchors.
- docs/cloud-work/mcp-prompt.md:6: no G4Vox dependency. Correct context only;
  the archived MCP-server task is not part of this work.
- ADR 0004: add a factual note tying the one-process restriction to the default
  G4PhysicsListHelper registration path. Leave all ADR decisions unchanged.

## Source evidence

Geant4 11.4.1: G4UIcommand.cc:54-71 registers commands; G4UImanager.cc:539-590
resolves commands, queues broadcasts and invokes DoIt; G4WorkerRunManager::DoWork
replays worker command lists. G4GenericMessenger.cc:73-78 allocates its directory;
SetNewValue:228-274 tokenizes method arguments. G4UImessenger::CreateDirectory
checks the tree before allocating. G4ReactionTableMessenger.cc:64-70 resets
the reaction table on /chem/reaction/UI. G4PhysicsListHelper.cc:435-456 rejects
nonduplicable type/subtype registrations; :978-986 marks scavenger nonduplicable;
G4DNAScavengerProcess.cc:54 assigns its subtype. Project source supplies the
actual default values, callback order, outputs, parser and fallback behavior.

## Deferred discrepancies and limitations

- .claude/.claude-project.json remains unchanged: smoke is 25 keV/10 rather
  than requested 10 keV/2, outputs omit TrackLengths, benignWarnings omits the
  known WrongResolution warning, dependency >=11.0 is unverified, and
  macroArgIsFilenameOnly is narrower than MacroResolver's actual accepted paths.
- docs/visual/app-workflow.svg:98,102 and app-workflow-files.svg:150,177 still
  name G4EmDNAPhysics rather than option2. List for a later visual update.
- docs/agents/g4run.md describes an external dotfiles/tool status not established
  by this repo. Historical reviews, reference datasets, numerical comparisons
  and paper provenance are not re-certified by this pass.
- docs/literature-comparison/farokhi2023/plot_gvalues.py:46 has a stale numeric
  ScoreSpecies anchor; analysis scripts/results stay unchanged.
- Production macros and GUI are not run; validation remains the requested
  10 keV/2-event mode examples plus the source-concern statistical matrices.
- macro/reactions.in:40 passes `10,` to /control/foreach. Geant4 11.4.1
  G4UImanager.cc:370-388 forwards that token unchanged to the energy alias;
  G4ParticleGunMessenger.cc:88 registers a double-with-unit command, and
  G4UIparameter.cc:174-186 calls IsDouble, whose digit branch rejects the comma.
  Source-verified command error, not runtime-reproduced here; document it without
  removing the comma. This also meets sim's existing ignored-command-status issue.

## Verification

Audit active commands in every macro against pre-pass contents; audit test
tokens for comment-only equality; git diff --check. Run Debug build/ctest after
test-header edits and final RelWithDebInfo build. Copy comment-updated macros
to build/macro explicitly if an up-to-date build does not run its post-build
copy. Run all three examples again into fresh output directories. Reuse the
completed source matrices for statistical comparison if sim's machine code is
unchanged; explicitly report that no additional full matrix was run for prose.
Record actual results and final per-directory deliverables before push/PR.

## Completion results

- All seven source concerns are verified and committed; sim is 3128547.
- Applied the documentation edits listed above. The ADR 0004 change is an
  appended addendum only. Local Markdown file-target audit found no missing
  targets; git diff --check passed. The reviewed preview matched the applied
  scope (18 maintained documentation files, excluding this plan).
- No build or simulation was run solely for this prose commit. Macro and test
  comment completion, followed by final builds/tests/examples, comes next.
- Macro completion: purpose/setup comments reviewed in all 54 .in files;
  active command sequences match original baseline 1fcf8ba exactly, including
  the known reactions.in comma. Duplicate adjacent ordering notes were combined.
  git diff --check passed. Production macros were not executed; final exact
  example runs follow the test-header build.
- Test completion: all 17 .cc files have purpose/dependency/thread notes.
  Final token audit proves all 17 test token sequences and all 54 active macro
  command sequences match original baseline 1fcf8ba. Assertions, defaults and
  commands were not changed.
- Final verify_builds.ps1 -Phase final succeeded: Debug rebuilt the test
  sources; ctest passed 17/17 in 2.11 s with vcpkg debug/bin first on PATH.
  RelWithDebInfo sim was already up to date. The complete executable SHA-256
  equals the retained sim-matrix binary:
  1dcd251e01ea35f8f84e3fac16b9f2021eaeb500f3df74b40538711a6df91d2e.
- Copied the comment-updated .in files to build/macro, then ran all three exact
  examples into fresh final-smokes output directories (10 keV e-, two beamOn 2
  sub-runs each). SBS passed in 89.734 s, IRT in 63.859 s, mesoscopic in
  48.782 s; all returned 0, printed both markers and matched baseline output
  structures. Only mesoscopic emitted the known WrongResolution warning.
- No additional full matrix was run for these comments/prose: the final
  simulation binary and all macro command sequences are unchanged from the
  completed source validation. GUI, production macros and a dedicated
  unknown-material error-branch runtime check were not run.
- Per-directory files, categories, statistical results, behavior risks and
  deferred decisions are consolidated in
  docs/reviews/2026-10-31-readability-pass.md. Raw evidence remains under
  .scratch/readability-2026-10-29/ (ignored by Git).
