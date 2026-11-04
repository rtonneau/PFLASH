---
status: captured
scope: project
created: 2026-11-04
reviews: 0
promoted_to: null
---

## Problem

Migrate notebooks 00-04 and vertical_slice to the standardized output products: first add G4Utils (C:\DEV\python\G4Utils, dump.py/estimates.py/loaders.py) readers for Species.csv/SpeciesMeso.csv (gMean, gStdErr, countMean, nEvents), Reactions.csv + ReactionsCatalog.csv and the h5/root tables, keeping the legacy Species_nt_species.csv path for old dumps; then update the notebook cells via the Jupyter MCP.

- Left open by ticket 13 of session 2026-11-04__data-output-should-be-standardized-each-type-of-data-should-have (ADR 0009): scripts, compare_reference.py and the Macro builder were migrated, the notebooks were not.
- G4Utils `dump.py` hard-codes `Species_nt_species.csv`, `Reactions_nt_reactions.csv`, `ReactionsMetadata.csv`; `estimates.py` requires `sumG`/`sumG2`/`nEvent`/`number`. The notebooks still work on dumps written before the change.

## Changelog

- 2026-11-04 — capture — G4Utils readers for the new output products, then migrate the notebooks (dnachem-min @ feat/standardized-output-formats)
