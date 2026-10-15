# Ticket 05: rewrite-scope-docs

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.68)
**Effort:** medium

**Acceptance Criteria:**
- [ ] CLAUDE.md title and Purpose and Scope say PFLASH: PFAS + FLASH, aim is the dose-rate effect; the old rule against dose-rate/pulse structure is reworded as "not part of housecleaning; next development"; an origin note says it derives from dnachem-min; `sim` becomes `pflash`.
- [ ] README.md and CONTEXT.md carry the same PFLASH purpose and origin note.
- [ ] CLAUDE.md no longer mentions removed items (`.kb-notes`, `docs/cloud-work`, old plans, skill name `sim-output`) and lists `macro/farokhi2023/`.
- [ ] Behavior descriptions (chemistry modes, outputs, macros) are unchanged.

**Files to Touch:**
- `CLAUDE.md`, `README.md`, `CONTEXT.md`, `.work/GLOSSARY.md` (only if it names the old project)

**Verification Step:**

Run:
```bash
grep -n "dnachem-min\|\bsim\b\|kb-notes\|UHDR is a use case" CLAUDE.md README.md CONTEXT.md
```

Expected:
Only the origin note and legitimate UHDR references remain.

**Notes:**

Do not invent dose-rate design; state only the aim. Keep the existing technical content of CLAUDE.md (Key Files, macros). Follow the global convention that CLAUDE.md is the project rule file. Run `unslop` on new prose.
