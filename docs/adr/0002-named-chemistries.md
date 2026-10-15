---
status: accepted
---

# Named, selectable Chemistries; the acid-base buffer is per-Chemistry and may be empty

Reproducing chemistries from other papers and libraries requires more than one reaction network, so the reaction content (reaction table + acid-base buffer rates) is packaged as a named **Chemistry**, selected once per process before `/run/initialize` (`/chem/select <name>`, default `PureWater`). Molecules and dissociation channels stay shared by all Chemistries; the `DnaChemistryList` driver, `DnaChemistryWorld` and the scavenger-material plumbing are unchanged.

Each Chemistry owns its acid-base buffer list, and that list may be empty or partial: many published networks have no bulk `H3Op(B)`/`OHm(B)` buffer, and forcing it on would make them irreproducible. This relaxes [[0001-baseline-acid-base-buffer]]'s "unconditional" rule; 0001 still holds for `PureWater`, the default. The author of a Chemistry owns its physical consistency and states in its file header when the buffer is omitted.

We rejected wrapping stock Geant4 chemistry lists (`G4EmDNAChemistry_option*`) as Chemistries: they bypass `DnaChemistryWorld` and the bulk-scavenger machinery this project is built around.
