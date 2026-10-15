# Knowledge Base Refresh: Chemical reactions — kinds, data model, definition

**Date:** 2026-09-01
**Focus:** types of chemical reactions in the Geant4-DNA chemical stage and how to define them
**KB path:** `C:\DEV\GEANT4\geant4-v11.4.1-kb`
**Geant4 source:** `C:\DEV\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1` (11.4.1)

## Sources read

- `source/processes/electromagnetic/dna/utils/include/G4DNAMolecularReactionTable.hh`
  (`G4DNAMolecularReactionData` + `G4DNAMolecularReactionTable` API)
- `.../utils/src/G4DNAMolecularReactionTable.cc`
  (`ComputeEffectiveRadius` = Smoluchowski inversion `k/(4*pi*SumD*Na)`; self-reaction uses `D_A`
  alone; Onsager radius `q1 q2/(4 pi eps0 kB T)` at 293.15 K / eps_r 80.1;
  `SetReactionType(1)` -> contact radius = vdW1+vdW2, `k_diff`, `k_act = k_D k_obs/(k_D-k_obs)`,
  probability with Rs = 0.29 nm; neutral = Type II, charged = Type IV / Debye-Smoluchowski;
  temperature: `ArrehniusParam` k=A0*exp(E_R/T), `PolynomialParam` 10^(sum Pi/T^i),
  `ScaledParameterization` scales by `DiffCoeffWater(T)`)
- `.../utils/src/G4ReactionTableMessenger.cc` (`/chem/reaction/{UI,add,new,print}`; rate methods
  `Fix`/`Arr`/`Pol`/`Scale`; trailing flag only sets type when == 1; product `H2O` dropped;
  exact-name matching)
- `.../molecules/management/include/G4MolecularDissociationChannel.hh` (decay-channel API:
  `AddProduct(conf, displacement)`, `SetProbability`, `SetDisplacementType`, `SetEnergy`,
  `SetDecayTime`, `SetRMSMotherMoleculeDisplacement`)
- `.../models/src/G4DNASmoluchowskiReactionModel.cc` (SBS `FindReaction`: separation^2 < R_eff^2;
  along-step Brownian bridge)
- `.../models/src/G4DiffusionControlledReactionModel.cc` (IRT `GetTimeToEncounter`: first-passage
  `W_inf = R_eff/d`; type 0 returns bare time; type 1 adds `k_act/(k_act+k_diff)` acceptance)
- `G4ChemDissociationChannels_option1.cc` (reference decay-channel set)

## Key facts recorded

1. Five "reaction" kinds: bimolecular (reaction table), unimolecular decay (dissociation channels),
   scavenger/bulk (pseudo-first-order), electron-hole recombination, solvation. Only bimolecular
   lives in `G4DNAMolecularReactionTable`.
2. Reaction **types**: 0 = totally diffusion-controlled (default, SBS); 1 = partially
   diffusion-controlled (IRT only; activation-limited, P<1); 6/7/8 = acid-base equilibrium pairs
   (code-only, need `G4DNAScavengerMaterial`).
3. SBS must keep type 0 - `G4DNASmoluchowskiReactionModel` only reads `GetEffectiveReactionRadius()`.
   `option3` / `ChemOxygenWaterBuilder` / `DnaChemistryList` guard `if (model != SBS) SetReactionType(1)`.
4. Both `G4DNAMolecularReactionData` ctors call `ComputeEffectiveRadius()` - pass the real rate.
   `SetReactionType` must come after reactants + rate.
5. Rate units: bimolecular `k * (1e-3*m3/(mole*s))`; pseudo-first-order `k / s` (often `k*55.3/s`).
6. Macro drops a product literally named `H2O`.

## Files changed

- **chemistry-stage.md** - replaced the thin "Reactions and Reaction Table" section with
  "Chemical Reactions - Kinds, Data Model, How to Define Them": the 5-kinds table,
  `G4DNAMolecularReactionData` entry, reaction-types table, code + macro recipes,
  `G4MolecularDissociationChannel` entry, reaction-models table. Added a cross-ref from the
  time-stepping section.
- **dna-processes.md** - "Reactions vs. processes" note in the Chemical-Stage Processes section.
- **gotchas.md** - new reaction gotchas (type vs model, ctor-computes-radius, H2O product drop,
  bulk x bulk IRT throw).
