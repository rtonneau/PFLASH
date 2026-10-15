# Knowledge Base Refresh: Scavenger implementation

**Date:** 2026-09-01
**Focus:** `G4DNAScavengerProcess`, `G4DNAScavengerMaterial`, bulk "(B)" species vs. tracked species
**KB path:** `C:\DEV\GEANT4\geant4-v11.4.1-kb`
**Geant4 source:** `C:\DEV\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1` (11.4.1)

## Trigger

`/kb-query "explain how scavengers are implemented and what are the differences with regular
species"` found only scattered mentions in the KB (no mechanism entry) -> answered from source,
then persisted here.

## Sources read

- `source/processes/electromagnetic/dna/processes/{include,src}/G4DNAScavengerProcess.*`
  (ctor: subtype 66, PostStep only, `fProposesTimeStep`; `BuildPhysicsTable` grabs the material
  from `G4Scheduler`; `PostStepGetPhysicalInteractionLength` = Gillespie propensity `k*[X]`,
  water special-cased to `k`, returns a negated time; `PostStepDoIt` builds product tracks /
  bumps scavenger counts; `SetReaction` throws if called post-init)
- `source/processes/electromagnetic/dna/utils/{include,src}/G4DNAScavengerMaterial.*`
  (`Reset()` rebuilds `fScavengerTable = floor(N_A * conc * V)` from the chemistry world, called
  each `G4Scheduler::Process()`; `Add/Reduce...ForMaterialConf` no-op for H2O/H3Op(B)/OHm(B);
  `IsEquilibrium(type)` true for non-{6,7,8}; `SetCounterAgainstTime` + `fCounterMap`)
- `source/processes/electromagnetic/dna/molecules/.../G4ChemDissociationChannels_option1.cc`
  ("(B)" configs created with D = 0; no plain "H2O")
- `source/processes/electromagnetic/dna/utils/include/G4VChemistryWorld.hh` (`fpChemicalComponent`)
- `source/processes/electromagnetic/dna/models/src/{G4DNAMakeReaction,G4DNAGillespieDirectMethod}.cc`
  (IRT/IRT_syn path: reaction-table "(B)" entry + `FindScavenging` from the scavenger material)
- `examples/.../UHDR/src/{EmDNAChemistry,ChemistryWorld,ChemOxygenWaterBuilder}.cc`
- this project: `src/DnaChemistryList.cc`, `src/DnaChemistryWorld.cc`

## Key findings recorded

1. A scavenger = a **scalar concentration** (`G4VChemistryWorld::fpChemicalComponent` ->
   `G4DNAScavengerMaterial::fScavengerTable`), not `G4Molecule` tracks. The "(B)" configs have D = 0.
2. SBS: `G4DNAScavengerProcess` (discrete, per diffusing molecule). IRT/IRT_syn: reaction-table
   "(B)" entry consumed by `G4DNAGillespieDirectMethod::FindScavenging`. -> register bulk reactions
   in BOTH places (UHDR and `DnaChemistryList` do).
3. Propensity `alpha = N_bulk * k / (V * N_A)` = `k*[X]`; water special-cased (`alpha = k`, rate
   already first-order `k*55.3/s`).
4. Consumed scavenger pool decrements, **except** H2O / H3Op(B) / OHm(B) (fixed pH assumption).
   Dissolved O2 depletes.
5. Segfault trap: `PostStepGetPhysicalInteractionLength` has no null-check on `fpScavengerMaterial`;
   it must be set on the thread-local scheduler before physics tables build.
6. `G4DNAScavengerProcess` needs a plain `"H2O"` config (member init) - not created by `_option1`.

## Files changed

- **chemistry-stage.md** - new section "Scavengers - Bulk Species vs. Tracked Species"
  (comparison table + `G4DNAScavengerMaterial` + `G4DNAScavengerProcess` entries + SBS vs IRT
  dispatch table); refreshed the stale `dnachem-min` note in the `G4VChemistryWorld` entry.
- **dna-processes.md** - expanded the `G4DNAScavengerProcess` row + gotcha, cross-linked.
- **gotchas.md** - new "Scavengers" subsection (null-material segfault, missing "H2O", post-init
  `SetReaction`, fixed water/ion pools, SBS-vs-IRT dual registration).
- **uhdr-specifics.md** - cross-reference from the reaction-builder table to the mechanism section.
