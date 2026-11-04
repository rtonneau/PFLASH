# Session: ** write the G4Utils work report** in [PLAN.md](D:/DEV/GEANT4/SIM/dnachem-min/notebooks/PLAN.md).

**Date:** 2026-11-03T19:37:06.650Z
**Status:** Grill phase complete

## Problem Statement

The executed dnachem-min notebook vertical slice proved a need for a reusable
G4Utils report.  The current library reads manifests, particle-stage species
and reactions, and mesoscopic spatial HDF5, but the notebook still performs
its own parsing for physical interactions, track lengths, pre-chemical event
records, and aggregate mesoscopic species.  It also avoids the real output
layouts that use a results index, prefixes, or subdirectories.  The requested
work is to record evidence-backed, testable library work in a separate
`notebooks/G4UTILS_REPORT.md`.

## Context & Constraints

- **Current behavior:** G4Utils `main` at `a456290` has lazy, cached
  `Dump`-based pandas accessors.  `find_dumps()` recognizes only a bare
  `Manifest.json` in the supplied directory or immediate child directories;
  `Dump` applies a manifest prefix only after a dump has been identified.
  `Dump.has_meso` and `Dump.meso` refer to spatial HDF5, not
  `SpeciesMeso.csv`.
- **Pain point:** Notebook-local readers are not a reusable public contract,
  and current discovery can mistake a ResultsIndex for a dump or miss
  prefixed/nested dumps.  The current `G` column is a total-count to
  total-deposited-energy ratio, distinct from the stored mean event-level G.
- **Dependencies:** Keep the report separate from `PLAN.md`; update PLAN
  section 3 to point at the completed report.  Do not change G4Utils or
  notebook code in this session.  Use the editable G4Utils `main` checkout as
  the implementation baseline and document released `v0.4.0` only as context.
- **Tech stack:** Python, pandas, Jupyter, the G4Utils DnaChem API, and
  dnachem-min output documentation/source.

## Success Metrics

- `notebooks/G4UTILS_REPORT.md` ties every proposed library change to an
  observed notebook need, current behavior, actual input, proposed interface
  and return data, units/normalization, optional or missing-file behavior,
  evidence, and acceptance checks.
- The report covers ResultsIndex/prefix/subdirectory discovery; physical
  interaction, track-length, pre-chemical, and aggregate-mesoscopic readers;
  and the distinction between aggregate and event-mean G values.
- The report ranks discovery first, readers second, normalization/uncertainty
  third, and the library-versus-notebook boundary last; it records unresolved
  limitations rather than inventing physics or statistical claims.
- `notebooks/PLAN.md` reflects that the report phase is complete, while its
  G4Utils implementation and notebook-series phases remain uncompleted.

## Architecture & Approach

Write a self-contained work report with an evidence and baseline section,
current-coverage inventory, prioritized gap dossiers, and a final scope and
validation section.  Each reader proposal follows the existing lazy
`Dump`/pandas shape and preserves raw, explicitly named source data.  The
report proposes discovery that understands ResultsIndex entries, a flat
unprefixed index/dump collision, prefixed manifests, and nested subdirectories.
It proposes DataFrame readers for physical interactions, track lengths,
pre-chemical records, and mesoscopic aggregate counts, with precise
normalization and missing-data contracts.  It keeps plots and scientific
interpretation in notebooks.

For particle species, retain the existing aggregate-energy `G` and document
the separately derivable mean event G (`sumG / nEvent`).  The report may
describe a sample SEM only with its exact stored-moment formula and its
independence condition; it must not claim an uncertainty for aggregate G that
the stored data cannot support.

## Assumptions & Trade-offs

This is report-only work: it does not implement or test the proposed G4Utils
interfaces.  The report can cite current dnachem-min code, docs, notebook
parsing, and the checked G4Utils revision, but it must not claim that
pre-chemical Geant4 field semantics were validated against source while the
configured Geant4 source is 11.4.1 and the active install is 11.4.3.  That
mismatch is an explicit unresolved verification item.  Backward-compatible
discovery is preferred over silently changing existing `find_dumps()` callers.

## Open Questions

- Confirm pre-chemical `FormattedText` field semantics against an aligned
  Geant4 source/install before treating the fixed-width interpretation as
  authoritative physics semantics.
- Choose stable `Simulation.subruns` identifiers when multiple prefixed dumps
  share one folder; index-relative manifest paths are a candidate.
- Decide later whether track-length summary fields become typed Manifest data
  or remain raw while reader rows carry the event-level detail.

## Notes

The user approved: a separate report; a report-only scope with PLAN section 3
updated; the editable G4Utils `main` baseline; and the stated priority order.
The report should cite the executed vertical slice and source documentation,
and should leave G4Utils implementation for PLAN section 4.
