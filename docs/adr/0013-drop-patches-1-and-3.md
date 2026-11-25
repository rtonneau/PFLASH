# Future kernels drop patches 1 and 3; patch 2 stays

_ADR 0013, recorded 2026-11-19._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

The patch-necessity suite ([patch-necessity.md](../dose-rate-pulse-study/patch-necessity.md)) showed that patch 1 (octree finder, coincident molecules) and patch 3 (rebound fold in `G4ChemReboundTransportation`) each fix a real Geant4 11.4.3 defect, but no dose-rate study history reaches either one: the patch-1 trigger (three molecules of one species at one point) is unreachable with the built-in Chemistries, and no IRT_syn step overshoots a wall by more than the box. Future Geant4 kernels for this project are therefore built **without patches 1 and 3**; only patch 2 (`timed-injection/source-contacts-experimental.patch`) remains in the patch set.

Patches 1 and 3 stay on record: the files remain in `patches/geant4/11.4.3/`, their fixtures in `test/dose-rate-patches/`, and their descriptions in [geant4-issues.md](../dose-rate-pulse-study/geant4-issues.md) (status DROPPED). Results already produced on the `mf-ti` install (all three patches) are unaffected and stay valid.

## Consequences

- A kernel without them is a new patch set: mark it with `g4-study mark` so the stamps record it.
- Re-apply patch 1 if a Chemistry gets a reaction or channel with three products of one species (`coincidence_audit.py` flags it as `direct>=3`), or if coincident-pair reactions start to matter.
- Re-apply patch 3 if a setting can step past a wall by more than the box length (very small boxes, long late steps), since the stock failure is silent (molecule left outside the box).
- Nothing is rebuilt now: the current `mf-ti` install keeps all three patches until the next kernel is built.
