# Ticket 04: flow-and-tasks

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] Run flow view shows the main path (sim.cc, physics, pre-chemical, particle-based stage, hand-over, mesoscopic stage, dump and Manifest.json) with branches for SBS, IRT_syn and IRT_syn + mesoscopic, and the output files per step linked to docs/output/README.md.
- [ ] Tasks view lists entry points linking the existing docs/recipes/*.md and ADRs, each with a one-line summary and files touched, plus "run a simulation" and "read the output" for non-coders (sim-output skill docs, example macros).
- [ ] Every flow step and task links to existing paths; the checker validates them.

**Files to Touch:**
- `docs/explorer/architecture.json`
- `docs/explorer/template.html`
- `docs/explorer/index.html`

**Verification Step:**

Run:
```bash
node docs/explorer/build.js && node docs/explorer/check.js; echo "exit=$?"
```

Expected:
`exit=0`; the three tabs render with content; every recipe file in docs/recipes appears in the Tasks view.

**Notes:**

Mode behaviour comes from ADR 0006 and ADR 0008 and CLAUDE.md; do not paraphrase beyond them. Output file facts from docs/output/README.md.
