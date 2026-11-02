# Ticket 02: module-map-content

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `architecture.json` has the 6 clusters plus `test/` and `macro/` lanes, with about 40-60 key-file nodes.
- [ ] Each node has a plain-language summary and a code section (files, symbols), sourced from CLAUDE.md, CONTEXT.md and the ADRs.
- [ ] Every src/ and header/ file is a key node or in a cluster's otherFiles list.
- [ ] Edges between nodes reflect the real dependencies described in CLAUDE.md.

**Files to Touch:**
- `docs/explorer/architecture.json`
- `docs/explorer/index.html`

**Verification Step:**

Run:
```bash
node docs/explorer/build.js && node docs/explorer/check.js; echo "exit=$?"
```

Expected:
`exit=0`; the summary reports node count between 40 and 60 and zero unlisted files.

**Notes:**

Do not invent physics: copy or condense wording from CLAUDE.md and CONTEXT.md. Use CONTEXT.md terms (Chemistry, Particle-based stage, Mesoscopic stage, Hand-over time). Flag uncertain summaries with a `"review": true` field for the user.
