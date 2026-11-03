# Ticket 02: builder-and-docs

**Model:** fast
**Effort:** low

**Acceptance Criteria:**
- [ ] Builder Off writes `/primaryKiller/energyCuts false`; `KILLER_OFF` constant, export and comment removed.
- [ ] `killerMode` tech text and README rule describe the switch; minKineticE is "not offered", not "no effect".
- [ ] Off test updated; `node docs/explorer/macro-builder.test.js` passes.
- [ ] CLAUDE.md PrimaryKiller paragraph and architecture.json (summary + command list) updated; explorer rebuilt; ctest ExplorerCheck and MacroBuilderTest pass.

**Files to Touch:**
- `docs/explorer/macro-builder.js`
- `docs/explorer/macro-builder.test.js`
- `docs/explorer/README.md`
- `docs/explorer/architecture.json`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
node docs/explorer/build.js && node docs/explorer/macro-builder.test.js && ctest --test-dir build-ninja -R "ExplorerCheck|MacroBuilder"
```

Expected:
All tests pass.

**Notes:**

The 'generated commands exist' test checks `"energyCuts"` appears in PrimaryKiller.cc (lands in ticket 1).
