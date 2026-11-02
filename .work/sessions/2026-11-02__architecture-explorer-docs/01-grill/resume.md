# Session: architecture-explorer-docs

**Date:** 2026-11-02T14:25:53.579Z
**Status:** Grill phase complete

## Problem Statement

Newcomers (collaborators and non-coders such as supervisors and students) have no friendly way to discover how dnachem-min is organised. The knowledge is spread over CLAUDE.md, CONTEXT.md, 8 ADRs, docs/recipes, docs/output and static SVGs. We want a browsable graphical "architecture explorer" documentation page.

## Context & Constraints

- **Current behavior:** Architecture knowledge lives in markdown (CLAUDE.md "Key Files", CONTEXT.md glossary, docs/adr, docs/recipes, docs/output) and a few static SVGs in docs/visual. Nothing is interactive or cross-linked.
- **Pain point:** Hard to find where things live, how a run flows (SBS / IRT_syn / IRT_syn + mesoscopic) and what to edit for common tasks; non-coders cannot read the code-centric docs.
- **Dependencies:** Must stay in sync with src/ and header/ (layout per ADR 0003); reuse existing docs/recipes and docs/output rather than duplicating them; Node is the only added tooling (embed + check scripts); ctest integration.
- **Tech stack:** Static HTML + vanilla JS + JS-rendered SVG, one canonical JSON, Node scripts, ctest.

## Success Metrics

- docs/explorer/index.html opens by double-click (file://), offline, with no server or build step for readers.
- Three views work: module map (6 clusters, expandable to key files/classes, ~40-60 nodes, plus test/ and macro/ lanes), run/data flow (with the 3 chemistry-mode branches and output files per step), task entry points (links to existing recipes/ADRs plus non-coder tasks such as run a simulation and read the output).
- A ctest-registered Node checker fails on: a missing linked path, any src/ or header/ file not accounted for in the JSON (key node or the cluster's "other files" list), or a committed index.html that differs from a fresh build of architecture.json.
- Every node has a plain-language summary always visible and an expandable "where in the code" section with relative file links, a copyable path and symbol name.

## Architecture & Approach

- docs/explorer/architecture.json is the single source of truth (nodes, edges, lanes, flow steps, task entries, summaries, file lists).
- A Node script bakes the JSON into docs/explorer/index.html (fetch is blocked on file://); index.html is a pure renderer that draws diagrams with JS from the JSON graph in fixed lanes. The built index.html is committed.
- Two-layer content per node (plain language first, code detail on demand). Links are relative file links.
- A Node check script, run by hand and as a ctest test, enforces link validity, file coverage and freshness.
- Announced via links from README.md and CLAUDE.md. Keeping the JSON as the data source leaves a future generated site (mkdocs/Doxygen) possible.
- Content is drafted by Claude from CLAUDE.md, CONTEXT.md, ADRs and source; the user reviews the plain-language summaries for physics correctness.

## Assumptions & Trade-offs

- Not doing: Glossary/ADR browser view (not in v1), auto-generation from source, a server or app, voxelization or any simulation change, a mkdocs/Doxygen site (future option only).
- Trade-off: curated prose can go stale, mitigated by the coverage and freshness checks; the coverage check only guarantees files are listed, not that summaries stay correct.
- Assumes Node is available on contributors' machines for rebuilding (readers need only a browser).

## Open Questions

- Whether to record an ADR for "JSON as single source of truth, HTML as renderer" (to decide at plan time).
- How the ctest check is wired in CMake and under which build dir (build-ninja Debug, per project testing notes).

## Notes

Decisions confirmed by the user in the grill: audience mixed incl. non-coders; static HTML now with option 2 in mind; curated JSON + path checker; views = module map, run/data flow, task entry points; two-layer depth; JSON + embed script; JS-rendered diagrams; relative file links; clusters to key files with an other-files list; Node script + ctest; commit index.html and verify freshness; location docs/explorer/ with README/CLAUDE links; run flow shows the 3 modes plus outputs; task paths link existing recipes; Claude drafts and the user reviews.
