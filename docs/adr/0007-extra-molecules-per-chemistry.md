---
status: accepted
---

# A Chemistry may add molecules of its own

A **Chemistry** can now carry an optional `constructMolecules` hook, a plain function pointer on `ChemistryRegistry::Chemistry` (null by default). `DnaChemistryList::ConstructMolecule` calls the selected Chemistry's hook, when set, after the shared molecule set and the `H2O` configuration. The Tonneau2025 network needs HO3, which no stock Geant4 molecule set defines.

`ConstructMolecule` runs when the physics list is handed to the run manager (`SetUserInitialization`), before any macro command, so it cannot see a `/chem/select` from a macro. `/chem/select` therefore also calls the hook (`ChemistrySelectMessenger`, master thread, still PreInit), which is before `/run/initialize` gives every particle its process manager. Found by the first Chemistry using the hook (Tonneau2025).

This relaxes [[0002-named-chemistries]], which kept molecules shared by all Chemistries. Dissociation channels remain shared. `PureWater` and `BoscoloChem` leave the hook null and are unchanged.

## Why this is hard to reverse

Molecule definitions live in process-wide Geant4 tables (`G4MoleculeTable`) and are looked up by name from reaction tables, bulk reactions and scavenger configuration. Once a Chemistry's reaction data and macros refer to a species it creates, removing or moving that species breaks them. The hook also fixes the contract that a Chemistry owns the molecules it adds.

## Why it is surprising

A reader of `ConstructMolecule` sees a fixed set and may assume it is the whole molecule table for every run. The molecule table now depends on `/chem/select`, which must be final before `/run/initialize`. The hook runs on the master thread only, and `new G4MoleculeDefinition(...)` is not idempotent (only `G4MoleculeTable::CreateConfiguration` is), so each implementation guards against creating a definition twice.

## Trade-off

- Adding HO3 to the shared set: simplest, but every Chemistry (including `PureWater` and `BoscoloChem`) would carry an unused species, change the molecule table and perturb existing outputs and tests.
- Omitting HO3: the Tonneau2025 reactions that produce or consume it could not be represented, so the published network would be irreproducible.
- A per-Chemistry hook: costs one nullable field and one call, and keeps the existing Chemistries bit-for-bit unchanged. We chose it.

The registry header stays free of Geant4 headers: the hook is a `void (*)()`, and any Geant4 types live in the Chemistry's own source file.
