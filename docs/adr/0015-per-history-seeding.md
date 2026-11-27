# Stage-2 replay reseeds per phase-space history

_ADR 0015, recorded 2026-11-26 (session 2026-11-26__phase-space-source)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

Several chemistry-stage runs (pH, dissolved O2, end time) must be able to start from the same phase-space files, and a repeated run must reproduce the same tracks in the same order. So stage 2 reseeds the engine at each event from (stage-2 base seed, file ID, history index) instead of relying on the run seed and the event order. The same history then gets the same physical stage in any range split, under any thread count and across chemistry variants (chemistry draws come after physics within an event). The price is that results no longer depend on `/random/setSeeds` alone. The run manifest has to record the base seed, the file IDs and digests, and the ranges used.

## Addendum (ticket 04, 2026-11-27): which variants share the physical stage

The physical stage is shared only among the variants that have the chemistry on: pH, dissolved O2 and the end time. With the Geant4-DNA chemistry active, the e_aq thermalisation model samples the displacement of each solvated electron during tracking (`G4DNAOneStepThermalizationModel`, only when `G4DNAChemistryManager::IsActivated()`). Those draws shift the random stream of the rest of the history. So the chemistry on and off are two different samples of each history. Their per-history deposits differ, and their means agree (7749 histories: 589.1 +/- 11.7 eV with the chemistry on, 597.7 +/- 12.0 eV off; `examples/phase-space-source/README.md`, "Chemistry"). Compare chemistry variants with the chemistry on in every run, and do not mix in a physics-only run.

The chemistry stage itself is not bit-reproducible from one process to the next. `G4DNAScavengerProcess` picks a molecule's bulk partner in the order of a map keyed by configuration pointers, and that order follows the heap addresses of each process (README, "Chemistry reproducibility"). The shared part is therefore the physical stage and the pre-chemical species; the chemistry stages of two variants are independent samples.
