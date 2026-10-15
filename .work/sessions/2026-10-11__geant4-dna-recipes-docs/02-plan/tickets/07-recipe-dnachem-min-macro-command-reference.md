# Ticket 07: recipe-dnachem-min-macro-command-reference

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/dnachem-min-macro-command-reference.md` enumerates dnachem-min's actual custom commands grouped by directory, each with its state (PreInit/Idle) and source file: `/run/*` (`dumpDataAndReset`, `dumpDataAndResetToDir`, `outputDir` — `src/scoring/RunAccumulatorMessenger.cc`, `src/core/OutputDirMessenger.cc`), `/chem/select`, `/chem/list` (`src/chemistry/ChemistrySelectMessenger.cc`), `/chem/env/*` (`pH`, `halfBox`, `scavenger` — `src/geometry/DnaChemistryWorld.cc`, `src/geometry/ScavengerMessenger.cc`), `/chem/meso/*` (`src/chemistry/MesoMessenger.cc`), `/chem/reaction/*` (`dump`, `timeBinsFixed`, `timeBinsList` — `src/chemistry/DnaChemistryList.cc:155-180`), `/dnaLogger/verbose` (`src/core/DnaLoggerMessenger.cc`), `/primaryKiller/*` (`src/scoring/PrimaryKiller.cc`), and the built-in `/process/chem/TimeStepModel`/`/scheduler/endTime` commands this project relies on.
- [ ] Each entry is verified against the actual `new G4UIcmd...`/`DeclareProperty`/`DeclareMethod` call in its source file (grep confirmed, not transcribed from `CLAUDE.md`'s prose alone), since `CLAUDE.md` already documents these at a narrative level and this recipe must not silently drift from or contradict it.
- [ ] Includes a `## Serial vs MT` section covering the general rule from ticket 6 (link, don't repeat) plus any command-specific exception found while enumerating (e.g. a command that is explicitly master-only or applied per-worker-thread), stated only if actually verified against source.

**Files to Touch:**
- `docs/recipes/dnachem-min-macro-command-reference.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/dnachem-min-macro-command-reference.md && grep -q "/chem/meso" docs/recipes/dnachem-min-macro-command-reference.md && grep -q "/primaryKiller" docs/recipes/dnachem-min-macro-command-reference.md && echo OK
```

Expected:
`OK`

**Notes:**

This is reference content, not a how-to — grep the whole `src/` tree for `new G4UIcmd`, `DeclareMethod(`, `DeclareProperty(`, and `new G4UIcommand(` before writing, to avoid missing a command `CLAUDE.md`'s prose summary glossed over.
