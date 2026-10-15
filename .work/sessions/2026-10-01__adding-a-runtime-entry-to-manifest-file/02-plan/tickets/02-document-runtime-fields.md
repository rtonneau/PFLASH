# Ticket 02: document-runtime-fields

**Model:** haiku

**Acceptance Criteria:**
- [ ] `docs/adr/0005-manifest-per-dump.md` has a new `## Addendum (2026-10-01): wall-clock runtimes` that says: the three keys and what each spans (`elapsedSinceStart_s`: `main()` start → dump; `elapsedSincePreviousDump_s`: previous dump or process start → dump; `runs[].wallTime_s`: master Begin→EndOfRunAction, run initialization excluded); wall clock only, because CPU time is misleading under MT; no total process time, because the `EndOfRun_` flush runs before the process ends; `schemaVersion` unchanged.
- [ ] `.claude/skills/sim-output/SKILL.md` `Manifest.json` bullet lists the two top-level keys after `timestamp` and `wallTime_s` in the `runs[]` key list, each with a few words on its span.
- [ ] `CLAUDE.md` `RunManifest.cc` bullet mentions `wallTime_s` in the run entry list and the two elapsed fields in `Write`, and says `MarkProcessStart()` is called from `sim.cc`.

**Files to Touch:**
- `docs/adr/0005-manifest-per-dump.md`
- `.claude/skills/sim-output/SKILL.md`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
grep -n "elapsedSinceStart_s\|elapsedSincePreviousDump_s\|wallTime_s\|MarkProcessStart" docs/adr/0005-manifest-per-dump.md .claude/skills/sim-output/SKILL.md CLAUDE.md
```

Expected:
At least one hit for each key in each of the three files (`MarkProcessStart` at least in `CLAUDE.md`).

**Notes:**

Use the key names exactly as ticket 01 shipped them. Check `src/scoring/RunManifest.cc` before writing. Keep the existing prose style: short, factual sentences. Commit: `docs: record manifest runtime fields`.
