# Implementation Plan

**Session:** architecture-explorer-docs
**Date:** 2026-11-02T14:36:01.422Z
**Estimated effort:** 2 days

## Strategy

Build the tooling first (JSON schema, embed script, checker, ctest), then the module-map content and renderer, then the run-flow and task views, then announce it. Each ticket is one commit and leaves the checker green.

## Tickets Overview

- **Ticket 1:** Schema, `build.js` embed script, `check.js` checker, minimal JSON and ctest wiring.
- **Ticket 2:** Full module-map content in `architecture.json` (6 clusters, key nodes, other-files lists covering every src/ and header/ file, test/ and macro/ lanes).
- **Ticket 3:** `index.html` template and JS renderer for the module map with two-layer node details.
- **Ticket 4:** Run/data flow (3 mode branches, outputs) and task entry points: JSON content and views.
- **Ticket 5:** Links from README.md and CLAUDE.md, a short docs/explorer/README.md, final rebuild.

## Sequencing Rationale

The checker comes first so every later ticket is verified by it. Content (2) precedes the renderer (3) so the renderer is tested on real data. Flow and tasks (4) reuse the renderer. Announcement (5) last.

## Risks & Mitigation

- **Risk:** Plain-language summaries are wrong physics → **Mitigation:** text sourced only from CLAUDE.md, CONTEXT.md, ADRs; the user reviews before merge.
- **Risk:** Content goes stale → **Mitigation:** checker enforces link validity, file coverage and freshness in ctest.
- **Risk:** ctest in this project has Windows-specific pitfalls → **Mitigation:** register the test like the existing ones and run from build-ninja per .claude/geant4-instructions.md.

## Assumptions

- Node is available to contributors who rebuild; readers need only a browser.
- Vanilla JS and inline SVG, no external libraries or network access.
- Scripts live in docs/explorer/ and use only the Node standard library.
