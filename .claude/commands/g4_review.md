---
description: Use when auditing a Geant4-DNA project for physics, chemistry, MT or lifetime problems before planning fixes; writes a report for /gps scout --from
argument-hint: [branch | PR number | path]
allowed-tools: Read Grep Glob Agent Edit(docs/reviews/**) Bash(git status *) Bash(git diff *) Bash(git log *) Bash(git rev-parse *) Bash(git show *) PowerShell(git status *) PowerShell(git diff *) PowerShell(git log *) PowerShell(git rev-parse *) PowerShell(git show *)
---

Review this Geant4-DNA project and write the findings to a Markdown file that
`/gps scout --from` can turn into sessions. This is read-only: do not edit code,
fix builds, or commit. The only file you may write is the report. Do not build unless I ask.

## Scope
Target: $ARGUMENTS (branch, PR number, path, or empty).
- Empty + uncommitted changes → review `git diff HEAD`, reading surrounding code as needed.
- Empty + clean tree → review the whole project, excluding build*/, generated and vendored code.
- Linked libraries are dependencies: review how this project calls them, not their
  internals, unless I name them as the target.
Don't stop to ask about scope; pick the most reasonable reading and state it in the report.

## Step 1 — Facts before judgment
1. **Project rules.** Read CLAUDE.md, CONTEXT.md, REVIEW.md, CONTRIBUTING, `docs/adr/*` and
   any docs/ conventions. Their priorities override the defaults below.
   - Decisions recorded in an ADR or in the stated scope are **not findings**. Never
     recommend something the project scope explicitly excludes. If a recorded decision looks
     wrong, put it in Open questions, citing the ADR.
   - Read earlier reports in `docs/reviews/` and open issues under `.scratch/`. Don't
     re-report a known finding; reference it (`known: <path>`) or say it is now fixed.
2. **Geant4 reference.** Resolve paths per `~/.claude/CONFIG_RESOLUTION.md`
   (`.claude/.claude-project.json`: `geant4SourcePath`, `kbPath`). Detect the Geant4 version
   from the install/source path or CMake, never assume it. For any finding that depends on
   Geant4-DNA API behaviour: KB first, then the `.cc` in the source tree (not only the `.hh`).
   If you can't verify, say so and set Confidence to Needs check.
3. **Build facts.** From CMake/config: C++ standard, Geant4 version, run manager type
   (MT/tasking/sequential, and how it is selected), compiler/platform assumptions.
4. **DNA wiring.** Physics constructor (G4EmDNAPhysics_optionN or custom), region-specific
   activation, materials in DNA regions, energy ranges; chemistry (constructor, time-step
   model, end time and where it is set, reaction table, scavengers, pH, time binning).
5. **Entry points.** main(), ActionInitialization (Build/BuildForMaster), DetectorConstruction
   (Construct/ConstructSDandField), primary generator, run/event/tracking/stepping actions,
   time-step/chemistry actions, scoring/output, messengers and macros.

## Step 2 — Review, in priority order
1. **Silent physics errors:** DNA models outside their validity (material, energy range),
   wrong constructor for the stated intent, missing or wrong units (eV, nm, ps, mol/L),
   double-counted or mis-scored energy deposits, wrong step/track status logic.
2. **Chemistry configuration:** chemistry disabled or misconfigured; rate-constant units and
   scavenger concentration conversions; reaction table vs the selected model/Chemistry;
   end-time or time-bin settings silently overwritten by later code (check the order of
   macro commands vs. code that runs at `/run/initialize`); state that persists or resets
   incorrectly across `/run/beamOn`.
3. **MT correctness and reproducibility:** shared mutable statics/globals/singletons without
   G4ThreadLocal or a lock, SDs or fields built outside ConstructSDandField, actions in the
   wrong Build/BuildForMaster, incorrect run merging (G4Accumulable, G4AnalysisManager, custom
   `Run::Merge`), per-thread output file collisions, RNG seeding, chemistry thread safety.
   Check the code under **every** run manager mode the project supports, not just the default.
4. **Lifetime and memory:** deleting kernel-owned objects, leaking user-owned ones, pointers
   dangling across runs, per-event allocations that grow, UB (uninitialized members,
   out-of-bounds, signed overflow).
5. **API misuse and error handling:** unchecked nullptr from G4 lookups (materials, volume
   stores, particles), G4Exception usage (severity fits the failure), messenger commands
   without validation, `G4GenericMessenger::DeclareMethod` with multi-token string arguments
   (truncated; `DeclareProperty` is needed), `G4UnitDefinition::GetValueOf()` called from
   Debug test binaries linked to a differently built Geant4, geometry/physics changes between
   runs without notifying the kernel.
6. **Performance:** I/O, string building or store lookups inside SteppingAction; per-step
   allocation.
7. **Maintainability:** only where the repo's conventions require it or it causes real confusion.

Rules:
- Every finding cites file:line and evidence. No evidence, no finding.
- If something looks deliberate (unusual DNA config, custom model), put it in Open questions.
- Fewer well-supported findings beat many speculative ones.
- For a large codebase, split areas across subagents (physics/geometry, actions/MT, chemistry,
  output), then merge and deduplicate.

## Step 3 — Write the report
Write `docs/reviews/YYYY-MM-DD-<short-slug>-review.md` (create the directory if missing;
if the file exists, add a `-2` suffix rather than overwriting) with exactly this structure:

# <Project> review — <target>
**Target:** … · **Commit:** <short sha> · **Date:** …
**Environment:** C++NN · Geant4 x.y.z · MT/sequential · DNA physics: … · Chemistry: …

## Summary
3–5 sentences, ending with the IDs to fix first.

## Architecture tour
At most 20 lines: entry points and how Geant4-DNA is wired.

## Findings
### F-001 — <short title>
- **Severity:** Critical | High | Medium | Low
- **Area:** physics | chemistry | MT | memory | API | performance | tests | maintainability
- **Location:** `src/File.cc:42` (`Class::Method`)
- **Confidence:** Confirmed | Likely | Needs check
- **Problem:** what is wrong and its effect (results, crash, reproducibility)
- **Evidence:** snippet of 10 lines max, or the code path
- **Fix direction:** concrete change, short snippet if useful
- **Verify with:** macro, test or command that proves the fix
- **Related:** F-00x

Sort by severity and number sequentially. Never renumber once written.
Severity: Critical = silently wrong results, or crash/UB in normal runs including MT.
High = wrong in plausible configs (MT, other DNA option, repeated beamOn), growing leak,
broken reproducibility. Medium = fragile, missing validation, perf that matters at scale.
Low = clarity, conventions.

## Testing gaps
Each gap: what to add, which findings it would catch, how to run it. Consider: fixed-energy
single-particle runs with a known reference, each run manager mode, two consecutive
`/run/beamOn`, each supported DNA physics option, chemistry on/off. Do not assume bit-exact
reproducibility: check whether the project documents that fixed-seed output is reproducible
(CLAUDE.md, memory, ADRs). If it is not, propose statistical comparisons (mean of several
baselines, a tolerance, a minimum count per compared quantity) rather than exact diffs.
Follow the project's own test procedure (build dir, config) instead of inventing one.

## Suggested sessions
Group findings into ordered work sessions, each shippable as one PR:
### S1 — <kebab-case-slug>
- **Findings:** F-001, F-004
- **Goal:** one sentence describing the outcome
- **Why together:** shared root cause or file
- **Depends on:** none | S2
Don't bundle Critical fixes with Low cleanup.

## Open questions
Q1 — … (F-00x). Only things I need to decide, including recorded decisions that look wrong.

## Not reviewed
What was skipped and why.

Finish with a short chat message: the report path, the count per severity, and the line
`/gps scout --from <path>`.
