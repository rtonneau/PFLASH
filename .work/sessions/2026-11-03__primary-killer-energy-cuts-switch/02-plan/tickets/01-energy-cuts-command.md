# Ticket 01: energy-cuts-command

**Model:** medium
**Effort:** medium

**Acceptance Criteria:**
- [ ] `/primaryKiller/energyCuts <bool>` (default true) exists; with false, `ProcessHits` returns before the abort/kill branches; box cut unchanged.
- [ ] Inline `SetEnergyCutsEnabled(G4bool)` in the header; command deleted in the destructor.
- [ ] `/primaryKiller/minKineticE` sets `fKineticE_Min`; `fpMinKineticE` deleted in the destructor; header comments updated.
- [ ] Smoke run (10 keV e-, beamOn 2, endTime 1 ms, energyCuts false, verbose 1): no `kill track at`, only stopped/escaped in TrackLengths.csv.
- [ ] 1 MeV run: primary length with cuts off > with cuts on.

**Files to Touch:**
- `src/scoring/PrimaryKiller.cc`
- `header/scoring/PrimaryKiller.hh`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo && (cd build && ./sim <scratch smoke macro with /primaryKiller/energyCuts false>)
```

Expected:
Build succeeds; no "kill track at" in the log; TrackLengths.csv end reasons only stopped/escaped.

**Notes:**

Scratch macros go in `.scratch/tests/2026-11-03__primary-killer-energy-cuts-switch/`. `/primaryKiller/*` commands exist only after `/run/initialize`.
