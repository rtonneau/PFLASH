# Knowledge Base Refresh: UHDR reaction inventory (all reactions, by build mechanism)

**Date:** 2026-09-02
**Focus:** complete list of every reaction in the UHDR example, grouped by construction mechanism,
with the Geant4-DNA framework how-to for each mechanism.
**KB path:** `C:\DEV\GEANT4\geant4-v11.4.1-kb`
**Geant4 source:** `C:\DEV\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1` (11.4.1)

## Sources read (this pass + earlier in the session)

- `examples/extended/medical/dna/UHDR/src/EmDNAChemistry.cc` (ConstructMolecule / DissociationChannels
  / ReactionTable / Process / TimeStepModel) - full
- `.../UHDR/src/ChemOxygenWaterBuilder.cc` - `OxygenScavengerReaction` (3), `CO2ScavengerReaction` (2),
  `SecondOrderReactionExtended` (45), `SetReactionType` helper
- `.../UHDR/src/ChemPureWaterBuilder.cc` - `WaterScavengerReaction` (21, all "(B)" acid-base pairs)
- `.../UHDR/src/ChemNO2_NO3ScavengerBuilder.cc` - `NO2_NO3ScavengerReaction` (3)
- `.../UHDR/src/ChemFrickeReactionBuilder.cc` - `FrickeDosimeterReaction` (4) -- **not called** by
  `EmDNAChemistry::ConstructReactionTable`
- `source/physics_lists/constructors/electromagnetic/src/G4ChemDissociationChannels_option1.cc`
  - `ConstructDissociationChannels` (~20 decay paths over 17 water electronic states)

## Structure recorded (4 mechanisms)

1. **Water dissociation channels** - `G4MolecularDissociationChannel` + `water->AddDecayChannel`,
   run by `G4DNAMolecularDissociation` rest process. Delegated to `_option1`.
2. **Reaction table** (`G4DNAMolecularReactionTable::SetReaction`) - 74 reactions:
   `SecondOrderReactionExtended` 45 + `OxygenScavengerReaction` 3 + `CO2ScavengerReaction` 2 +
   `NO2_NO3ScavengerReaction` 3 + `WaterScavengerReaction` 21. (+ 4 dormant Fricke.)
3. **`G4DNAScavengerProcess`** (`process->SetReaction(mol, data)` per molecule) - 32 reactions,
   from `EmDNAChemistry::ConstructProcess` (H, e_aq, O2, OH, "OH"->OHm, HO2, "HO_2"->HO2m, Oxygen,
   O3, H3O, H2O2).
4. **Other stage processes** - solvation, e-hole recombination, VibExcitation extension,
   `G4ChemReboundTransportation`.

## Key insight recorded

The ~24 bulk/"(B)" reactions appear in **both** mechanism 2 (reaction table, for the IRT/IRT_syn
`FindScavenging` path) **and** mechanism 3 (`G4DNAScavengerProcess`, for SBS). Deliberate dual
registration. The 45-reaction radical set (`SecondOrderReactionExtended`) is reaction-table-only;
a few (`H+H2O2`, `OH+NO2-`) are scavenger-process-only.

## Files changed

- **uhdr-specifics.md** - new section "## UHDR Reaction Inventory (every reaction, by construction
  mechanism)": the 4 mechanisms with framework how-to + full grouped reaction lists + totals table
  + SBS/IRT duality note. ChemFricke noted as provided-but-dormant.
- **chemistry-stage.md** - cross-ref from the "Defining reactions" section to the new inventory.
