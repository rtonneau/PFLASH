# Implementation Plan

**Session:** ** write the G4Utils work report** in [PLAN.md](D:/DEV/GEANT4/SIM/dnachem-min/notebooks/PLAN.md).
**Date:** 2026-11-03T20:53:45.027Z
**Estimated effort:** 1–2 hours

## Strategy

Create the separate, evidence-backed work report first, then update the
existing roadmap to mark only its report phase complete.  The single ticket
keeps the report and the corresponding PLAN status change together, with no
G4Utils or notebook implementation work.

## Tickets Overview

- **Ticket 1:** Write the G4Utils work report and record its completion in the
  notebook roadmap.

## Sequencing Rationale

The report is the planned basis for later library changes, so it must be
completed before any G4Utils implementation ticket.  The PLAN update belongs
with the report because it records the same completed milestone.

## Risks & Mitigation

- **Risk:** Unverified source-level pre-chemical semantics could be presented
  as established physics. → **Mitigation:** State the 11.4.1 source/11.4.3
  install mismatch explicitly and retain only evidence supported by the
  current project code, docs, and executed notebook.

## Assumptions

- The editable G4Utils checkout at `a456290` is the implementation baseline;
  released `v0.4.0` is context only.
- The report uses current dnachem-min source and output documentation as its
  primary evidence and keeps visualization and scientific interpretation in
  notebooks.
