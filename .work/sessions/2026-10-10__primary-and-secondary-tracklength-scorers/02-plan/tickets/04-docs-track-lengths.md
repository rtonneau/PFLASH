# Ticket 04: docs-track-lengths

**Model:** haiku
**Model (Jev):** haiku-5.5 (confidence 0.87)
**Effort:** low

**Acceptance Criteria:**
- [ ] `CLAUDE.md` Key Files lists `src/scoring/TrackLengthTable.cc` and mentions the `TrackingAction` / `PrimaryKiller` flag roles; the output paragraph mentions `TrackLengths.*` and the manifest's `trackLengths`.
- [ ] `.claude/skills/sim-output/SKILL.md` describes `TrackLengths.Txt/.csv` (columns, units, end reasons, aborted events skipped, per-Dump accumulation).
- [ ] `docs/output/README.md` (and any layout page under `docs/output/layouts/` that lists files) lists the two files.
- [ ] `docs/literature-comparison/farokhi2023/track_length_let.md` notes that the instrumented track length/LET now exists in `TrackLengths.*`.

**Files to Touch:**
- `CLAUDE.md`
- `.claude/skills/sim-output/SKILL.md`
- `docs/output/README.md`, `docs/output/layouts/*`
- `docs/literature-comparison/farokhi2023/track_length_let.md`

**Verification Step:**

Run:
```bash
grep -rn "TrackLengths" CLAUDE.md .claude/skills/sim-output/SKILL.md docs/output docs/literature-comparison/farokhi2023/track_length_let.md
```

Expected:
At least one match in each of the four locations.

**Notes:**

Use the glossary terms from `CONTEXT.md`: **Primary track**, **Secondary electron**, **First-generation secondary electron**. Docs only, no code changes.
