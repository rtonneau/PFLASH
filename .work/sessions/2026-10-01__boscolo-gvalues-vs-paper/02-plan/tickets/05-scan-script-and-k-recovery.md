# Ticket 05: scan-script-and-k-recovery

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `macro/test_run.ps1` gains `-RateAware` (switch) and `-MaxTimeStep <string>` (e.g. `'1 ns'`, default empty) parameters. When set, the generated macro contains `/chem/sbs/rateAwareReactions true` and `/chem/sbs/maxTimeStep <value>` before `/run/initialize`. It also gains `-ReactionBins <string>` (default empty), which emits `/chem/reaction/timeBinsList <value>`. Today's default behavior is unchanged. The `.EXAMPLE` block shows the new parameters.
- [ ] k-recovery check: run BoscoloChem, 0% pO2, 100 keV, 2 events, with `-ReactionBins '1 3 10 30 100 300 1000 3000 10000 30000 100000 300000 1000000 ps'`, `-RateAware` and `-MaxTimeStep '1 ns'`. The baseline is the existing `runs/o2_0`. A short `k-recovery.md` in the session scratch dir reports, for every bin from 10 ns to 1 µs, the ratio `(OH + H2 -> H) / (OH + H -> no products)` before and after. Pass: the after-ratio is < 0.05 in every such bin (before: ~0.6–0.9), and `H + OH- -> e_aq` and `OH + H2O2 -> HO2` drop by ≥ 5× in total over 10 ns–1 µs.
- [ ] If the pass condition fails, the ticket reports BLOCKED with the numbers; don't tune anything.

**Files to Touch:**
- `macro/test_run.ps1`

**Verification Step:**

Run:
```powershell
cd build; ../macro/test_run.ps1 -Levels 0 -Energy '100 keV' -RateAware -MaxTimeStep '1 ns' -ReactionBins '1 3 10 30 100 300 1000 3000 10000 30000 100000 300000 1000000 ps' -Results ../.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t05
```

Expected:
`done in N s`; then `k-recovery.md` shows every after-ratio < 0.05.

**Notes:**

`test_run.ps1` runs from the dir holding `sim.exe` (`build/`) and writes its scan macro into `macro/` there. Rebuild `sim` first, since the build copies `macro/`. Run it in the background (expect 15–40 min) and read the log. Parse `Reactions.Txt` (bin edges are bare lines in ns, followed by `label    count = N` lines). The before data lives in `.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/runs/o2_0`.
