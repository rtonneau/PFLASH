# Ticket 03: docs-adr-compile-commands

**Acceptance Criteria:**
- [ ] `CLAUDE.md`, `README.md` and the 5 macro comments use the clustered paths; `.claude/.claude-project.json` test-wiring text says `src/<cluster>/<X>.cc`.
- [ ] `CLAUDE.md` has a **Source layout** section, and its portability wording says "portable" means no dependency on other project classes beyond the named type header, with the include prefix adjusted on copy.
- [ ] `docs/adr/0003-clustered-source-layout.md` exists and follows the style of `0002-named-chemistries.md`.
- [ ] `compile_commands.json` is regenerated: it lists `src/core/…` and `src/chemistry/…` and has no top-level `src/<Name>.cc` entry.
- [ ] The stale-path sweep prints nothing; historical files (`.kb-notes/*`, `.claude/plan-class-based-chemistry.md`, older `.claude/plans/*`) are untouched.
- [ ] The memory note `portable-class-convention.md` mentions the rooted-include prefix.
- [ ] One commit: `docs: document clustered source layout and rooted includes (ADR 0003)`.

**Files to Touch:**
- `CLAUDE.md`, `README.md`, `macro/beam.in`, `macro/beam_02.in`, `macro/beam_o2.in`, `macro/reactions.in`, `macro/reaction_counter.in`
- `.claude/.claude-project.json` (line 77)
- `docs/adr/0003-clustered-source-layout.md` (new)
- `compile_commands.json` (regenerated)
- project memory `portable-class-convention.md` (outside the repo)
- `.scratch/tests/2026-09-28__source-layout-subdirs/rewrite-doc-paths.js` (git-ignored)

**Verification Step:**

Run:
```bash
grep -rnE "(src|header)/[A-Z][A-Za-z]+\.(cc|hh)" CLAUDE.md README.md macro docs CONTEXT.md .claude/.claude-project.json
grep -c "src/core" compile_commands.json
grep -cE "src/[A-Z][A-Za-z]+\.cc" compile_commands.json
git status --short
```

Expected:
the first grep prints nothing; the second count is nonzero; the third is `0`; `git status` lists only the docs, macros, ADR, `.claude/.claude-project.json` and `compile_commands.json` (no `src/`, `header/` or `test/` changes).

**Notes:**

1. **Path rewrite script** `$S/rewrite-doc-paths.js`: build a basename → path map from the real `src/**/*.cc` and `header/**/*.hh`, then rewrite each `src/<Name>.cc` and `header/<Name>.hh` occurrence (regex `\b(src|header)\/([A-Za-z0-9_]+\.(?:cc|hh))\b`) to its clustered path when the map has it. Apply it to `CLAUDE.md`, `README.md` and `macro/*.in`; leave the frozen historical files alone.

2. **Hand edits after the script:**
   - `.claude/.claude-project.json:77`: `add_executable(<X>Test test/<X>Test.cc src/<cluster>/<X>.cc [src deps]) …`.
   - `README.md`: the layout bullet "`src/`, `header/` — implementation and headers" describes the clusters.
   - `CLAUDE.md`: add this section before **Key Files**:
     ````markdown
     ## Source layout

     `src/` and `header/` are mirrored trees: `core/` (argument parsing, output directory, logging), `actions/` (run, event, tracking, stacking and stepping actions, action initialization, primary generator), `geometry/` (detector, chemistry world), `physics/` (physics list), `chemistry/` (chemistry list, registry and selection, time-step action, reaction-table dump) with `chemistry/catalog/` for the named Chemistries, and `scoring/` (species scorer, counters, run accumulator). `sim.cc` stays at the root and `test/` is flat.

     Project includes are rooted at `header/`, which is the only project include directory: `#include "chemistry/DnaChemistryList.hh"`. A new file goes in the directory of the concern it belongs to; a new Chemistry goes in `chemistry/catalog/`. Rationale: `docs/adr/0003-clustered-source-layout.md`.
     ````
   - `CLAUDE.md` portability wording (the `PureWaterReactions`, `ChemistryRegistry` and `PhysicsInteractionCounter` entries): "portable" means no dependency on other dnachem-min classes beyond the named type header; when copying to another project, adjust the include path prefix.

3. **ADR** `docs/adr/0003-clustered-source-layout.md`:
   ````markdown
   ---
   status: accepted
   ---

   # Clustered source layout with includes rooted at header/

   `src/` and `header/` are mirrored trees of subdirectories (`core/`, `actions/`, `geometry/`, `physics/`, `chemistry/` with `chemistry/catalog/`, `scoring/`); `sim.cc` stays at the root and `test/` stays flat. Every project include is rooted at `header/` (`#include "chemistry/DnaChemistryList.hh"`), and `header/` is the only project include directory, so a bare-name include is a build error.

   The two flat directories held 64 files, and finding everything about one concern meant grepping; a new Chemistry or counter had no obvious home. The build already globbed recursively, so clustering cost only file moves and an include rewrite.

   We kept `header/` as a mirror of `src/` rather than co-locating each `.hh` with its `.cc`: it preserves the `src/` + `header/` split the project follows from Geant4, and it keeps the portable `.cc`/`.hh` pairs easy to copy. We rejected keeping bare includes through a multi-directory include path: it hides which directory a header lives in and lets any file reach any header without the dependency showing in the code.

   Consequences: moving a file now means rewriting its includes; the portable files (`PureWaterReactions`, `PhysicsInteractionCounter`, …) need their include prefix adjusted when copied to another project; no rule stops one directory from including another (cross-directory edges are visible but not enforced).
   ````

4. **Regenerate `compile_commands.json`** (PowerShell):
   ```powershell
   '{"tool_input":{"file_path":"src/core/OutputDir.cc"}}' | pwsh -File .claude/hooks/update-compile-cmd.ps1
   ```
   If the hook leaves the repo-root file untouched, copy `build-ninja/compile_commands.json` over it.

5. **Memory note.** In the project memory folder, add to `portable-class-convention.md`: since the clustered layout, includes are rooted at `header/`, so portable files need their include prefix adjusted when copied to another project.

6. Run the verification commands, then commit with `git add` on the listed files only. No rebuild is needed beyond the configure the hook performs; `CONTEXT.md` is unchanged (directory names are not domain terms).
