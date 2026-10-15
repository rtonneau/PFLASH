# Ticket 03: scavenger-command

**Acceptance Criteria:**
- [ ] `/chem/env/scavenger <species> <value> <unit>` exists (PreInit, not broadcast) and stores entries in `DnaChemistryWorld` (`SetScavenger`, `GetScavengers`, last wins)
- [ ] `/chem/env/O2`, `SetOxygenPercent`, `GetOxygenConcentration`, `IsOxygenScavengerEnabled` and `fO2Percent` are removed
- [ ] `ConstructChemistryComponents` adds each scavenger with molarity > 0 to `fpChemicalComponent`; a species missing from the molecule table is a fatal `UnknownScavenger`; the composition log lists the scavengers
- [ ] `DnaChemistryList::ConstructReactionTable` prints a `DnaLogger` Warning `scavenger <X> is inert: ...` for every entry of `ScavengerSpec::InertSpecies`
- [ ] `macro/beam_o2.in` uses `/chem/env/scavenger O2 21 %`; the `beam.in` comment is migrated; CLAUDE.md documents the command and drops the "`/chem/env/O2` has no effect" text
- [ ] The run without a scavenger is unchanged vs ticket 01 (sorted dump, physics files); every row of the scratch-macro table in plan Task 3 Step 9 behaves as listed

**Files to Touch:**
- `header/geometry/ScavengerMessenger.hh`
- `src/geometry/ScavengerMessenger.cc`
- `header/geometry/DnaChemistryWorld.hh`
- `src/geometry/DnaChemistryWorld.cc`
- `header/chemistry/DnaChemistryList.hh`
- `src/chemistry/DnaChemistryList.cc`
- `macro/beam_o2.in`
- `macro/beam.in`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
# inside the vcvars64 wrapper
cmake --build build --config RelWithDebInfo --target sim
cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure
cd build && ./sim scav_smoke.in --dir smoke/t03 > smoke/t03.log 2>&1
diff <(sort smoke/t03/reactions_dump.txt) <(sort smoke/t01/reactions_dump.txt)
# then each scratch macro from plan Task 3 Step 9 (<line> + /run/initialize)
```

Expected:
All tests Passed. The diff is empty and the physics files are identical to `smoke/baseline/`. Scratch macros: `O2 21 mol` → fatal "unknown unit"; `CO2 1 %` → fatal "only defined for O2"; `O2 -1 %` → fatal "negative"; `H3Op(B) 1 M` → fatal "/chem/env/pH"; `Foo 1 mM` → fatal `UnknownScavenger` at init; `H2O2 1 mM` → init succeeds with the "scavenger H2O2 is inert" warning; `O2 21 %` then `O2 0 %` → no O2 in the composition and no warning; the command after `/run/initialize` → refused (illegal state), batch stops; `/chem/env/O2 21` → command not found.

**Notes:**

The command needs a plain `G4UImessenger` (`ScavengerMessenger`, three `'s'` parameters): `G4GenericMessenger` method dispatch cuts a string argument to its first token. Construct it in the `DnaChemistryWorld` constructor after the `G4GenericMessenger`, which creates `/chem/env/`. Use `G4MoleculeTable::GetConfiguration(name, false)`: the default `mustExist = true` raises Geant4's own fatal error first. The inert warning goes in `ConstructReactionTable` (master-only, once): look up the Chemistry first, and warn after `chemWorld->ConstructChemistryComponents()`. Full code and the exact CLAUDE.md wording are in plan Task 3. Stage explicit paths only (CLAUDE.md may hold unrelated uncommitted edits; see Risks).
