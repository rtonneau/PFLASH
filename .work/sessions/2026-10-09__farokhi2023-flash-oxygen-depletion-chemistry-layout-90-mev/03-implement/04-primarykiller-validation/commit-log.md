# Ticket 04: primarykiller-validation

**Status:** ✅ Done

## Local Test Result

Subagent ran the Verification Step: build (already current), then `./sim.exe scratch_primarykiller_smoke.in --dir .../ticket04-smoke` (Farokhi2023, meso off, 1 us end time, 500 keV e-, `/primaryKiller/eLossMin 5 keV`, verbose 1, 5 events). Exit 0. `grep -c "kill track at"` → 5 (one per event); each kill line reports cumulative E loss of 5.00-5.03 keV, right at/just above the 5 keV threshold. No `FatalException`/`EEEE`, no spurious `aborts event` lines (eLossMax stayed at its 1 MeV default). `Manifest.json`'s `totalEnergyDeposit_eV` = 25102.6 for 5 events of a 500 keV beam (full-range would be ~2.5 MeV) — about 1% of full-beam energy, confirming early truncation. `fELoss` correctly resets each event (5 independent ~5 keV kills, not a growing cumulative total); nothing suggested orphaned secondaries or broken species/reaction counts.

## Review Notes

Reviewed `git diff -- CLAUDE.md`: exactly one new Key Files bullet for `src/scoring/PrimaryKiller.cc` (+ header), placed after `PhysicsInteractionCounter.cc` in file order, same style as neighboring scorer bullets. Covers: pre-existing code from the original Geant4-DNA "scavenger" example, unconditionally registered in `DetectorConstruction::ConstructSDandField()` with the hardcoded 500 keV/1 MeV defaults already live today; its behavior (kill on eLossMin, abort on eLossMax, kill below minKineticE, bounding-box kill via setSize independent of `/chem/env/halfBox`); that it's wired only to the primary electron (TrackID==1, PDG 11); and the verbose output. `git status` confirms `CLAUDE.md` is the only tracked file this ticket touched (the scratch macro and run output live under `build/` and `.scratch/tests/...`, neither tracked). All 4 acceptance criteria hold; no changes made after review.

## Blockers / Challenges

None. Verification found no anomalous behavior.

## Commits

- 4604a79 docs: validate and document PrimaryKiller (ticket 04)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
