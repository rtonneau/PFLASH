# Ticket 06: recipe-create-a-macro-command

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/recipes/create-a-macro-command.md` documents both messenger styles used in this codebase: a plain `G4UImessenger` subclass with explicit `G4UIcmdWith*` command objects (e.g. `src/chemistry/MesoMessenger.cc`, `src/chemistry/ChemistrySelectMessenger.cc`), and `G4GenericMessenger` with `DeclareProperty`/`DeclareMethod` (e.g. `src/geometry/DnaChemistryWorld.cc`, `src/chemistry/DnaChemistryList.cc:155-180`), each verified against its actual source.
- [ ] States when to pick a new `G4UImessenger` on an existing directory (e.g. `/chem/select`, a plain messenger attached to the directory `G4DNAChemistryManager` already created, per `src/chemistry/ChemistrySelectMessenger.cc`) vs. a `G4GenericMessenger` owning a new directory.
- [ ] Documents the `DeclareMethod` multi-token string-argument truncation pitfall and the `DeclareProperty` fix, per the project's own prior finding (verify against `src/chemistry/DnaChemistryList.cc`'s `DeclareProperty` usage at line 171 and its surrounding comment, not from memory alone).
- [ ] Includes a `## Serial vs MT` section that explicitly verifies against `G4MTRunManager` source (not general Geant4 knowledge) whether a `PreInit`-state command issued once in a macro is replayed identically to every worker thread before `/run/beamOn`, and states the confirmed mechanism with a citation; if the exact replay mechanism cannot be pinned to a specific source line in the time available, say so explicitly rather than asserting it.

**Files to Touch:**
- `docs/recipes/create-a-macro-command.md`

**Verification Step:**

Run:
```bash
grep -q "^## Serial vs MT" docs/recipes/create-a-macro-command.md && grep -qi "DeclareMethod" docs/recipes/create-a-macro-command.md && grep -qi "PreInit" docs/recipes/create-a-macro-command.md && echo OK
```

Expected:
`OK`

**Notes:**

This is the one recipe most likely to tempt an un-verified claim about Geant4 kernel MT internals (how `G4MTRunManager` broadcasts PreInit commands to workers) — the acceptance criteria deliberately require checking it against `geant4SourcePath` rather than relying on general Geant4 familiarity; an honest "unverified, here is what the source would need to show" is acceptable, a confident guess is not.
