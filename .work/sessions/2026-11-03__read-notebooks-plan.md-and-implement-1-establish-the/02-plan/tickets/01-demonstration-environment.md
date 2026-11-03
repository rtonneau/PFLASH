# Ticket 01: demonstration-environment

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.39)
**Effort:** medium

**Acceptance Criteria:**
- [ ] Inspect output documentation and G4Utils interfaces; verify required imports in the actual Jupyter kernel.
- [ ] Select or generate small seeded demonstration data and identify mesoscopic and spatial runs, verifying required outputs on disk.
- [ ] Document macros, seeds, versions, events, reproduction commands, paths, dependencies and limitations; exclude generated datasets from Git.

**Files to Touch:**
- `notebooks/README.md`
- `notebooks/` demonstration macros or configuration as needed
- `.claude/plans/` project plan

**Verification Step:**

Run:
```powershell
# Execute the documented simulation commands and kernel import checks, then inspect each required output on disk.
```

Expected: documented runs complete, required files exist, and kernel imports succeed or missing prerequisites are explicitly identified.

**Notes:**

Use Jupyter MCP for notebook operations. Treat small runs as demonstrations, not converged results.
