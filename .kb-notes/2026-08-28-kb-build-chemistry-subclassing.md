# Knowledge Base Refresh: Custom Chemistry Definition (subclassing pattern)

**Date**: 2026-08-28 (Refresh)
**Focus**: Defining a bespoke chemical stage by subclassing `G4VChemistryWorld`,
`G4VUserChemistryList`, and `G4VPhysicsConstructor`, as done in the Geant4-DNA `UHDR` example.
**KB Path**: `C:\DEV\GEANT4\geant4-v11.4.1-kb`
**Geant4 source**: `C:\DEV\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1` (11.4.1)

## Motivation

The prior UHDR refresh described `ChemistryWorld` / `EmDNAChemistry` from headers and inference,
and contained inaccuracies:
- claimed `class EmDNAChemistry : public G4VPhysicsConstructor` (it is **also** `G4VUserChemistryList`);
- invented UI commands (`/UHDR/chemistry/pH`, `/UHDR/chemistry/scavenger Pure_Water`) — real dir is `/UHDR/env/`;
- claimed `G4EmDNAChemistry_option3 = "IRT + synchronized"` — the time-step model is a runtime choice;
- wrong init flow (builders are not called from `ConstructChemistryComponents`).

This pass re-grounds everything against `.cc` sources.

## Sources read

- `source/processes/electromagnetic/dna/utils/include/G4VChemistryWorld.hh`
- `source/processes/electromagnetic/dna/utils/include/G4VUserChemistryList.hh` + `src/...cc`
- `source/processes/electromagnetic/dna/utils/src/G4DNAChemistryManager.cc` (`InitializeMaster`/`InitializeThread`/`Deregister`/`SetChemistryList`)
- `source/physics_lists/constructors/electromagnetic/src/G4EmDNAChemistry_option3.cc` + `.hh` (canonical dual-inheritance list)
- `source/physics_lists/constructors/electromagnetic/{src,include}/G4EmDNAChemistry{,_option1,_option2}` (time-step model = SBS)
- `source/physics_lists/constructors/electromagnetic/include/G4ChemDissociationChannels_option1.hh`
- `source/processes/electromagnetic/utils/src/G4EmLowEParametersMessenger.cc` (`/process/chem/TimeStepModel`)
- `examples/extended/medical/dna/UHDR/`: `UHDR.cc`, `src/PhysicsList.cc` + `include/PhysicsList.hh`,
  `src/EmDNAChemistry.cc` + `include/EmDNAChemistry.hh`, `src/ChemistryWorld.cc` + `.hh`,
  `src/DetectorConstruction.cc` + `.hh`, `src/ActionInitialization.cc`, `src/ChemPureWaterBuilder.cc`

## Key source-grounded facts recorded

1. **Dual inheritance**: `EmDNAChemistry : public G4VUserChemistryList, public G4VPhysicsConstructor`
   (same as every `G4EmDNAChemistry*`). Constructor: `G4VUserChemistryList(true)` +
   `G4DNAChemistryManager::Instance()->SetChemistryList(this)`.
2. **`true` flag = ownership**: `~G4VUserChemistryList` → `Deregister` → `release()` (not `delete`)
   when `IsPhysicsConstructor()`; owning `PhysicsList` frees it once. Flag `false` → double free.
3. **Manager dispatch**: `InitializeMaster()` calls `ConstructDissociationChannels()` +
   `ConstructReactionTable(table)`; `InitializeThread()` calls `ConstructTimeStepModel(table)` per
   worker; `ConstructMolecule()`/`ConstructProcess()` driven by the `PhysicsList`.
4. **UHDR `PhysicsList` bypasses `RegisterPhysics`** — holds `unique_ptr<EmDNAChemistry>`, calls
   `->ConstructParticle()` / `->ConstructProcess()` by hand.
5. **`G4VChemistryWorld`** = pure-virtual `ConstructChemistryBoundary()` +
   `ConstructChemistryComponents()`; holds `unique_ptr<G4DNABoundingBox>` +
   `map<const G4MolecularConfiguration*, double>` (species→molarity). No geometry, no messenger.
6. **Not auto-wired**: `DetectorConstruction` owns the `ChemistryWorld`;
   `EmDNAChemistry::ConstructProcess()` `dynamic_cast`s the detector to get it, calls
   `ConstructChemistryComponents()`, passes `GetChemistryBoundary()` to
   `G4ChemReboundTransportation` / `BoundedBrownianAction` / `G4DNAScavengerProcess`;
   `ActionInitialization::Build()` does `new G4DNAScavengerMaterial(chemWorld)` +
   `G4Scheduler::SetScavengerMaterial(...)`.
7. **UI**: `/UHDR/env/pH`, `/UHDR/env/volume <v> <unit>` (PreInit only),
   `/UHDR/env/scavenger <species> <value> <M|mM|uM|%>`.
8. **Builders** are static-method classes (`ChemPureWaterBuilder::WaterScavengerReaction(table)`
   etc.), called from `EmDNAChemistry::ConstructReactionTable()`, not subclasses.
9. **Time-step model** = `/process/chem/TimeStepModel IRT|SBS|IRT_syn` via `G4EmParameters`;
   `_option3` honours all three, UHDR's `ConstructTimeStepModel` handles only `SBS`/`IRT_syn`.
10. Rate unit idiom: `k * (1e-3 * m3 / (mole * s))` = M^-1 s^-1; first-order `k / s`.

## Files modified

- **chemistry-stage.md** — new section "Custom Chemistry List — Subclassing `G4VUserChemistryList`
  (+ `G4VPhysicsConstructor`)" (with `G4VUserChemistryList` + `G4ChemDissociationChannels` entries);
  replaced the thin tail section with a proper `G4VChemistryWorld` entry.
- **uhdr-specifics.md** — rewrote "Chemistry World and Builders" and "EmDNAChemistry (UHDR Variant)"
  with corrected class signatures, UI commands, wiring, and init flow; added reaction-builder table.
- **physics-lists.md** — corrected "Available Chemistry Constructors" (removed the false
  "option3 = IRT+synchronized"), documented the two registration patterns.
- **dna-processes.md** — new section "Chemical-Stage Processes (registered by a chemistry list's
  `ConstructProcess()`)".
- **gotchas.md** — new subsection "Custom Chemistry List (`G4VUserChemistryList` + `G4VPhysicsConstructor`)".

## Applicability to dnachem-min

`dnachem-min` currently uses stock `G4EmDNAChemistry_option3` + macro `/chem/reaction/add`. If the
project ever needs bulk/scavenger chemistry (dissolved O2, pH, Fricke), the UHDR pattern is the
template: add a `G4VChemistryWorld` subclass owned by `DetectorConstruction` and a
`G4VUserChemistryList`+`G4VPhysicsConstructor` subclass — but per project scope, only if the study
requires it (no UHDR pulse structure).
