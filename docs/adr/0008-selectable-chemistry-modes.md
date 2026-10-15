---
status: accepted
---

# Chemistry modes are selectable: SBS, IRT_syn, and IRT_syn + mesoscopic

This supersedes the "SBS is removed" and "Keeping SBS selectable rejected" parts of [[0006-irt-syn-and-mesoscopic-chemistry]]. ADR 0006 still holds for the IRT_syn + mesoscopic model, which stays the model of record.

Three chemistry modes can be chosen before `/run/initialize`:

- **IRT_syn + mesoscopic** (default): `/process/chem/TimeStepModel IRT_syn` with `/chem/meso/enable true`. Particle-based stage up to the hand-over time, then the mesoscopic stage.
- **IRT_syn only**: `/process/chem/TimeStepModel IRT_syn` with `/chem/meso/enable false`. Particle-based stage only.
- **SBS**: `/process/chem/TimeStepModel SBS`. Step-by-step Brownian dynamics (chem1-chem6), particle-based stage only. There is no mesoscopic stage in SBS.

`/chem/meso/enable true|false` (PreInit) switches the mesoscopic stage. It only exists after IRT_syn. Combining `SBS` with an explicit `/chem/meso/enable true` is a fatal error at `/run/initialize`. Without an explicit `enable`, SBS silently runs without the mesoscopic stage.

The chemistry end time defaults to 1 us without the mesoscopic stage and 1 s with it. `/scheduler/endTime` after `/run/initialize` still overrides it. The mode and the stage state are recorded in `Manifest.json` (`timeStepModel`, `mesoEnabled`), and no `SpeciesMeso*` file is written when the mesoscopic stage is off.

**Considered options.**
- *Keep SBS only as a regression reference, not selectable*: rejected. A user who wants the SBS yields needs to reach them without editing code, and the SBS-vs-IRT comparison is only reproducible if both run from the same binary.
- *Let SBS and meso combine*: rejected. The SBS model has no mesoscopic hand-over, so an explicit request for it is an error, not a silent drop.

**Consequences.**
- **Trade-off.** The modes do not agree. SBS differs from IRT_syn + mesoscopic in its yields, and IRT_syn alone already differs from SBS with an identical reaction table. The numbers are in `docs/irt-syn-mesoscopic-validation.md`. The modes are alternatives for comparison, not interchangeable, and the mode must be stated when quoting a yield.
- **Macros.** `macro/example_sbs.in`, `example_irt.in` and `example_meso.in` run one mode each (10 keV e-, dissolved O2 21 %, two sub-runs). `example_meso.in` sets a 1 ms end time after `/run/initialize` so it stays smoke-sized.
- **Reaction types.** SBS registers every reaction as type 0 (fully diffusion-controlled, as in chem1-chem6 and the SBS reference); IRT_syn keeps the partially diffusion-controlled type 1 for the catalogue's Type II/IV pairs. `SetReactionType(1)` cannot be undone, so the Chemistry builders skip it under SBS (`ChemistryTypes::PartialReactionsEnabled()`, set in `CheckTimeStepModel`). Without this SBS drifted 12 to 30 % from its reference; with it SBS agrees with the reference within the combined statistical error (`docs/sbs-regression.md`).
- **Layering.** The mode check lives in `DnaChemistryList::CheckTimeStepModel` and runs at `/run/initialize`, so a bad combination stops before any event.
