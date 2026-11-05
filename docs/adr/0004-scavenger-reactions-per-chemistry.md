---
status: accepted
---

# Scavenger concentration is environment-wide; scavenger reactions are per-Chemistry bulk reactions

Dissolved O2 is modelled as a bulk scavenger the way the Geant4-DNA `UHDR` example does it: the scavenger's concentration is part of the environment (`/chem/env/scavenger <species> <value> <M|mM|uM|%>` on `DnaChemistryWorld`, shared by all Chemistries), while the reactions it takes part in (`e_aq + O2(B) → O2⁻`, `H + O2(B) → HO2°`, `O⁻ + O2(B) → O3⁻`) are **bulk reactions** of the selected **Chemistry**, listed per molecule alongside its acid-base buffer reactions. The former acid-base list is therefore renamed the bulk-reaction list.

We rejected the "future scavenger file" anticipated by [[0001-baseline-acid-base-buffer]] — one shared scavenger layer applying the same reactions whatever Chemistry is selected. Rate constants are reaction content, which [[0002-named-chemistries]] assigns to the Chemistry (a paper's network may use different O2 rates), and Geant4 allows only one `G4DNAScavengerProcess` per molecule, so a separate layer would have had to be merged into each Chemistry's per-molecule process anyway.

**Consequences**: scavenger reactions are always registered and are inert when their bulk species has zero concentration (`G4DNAScavengerProcess` skips empty components). A scavenger set in a macro that no bulk reaction of the selected Chemistry uses only produces a warning, not an error. The `/chem/env/O2` command is removed in favour of the generic command; it never had any effect on the chemistry. While a bulk O2 concentration is set, an `O2` product of a *bulk* reaction (e.g. `O3⁻ + H2O → O⁻ + O2`) joins the bulk pool instead of becoming a track (`G4DNAScavengerProcess::PostStepDoIt`), as in the UHDR example; `O2` produced by reactions between two tracked molecules stays tracked under SBS (`G4DNAMolecularReaction` does not consult the scavenger material). Adding the O2(B) reactions changes the random-number sequence even at zero concentration, so runs are not event-by-event comparable with runs made before them, only statistically.


## Addendum (2026-10-31): registration scope of the process restriction

The one-scavenger-process restriction above is enforced by Geant4 11.4.1's
default `G4PhysicsListHelper` registration path. In
`source/run/src/G4PhysicsListHelper.cc:435-456`, a nonduplicable type/subtype
pair already on a particle causes warning `Run0111` and registration returns
false. Its default ordering table marks `fUserDefined`/`fLowEnergyScavenger`
nonduplicable (lines 978-986); `G4DNAScavengerProcess.cc:54` assigns that subtype.
This is not a claim about every possible direct process-manager registration.
The project continues grouping bulk reactions into one scavenger process per
molecule. The decision and reaction content are unchanged.

## Addendum (2026-11-05): `O2` products under IRT_syn

While a bulk O2 concentration is set, an `O2` product of a reaction between two
tracked molecules also joins the bulk pool under IRT_syn, in Serial and MT alike:
`G4DNAMakeReaction::MakeReaction` sends any product held by the scavenger material
to the pool, as in the UHDR example. So `Species.*` has no `O_2^0` row in that
configuration; SBS keeps the tracked `O2` described above, and its row. Pure water
keeps the row in every mode, because `O2` is not in the material there.

`G4DNAMakeReaction` caches the material in its constructor, so the material must be
installed before the time-step model is built. A worker builds the model inside
`G4DNAChemistryManager::Initialize()`, Serial at the first event. The material used
to be installed after `Initialize()`, which left workers with a null pointer: MT kept
this `O2` tracked until the hand-over, while Serial did not. It also skipped
`SetEquilibrium`, which no Chemistry here depends on (none uses reaction types 6-8).
