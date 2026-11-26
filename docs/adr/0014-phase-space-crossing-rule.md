# Phase space records every inward crossing; stage 1 kills nothing

_ADR 0014, recorded 2026-11-26 (session 2026-11-26__phase-space-source)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

Stage 1 of `examples/phase-space-source` (500 keV e- in a water phantom) writes one phase-space record for every inward crossing of a scoring box by any track, kills nothing, and transports the box and a shell around it with the same Geant4-DNA physics as stage 2, which kills each replayed particle when it leaves the box. Killing on first entry was rejected: it drops returns and the secondaries of excursions outside the box, and it would make the boxes of a **Scoring lattice** shadow each other. Because the medium is left untouched, a broad uniform beam can be replaced by firing uniformly over one lattice cell of pitch P (fluence N_primaries / P^2), so a 4 um box at 1 mm depth receives enough primaries. The catch: a recorded re-entry of a secondary born inside the box in stage 1 is not correlated with the secondary that stage 2 draws itself, so the result is unbiased in the mean but not track by track.
