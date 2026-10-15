---
status: accepted
---

# Clustered source layout with includes rooted at header/

`src/` and `header/` are mirrored trees of subdirectories (`core/`, `actions/`, `geometry/`, `physics/`, `chemistry/` with `chemistry/catalog/`, `scoring/`); `sim.cc` stays at the root and `test/` stays flat. Every project include is rooted at `header/` (`#include "chemistry/DnaChemistryList.hh"`), and `header/` is the only project include directory, so a bare-name include is a build error.

The two flat directories held 64 files, and finding everything about one concern meant grepping; a new Chemistry or counter had no obvious home. The build already globbed recursively, so clustering cost only file moves and an include rewrite.

We kept `header/` as a mirror of `src/` rather than co-locating each `.hh` with its `.cc`: it preserves the `src/` + `header/` split the project follows from Geant4, and it keeps the portable `.cc`/`.hh` pairs easy to copy. We rejected keeping bare includes through a multi-directory include path: it hides which directory a header lives in and lets any file reach any header without the dependency showing in the code.

Consequences: moving a file now means rewriting its includes; the portable files (`PureWaterReactions`, `PhysicsInteractionCounter`, …) need their include prefix adjusted when copied to another project; no rule stops one directory from including another (cross-directory edges are visible but not enforced).
