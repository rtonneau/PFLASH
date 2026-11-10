# poisson-track-irt builds on stock Geant4; patched build kept selectable

_ADR 0010, recorded 2026-11-10 (session 2026-11-10__poisson-track-irt-stock-build)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

`examples/poisson-track-irt` needed three local Geant4 11.4.3 patches (source contacts, molecular finder, rebound fold). It now builds on a stock install by default, with `PTIRT_PATCHED_GEANT4` (checked at configure time) selecting the patched path. Instead of patching, the stock build resolves the **Startup contacts** at capture, uses its own fold at history boundaries, refuses coincident molecules except products of one startup reaction (each marked by a `productGroup`), and measures what the kernel still misses: **New-old contacts**, reactive coincident pairs, the first-packet second trial (accepted while it averages ≤ 1 expected reaction) and a box-size ratio warning below 3. Each build captures its own library from the same physics seeds, because a post-startup packet in the patched build would be trialled twice. Subclassing or copying kernel classes was rejected: the octree finder singleton is split between exe and DLL, and thread-local accessors fail on Windows.

Measured sizes, the conditions that require the patched build, and the checks not run are added when the validation runs (sections below, to be filled during implementation).
