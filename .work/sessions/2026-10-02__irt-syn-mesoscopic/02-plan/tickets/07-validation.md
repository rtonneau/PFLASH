# Ticket 07: validation

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `analysis/compare_reference.py <reference_dir> <new_dump_dir> [--tol 0.10] [--plot out.png]` works as follows:
  - It reads `Species_nt_species.csv` and `Manifest.json` from both directories. For a new-model dump (its `Manifest.json` has `chemistryModel`), it also reads `SpeciesMeso.csv` and uses it for times later than the hand-over: `Species.*` stops at the hand-over (ticket 05), and the mesoscopic output holds 1 µs.
  - It computes G(t) per 100 eV from each manifest's energy deposit.
  - It prints G at 1 µs (nearest common time ≤ 1 µs) for e_aq, °OH, H2O2 and H2 with the relative difference, plus the wall-time ratio.
  - It optionally plots G(t) overlays.
  - It exits 1 when any of the four species differs by more than `--tol`, and 0 otherwise.
  - It runs in conda env `GEANT4_py311`.
- [ ] *Amended 2026-10-02 after the first attempt was BLOCKED (user decision: adopt IRT_syn as the model of record and document the SBS difference).* The new model is **compared with** `validation/reference/sbs_water` and `sbs_o2` using the same `macro/validate_{water,o2}.in` (end time 1 µs). The differences are **reported, not gated**: run `compare_reference.py` with its default `--tol`, record the numbers, and note its exit 1 as expected. The first attempt measured water at e_aq +27%, °OH +22%, H2O2 +51%, H2 −2.5%. The diagnostics, all water with N = 40, are listed below.

  | Diagnostic | e_aq | °OH | H2O2 | H2 |
  |---|---|---|---|---|
  | D1: IRT_syn only, hand-over 2 µs > end time | +16% | +7% | +43% | +2.6% |
  | D2: 400 µm box, 6.1 nm cells | +31% | +23% | +53% | −4.6% |
  | D3: IRT_syn only, all reactions type 0 | +19% | +6% | +46% | +1.3% |

  Conclusion: IRT_syn and SBS disagree by themselves, with an identical reaction table. The mesoscopic stage adds about 10 points on e_aq and °OH. Cell size and diffusion class are ruled out. These numbers go into the report, with D1 re-run once for the O2 pair (IRT_syn only vs SBS) so both pairs show the split between the particle and mesoscopic stages.
- [ ] *Update 2026-10-02 (second BLOCKED attempt, then inline diagnosis):* two defects were found and fixed in the project code. Both runs must use a build that includes them:
  - **`e0a5b88`, O2 hang.** `Voxelizing` left radiolytic O2 tracks alive at the hand-over. IRT_syn then paired them with killed tracks and `G4DNAIndependentReactionTimeStepper::FindReaction` looped forever. These tracks are now merged into the bulk pool.
  - **`baf4b29`, stale `SpeciesMeso` late records.** `G4DNAEventScheduler::RecordTime` only runs after reaction steps, so late record times kept old counts. `SpeciesMeso` is now recorded by our mesh action after every step.

  Post-fix comparison against SBS (dumps in `.scratch/tests/2026-10-02__irt-syn-mesoscopic/fix-validate_water`, `fix-validate_o2`; N = 40 and N = 1500; MT ×10). Reuse these dumps and don't re-run them:

  | Pair | e_aq | °OH | H2O2 | H2 |
  |---|---|---|---|---|
  | water | +25.0% | +21.2% | +51.1% | −1.9% |
  | O2 | +9.0% | −1.7% | +36.9% | +30.9% |

  The O2 run completes (1500 events, 1184 s). The **Serial vs `--threads 4` check must be re-run** on the post-fix build, because the recording changed: same batch-means method and gate. The report states the remaining open point: in water, the mesoscopic stage adds about +9 points on e_aq and +14 on °OH over IRT_syn alone, and the cause is not established (the cell-size test was inconclusive).
