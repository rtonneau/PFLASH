# Ticket 03: extra-molecules-hook

**Model:** sonnet
**Effort:** high

**Acceptance Criteria:**
- [ ] `ChemistryRegistry::Chemistry` gains a fourth field `void (*constructMolecules)() = nullptr;`; existing brace initialisations in `BuiltInChemistries.cc` and `ChemistryRegistryTest.cc` still compile and `Register` still accepts a null value.
- [ ] `DnaChemistryList::ConstructMolecule` calls the selected Chemistry's `constructMolecules` when non-null, after the shared molecules and the `H2O` configuration.
- [ ] `ChemistryRegistryTest` gains a test that a Chemistry with a null `constructMolecules` registers, and one that a non-null value is stored and returned by `Selected()`.
- [ ] The default (`PureWater`) and `BoscoloChem` behaviour is unchanged.
- [ ] `docs/adr/0007-extra-molecules-per-chemistry.md` records the decision (hard to reverse, surprising, a trade-off against adding HO3 to the shared set or omitting it) and notes that it relaxes ADR 0002.

**Files to Touch:**
- `header/chemistry/ChemistryRegistry.hh`
- `src/chemistry/ChemistryRegistry.cc`
- `src/chemistry/DnaChemistryList.cc`
- `test/ChemistryRegistryTest.cc`
- `docs/adr/0007-extra-molecules-per-chemistry.md`

**Verification Step:**

Run:
```bash
ctest --test-dir build-ninja --output-on-failure
```

Expected:
All listed tests pass, none `Not Run`, after building every test target in `build-ninja/` with the MSVC environment.

**Notes:**

`ConstructMolecule` runs on the master thread during `/run/initialize`, after `/chem/select` is final (`SelectedChemistry` is in `DnaChemistryList.cc`). `new G4MoleculeDefinition(...)` is not idempotent; only `G4MoleculeTable::CreateConfiguration` is, so the hook implementations must guard against double creation. Keep `ChemistryRegistry.hh` free of Geant4 headers.
