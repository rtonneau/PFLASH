# Recipes

Seed material for future Claude skills: a recipe per recurring development task
in this project, written once the task has been done for real so the pattern
is grounded in actual code, not aspiration. Each recipe follows the same fixed
section skeleton:

- **What this is** — the task in one or two sentences.
- **Generic pattern** — the Geant4/Geant4-DNA-level shape of the task, independent of this project.
- **dnachem-min reference implementation** — where it is done here, with file paths.
- **Serial vs MT** — what changes, if anything, between the Serial and MT run managers.
- **See also** — related recipes, ADRs and docs.

## Index

- [`create-a-scorer.md`](create-a-scorer.md) — add a new `G4VPrimitiveScorer` or counter driven from a stepping/tracking action.
- [`create-a-species-counter.md`](create-a-species-counter.md) — add a portable counter that tallies chemical species over time.
- [`mesoscopic-3d-species-counting.md`](mesoscopic-3d-species-counting.md) — count species per cell on the mesoscopic mesh, including spatial snapshots.
- [`export-data-as-hdf5.md`](export-data-as-hdf5.md) — write structured per-event data to HDF5 following the `MesoSpatialFile` pattern.
- [`create-a-macro-command.md`](create-a-macro-command.md) — add a new `/command` via a messenger class.
- [`dnachem-min-macro-command-reference.md`](dnachem-min-macro-command-reference.md) — reference listing of every macro command this project defines.
- [`add-a-new-chemistry.md`](add-a-new-chemistry.md) — add a new named Chemistry (molecule set, reactions, bulk reactions) to the registry.
- [`add-a-reaction-or-molecule.md`](add-a-reaction-or-molecule.md) — extend an existing Chemistry's reaction table or molecule set.
- [`add-a-per-run-counter.md`](add-a-per-run-counter.md) — add an accumulator fed from `RunAction::EndOfRunAction` and persisted across `/run/beamOn` calls.

Each recipe's own "See also" section cross-references related ADRs and docs,
for example `docs/output/README.md` for output-file conventions.
