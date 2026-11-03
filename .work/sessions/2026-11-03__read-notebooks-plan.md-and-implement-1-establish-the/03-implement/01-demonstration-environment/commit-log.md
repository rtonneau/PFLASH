# Ticket 01: demonstration-environment

**Status:** ✅ Done

## Local Test Result

Independently reran both documented sim.exe commands in a fresh review output directory: both exited 0. The documented disk verification found 12 nonempty files for IRT and 15 for mesoscopic spatial, each with 2 PureWater events. A fresh Jupyter MCP kernel imported all documented packages and G4Utils symbols successfully.

## Review Notes

Reviewed the README and seeded macros against the ticket. Implementation by gpt-6-sol; inline follow-up reran both simulations and kernel imports and added the project planning record. The pre-existing untracked notebooks/PLAN.md is left uncommitted.

## Blockers / Challenges

The existing G4Utils editable installation reports package metadata 0.3.0 while its checkout declares 0.4.0; documented in README. The mesoscopic run emits the documented WrongResolution warning.

## Commits

- e4ccc38 docs: establish notebook demonstration data (ticket 01)

## Time Spent

11m (ticket-start.js to ticket-complete.js)