- [ ] **The O2 pair must complete** (done after `e0a5b88`, see above). The first `validate_o2.in` run (N = 1500, `--threads 10`) hung on one worker after 1495 events (more than 50 min, no progress) and was killed. Re-run it with `/dnaLogger/verbose Info` (per-event "Chemistry ends ... wall time" lines) and the event ID logged at the start of each event's chemistry. If it hangs again, identify the event and thread, reproduce that single event (seed and event number) on its own, and report BLOCKED with the stage it hangs in (particle or mesoscopic, and for mesoscopic the mesh level and time) instead of working around it. If it completes, report the wall time and note that the hang did not reproduce.
- [ ] Serial vs `--threads 4` on `validate_water.in`: `compare_reference.py` with one as the reference and `--tol` set to 3× the measured relative error passes.
- [ ] Hand-over sensitivity (cell 15.26 nm on the 1 mm box): `validate_water.in` with `/chem/meso/handOverTime 20 ns` is compared with the 5 ns run. The G(1 µs) relative difference for the four species goes in the report. This is a measurement, not a pass/fail gate.
- [ ] One long run per pair (`/scheduler/endTime 1 s`, `SpeciesMeso.*` included) feeds `docs/irt-syn-mesoscopic-validation.md`. The report covers:
  - the ≤1 µs comparison numbers and plots, and the 5 ns vs 20 ns hand-over result;
  - the Serial vs MT result;
  - long-time G(t) from 5 ns to 1 s compared by eye with published values or the UHDR example (sources cited);
  - wall times;
  - known limits: no reaction counts after the hand-over, per-event O2 pool, the stage-2 items.
- [ ] The report gives the SBS comparison and the D1–D3 diagnostics their own section, stating that IRT_syn is the model of record and that the SBS reference is a regression baseline, not ground truth. It also names the open question: which model handles e_aq + H2O2 and related reactions correctly, decidable with an analytic pair test as a possible follow-up.
- [ ] ADR 0006 status changes to `accepted`, with the validation numbers added and the decision "IRT_syn is the model of record; the SBS difference is documented, not gated".

**Files to Touch:**
- `analysis/compare_reference.py`
- `docs/irt-syn-mesoscopic-validation.md` (+ plot PNGs under `docs/`)
- `docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md`
- `src/chemistry/TimeStepAction.cc`, for an Info-level line with the event ID at the start of each event's chemistry (logging only, needed to locate the O2 hang)

**Verification Step:**

Run (long runs in the background; PowerShell for the app):
```bash
cd build; ./sim validate_water.in --threads 4 --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-water > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-water.log 2>&1
./sim validate_o2.in --threads 4 --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-o2 > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-o2.log 2>&1
conda run -n GEANT4_py311 python ../analysis/compare_reference.py ../validation/reference/sbs_water ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-water --plot ../docs/irt-syn-meso-water.png
conda run -n GEANT4_py311 python ../analysis/compare_reference.py ../validation/reference/sbs_o2 ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-o2 --plot ../docs/irt-syn-meso-o2.png
```

Expected:
Both runs exit 0 with the success markers present. The two SBS comparisons print their numbers (exit 1 expected, documented). The Serial vs MT comparison exits 0. The O2 run completes, or the ticket is BLOCKED with the hang located.

**Notes:**

The SBS comparison is documentation, not a gate (amended). The Serial vs MT comparison is still a gate: if it fails, the ticket is BLOCKED with the numbers. Don't loosen its tolerance. The water dumps from the first attempt (`.scratch/tests/2026-10-02__irt-syn-mesoscopic/07-water`, `07-water-ho20`, `d1-nomeso`, `d2-box400`, `d3-type0`) and `analysis/compare_reference.py` (uncommitted, working) can be reused. Before claiming done, read every `EEEE`/`WWWW` in the logs (`.claude/geant4-instructions.md` §4). Check that pandas and matplotlib are in `GEANT4_py311` before writing the script, and report it if they aren't. Never install silently.
