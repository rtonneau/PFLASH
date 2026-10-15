# Acid-base buffer network is baseline pure-water chemistry, not gated by O2

`DnaChemistryList` is being rewritten around a `PureWaterReactions` file containing
only the reaction table content, decoupled from `DnaChemistryWorld` wherever
possible. The O2/O2⁻/HO2/HO2⁻/O⁻/O3⁻ diffusing-species reactions fit this: they're
ordinary reactions between tracked molecules and need no bulk species.

The pH-dependent acid-base equilibria (HO2°⇌O2⁻, e_aq⇌H, HO2⁻⇌H2O2, O⁻⇌°OH, O3⁻
decay, H3Op+OHm→2H2O — UHDR's `ChemPureWaterBuilder::WaterScavengerReaction` set)
don't: they react against the bulk `H3Op(B)`/`OHm(B)` pseudo-species, whose
concentration comes from `DnaChemistryWorld`'s pH, and which only fire through a
`G4DNAScavengerProcess` registration per molecule.

We considered three options: (a) bake a fixed pH=7 into plain first-order rate
constants, keeping the file fully self-contained with no `DnaChemistryWorld`
dependency; (b) reintroduce the bulk/scavenger machinery but only for the
HO2⇌O2⁻ pair; (c) reintroduce it for the full acid-base set, unconditionally
(not gated by the old "O2 enabled" toggle).

We chose (c). pH buffering is baseline aqueous chemistry, not an O2-specific
effect — it can produce O2 from pure water radiolysis on its own (H2O2 + °OH →
HO2° → [pH eq] → O2⁻, then O2⁻ + °OH → O2). Gating it behind O2, or only including
part of the network, would be physically inconsistent (a proton/hydroxide sink
with no corresponding source elsewhere in the network).

**Consequences**: `DnaChemistryList` keeps a `DnaChemistryWorld` dependency
unconditionally (`ConstructReactionTable` always calls
`ConstructChemistryComponents()`; `ConstructProcess` always registers
`G4DNAScavengerProcess` for the ~11 species in the acid-base set). The old
`IsOxygenScavengerEnabled()` toggle's remaining scope narrows to introducing an
*exogenous* dissolved-O2 population — deferred to a future scavenger file — since
the acid-base network no longer depends on it.

**Superseded in part by [[0002-named-chemistries]]**: the network stays unconditional
for the default `PureWater` Chemistry, but other named Chemistries may omit it.

**Superseded in part by [[0004-scavenger-reactions-per-chemistry]]**: the exogenous
dissolved-O2 population is not a separate scavenger file; its reactions are
per-Chemistry bulk reactions, its concentration is set by `/chem/env/scavenger`.
