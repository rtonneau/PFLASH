# Ticket 07: solvation-subtype-comparison

**Model:** sonnet

**Acceptance Criteria:**
- [ ] If ticket 6's `validation.md` shows e_aq(0%) and O2m(21%) both within ±10%, write `solvation.md` with "not needed: <values>" and stop; no code change.
- [ ] Otherwise, for each `/process/dna/e-SolvationSubType` value Geant4 11.4.1 accepts (list them from the Geant4 source, e.g. `G4DNAElectronSolvation`/`G4EmDNAPhysics` messenger, not from memory), run 0% and 21% (100 keV, 2 events, both SBS switches on, via a scratch copy of the scan macro), run the comparison tool, and tabulate e_aq(0%), O2m(21%), HO2(21%), H(0%) at 1 µs plus the shape checks per subtype in `solvation.md`.
- [ ] Pick the subtype with the smallest maximum |deviation| over the four ±10% values whose shape checks all pass. Set it in `macro/beam_boscolo.in` (replacing `Ritchie1994` only if it is better), and record the choice and the remaining deviations in ADR 0006 under a "Validation" heading.
- [ ] If no subtype reaches ±10% on all four, say so plainly in `solvation.md` and the ADR (that is a valid outcome, not BLOCKED).

**Files to Touch:**
- `macro/beam_boscolo.in`
- `docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md`

**Verification Step:**

Run:
```bash
cat .scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/solvation.md
```

Expected:
Either "not needed" with values, or a per-subtype table with the chosen subtype and the remaining deviations.

**Notes:**

The shared physics code is out of scope: only the macro line changes. Each 2-level run costs about 35–60 min; run at most 3 sims in parallel.
