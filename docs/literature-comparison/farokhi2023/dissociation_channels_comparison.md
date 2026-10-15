# Dissociation channels: stock scavenger example vs dnachem-min (option1)

Compared by reading the Geant4 11.4.1 sources:
`examples/extended/medical/dna/scavenger/src/EmDNAChemistry.cc` (the example Farokhi et al. 2023 built on) and
`source/physics_lists/constructors/electromagnetic/src/G4ChemDissociationChannels_option1.cc` (used by `DnaChemistryList`
for every Chemistry).

| Item | Stock scavenger example | dnachem-min (option1) |
|---|---|---|
| A¹B₁ excitation | relaxation 0.35 / dissociative decay 0.65 | same |
| B¹A₁ excitation | relaxation 0.30 / dissociative decay 0.15 / **autoionisation 0.55** | relaxation 0.175 / 2 dissociative decays 0.0325 + 0.039 / A¹B₁-type decay 0.2535 / **autoionisation 0.50** |
| Rydberg/diffuse bands (1st-3rd layer) | autoionisation 0.5 / relaxation 0.5 | same |
| Ionisation (5 shells) | OH + H3O+ (+ e_aq through solvation) | same |
| Dissociative electron attachment channel | not defined in this file | defined |
| Vibrational-excitation decay channels | not defined in this file | defined (4 channels) |

## Effect on e_aq at 1 ps

Only B¹A₁ autoionisation produces an extra e_aq (0.55 vs 0.50). B¹A₁ is a small fraction of the excitations, and excitations
are about 1/9 of the ionisation count, so the effect is a fraction of a percent.

Measured (500 keV e-, eLossMin 12.5 keV, 250 events, 0 % O2, e_aq G at 1 ps):

| Run | Solvation | G(e_aq, 1 ps) |
|---|---|---|
| dnachem-min, original | Ritchie1994 | 4.036 |
| dnachem-min, `beam_farokhi_electron_o2_0pct_meesungnoen.in` | Meesungnoen2002 | 4.150 +/- 0.006 |
| stock scavenger example | Meesungnoen2002 | 4.176 +/- 0.006 |

The solvation subtype accounts for 0.114 of the 0.140 gap (81 %). The remaining 0.026 (0.6 %) is inside the 1 % target and is
consistent in sign with the B¹A₁ autoionisation difference, but this was not isolated.

## Decision

The per-Chemistry dissociation-channel hook (planned as ticket 3 of the `eaq-g-value-gap-vs-farokhi-at-1-ps` session) was not
built: the solvation change alone meets the 1 % target, and a hook plus an ADR for a sub-percent effect is not justified.
If a tighter agreement is ever needed, the channel set to copy is the one listed above (ADR 0007 is the model for the hook).
