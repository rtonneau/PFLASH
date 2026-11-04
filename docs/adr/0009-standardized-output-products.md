# Standardized output products and selectable formats

_ADR 0009, recorded 2026-11-04 (session 2026-11-04__data-output-should-be-standardized-each-type-of-data-should-have)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

Each output product used to have its own ad hoc `.Txt` and `.csv` files with different content, so the files of one product could not be compared, and only `SpeciesMesoSpatial` existed in HDF5. Every output product now has one redesigned table schema (long layout, camelCase columns with a unit suffix, mergeable per-event sums plus mean and standard error), written identically in any combination of `txt`/`csv`/`h5`/`root` chosen per product (`/output/<product>/format`) or by the default (`/output/defaultFormat`, built-in `csv`). Files are named `<Product>.<ext>`. We accepted breaking every existing file name, column and `/chem/meso/spatialOutput` in exchange for one schema per product. `root` is written with Geant4's g4tools, so it needs no ROOT install.

Consequences: the analysis scripts, the `sim-output` skill, `docs/output/` and the Macro builder change with this work; old dumps keep the old layout.
