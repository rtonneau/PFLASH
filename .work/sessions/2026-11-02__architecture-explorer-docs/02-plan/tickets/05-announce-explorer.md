# Ticket 05: announce-explorer

**Model:** fast
**Effort:** low

**Acceptance Criteria:**
- [ ] `docs/explorer/README.md` explains how to open, edit the JSON, rebuild and check.
- [ ] README.md and CLAUDE.md link to the explorer, with a rule to update architecture.json and rebuild when files are added.
- [ ] `node docs/explorer/check.js` passes; the full ctest suite is unaffected.

**Files to Touch:**
- `docs/explorer/README.md`
- `README.md`
- `CLAUDE.md`
- `docs/explorer/index.html`

**Verification Step:**

Run:
```bash
node docs/explorer/build.js && node docs/explorer/check.js; echo "exit=$?"; git diff --stat
```

Expected:
`exit=0`; the diff touches only the listed files and index.html is unchanged by the final build.

**Notes:**

Keep the CLAUDE.md addition to a couple of lines.
