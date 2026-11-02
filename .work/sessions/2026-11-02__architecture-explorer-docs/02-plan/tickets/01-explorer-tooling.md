# Ticket 01: explorer-tooling

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `docs/explorer/architecture.json` exists with a documented minimal schema (lanes, nodes with summary/files/symbols, otherFiles per cluster, flow, tasks).
- [ ] `docs/explorer/build.js` embeds the JSON into `docs/explorer/index.html` (from a template) deterministically.
- [ ] `docs/explorer/check.js` exits non-zero on a missing linked path, a src/ or header/ file not listed anywhere, or an index.html differing from a fresh build; it exits 0 otherwise.
- [ ] A ctest test `ExplorerCheck` runs `node docs/explorer/check.js`.

**Files to Touch:**
- `docs/explorer/architecture.json`
- `docs/explorer/build.js`
- `docs/explorer/check.js`
- `docs/explorer/template.html`
- `docs/explorer/index.html`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
node docs/explorer/build.js && node docs/explorer/check.js; echo "exit=$?"; cd build-ninja && ctest -R ExplorerCheck
```

Expected:
The checker prints a pass line (initially the minimal JSON must list all files in otherFiles) with `exit=0`, and ctest reports ExplorerCheck passed. Temporarily removing one file from the JSON makes the checker fail naming that file.

**Notes:**

Use only the Node standard library. Prepend vcpkg debug\bin to PATH before ctest if it hangs (see project memory). Keep the check script path-independent (resolve from `__dirname`). Escape `</script>` in the embedded JSON.
