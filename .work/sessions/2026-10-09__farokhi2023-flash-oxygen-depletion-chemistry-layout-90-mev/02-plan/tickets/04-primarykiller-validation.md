# Ticket 04: primarykiller-validation

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] A scratch macro run (documented in this ticket's Verification Step, not committed) confirms `PrimaryKiller` actually truncates a primary electron's track once it has deposited the configured `/primaryKiller/eLossMin` energy: with `/primaryKiller/verbose 1`, the run log contains `kill track at` lines, and the reported cumulative `E loss` at each kill is close to (at or just above) the configured threshold.
- [ ] The same run's `Manifest.json` energy-deposit figure is markedly lower than the full beam energy (confirming the primary was stopped early, not merely logged), for a beam energy well above the configured `eLossMin`.
- [ ] CLAUDE.md's Key Files section gets a new bullet for `src/scoring/PrimaryKiller.cc` (+ `header/scoring/PrimaryKiller.hh`), in the same style as the other scorer bullets (`ReactionCounter.cc`, `PhysicsInteractionCounter.cc`): what it does (kills the primary electron track once it has deposited a configured energy, or aborts the event above a second threshold, or kills below a minimum kinetic energy; also kills any track — not just the primary — that leaves a configurable bounding box, default 1 km, independent of `/chem/env/halfBox`), that it is **only** wired for the primary electron (`G4Track::GetTrackID()==1` and PDG 11 — a no-op for protons), its macro commands (`/primaryKiller/eLossMin`, `eLossMax`, `minKineticE`, `setSize`, `verbose`), and that `DetectorConstruction.cc` already applies non-trivial hardcoded defaults (`eLossMin` = 500 keV, `eLossMax` = 1 MeV) to every run today, not just when a macro overrides them.
- [ ] No source file changes — this ticket is validation + documentation only.

**Files to Touch:**
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
cd build
cat > macro/scratch_primarykiller_smoke.in <<'EOF'
/chem/select Farokhi2023
/chem/meso/enable false
/run/initialize
/scheduler/endTime 1 us
/gun/particle e-
/gun/energy 500 keV
/primaryKiller/eLossMin 5 keV
/primaryKiller/verbose 1
/run/beamOn 5
/run/dumpDataAndReset
EOF
./sim.exe scratch_primarykiller_smoke.in --dir ../.scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/ticket04-smoke > ticket04-smoke.log 2>&1
grep -c "kill track at" ticket04-smoke.log
```

Expected: exit 0; `grep -c` reports at least 1 (one per event, so at least 5 for 5 events) occurrences of `kill track at`; no `EEEE`/`FatalException`; `Manifest.json`'s energy-deposit figure is well under 500 keV x 5 events (confirming truncation, not full-range deposit).

**Notes:**

`PrimaryKiller` (`header/scoring/PrimaryKiller.hh`, `src/scoring/PrimaryKiller.cc`) is pre-existing code from the original Geant4-DNA "scavenger" example this project was derived from — it is registered unconditionally in `DetectorConstruction::ConstructSDandField()` (`src/geometry/DetectorConstruction.cc:136-146`) with hardcoded defaults, so it is **already live** for every run today, just invisible because no existing macro's beam deposits anywhere near its 500 keV default threshold. This ticket is about confirming it still behaves correctly with the current chemistry/scoring pipeline (it predates `ScoreSpecies`'s current form and was never covered by CLAUDE.md), not about writing new functionality. If the smoke run shows anomalous behavior (e.g., `fELoss` not resetting between events, or species counts that look broken because the primary was killed mid-step in a way that orphans secondaries), stop and report it rather than silently working around it — the electron macros in ticket 6 depend on this working correctly.
