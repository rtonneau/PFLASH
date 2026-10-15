# Ticket 04: o2-bulk-reactions

**Acceptance Criteria:**
- [ ] The `e_aq`, `H` and `Om` entries of both `BuildPureWaterBulkReactions` and `BuildBoscoloChemBulkReactions` gain `{"O2", 1.74e10 * M, {"O2m"}, 0}`, `{"O2", 2.1e10 * M, {kHO2}, 0}` and `{"O2", 3.7e9 * M, {"O3m"}, 0}` respectively
- [ ] With `/chem/env/scavenger O2 21 %` (PureWater): no inert warning; the dump's bulk section lists `e_aq + O2 -> O2m`, `H + O2 -> HO2°`, `Om + O2 -> O3m` with the UHDR rates
- [ ] Without a scavenger: the sorted dump differs from ticket 03 by exactly those three lines; physics files identical to the baseline
- [ ] Direction check at 1 µs (2 events): G(e_aq) and G(H) lower and G(O2m) higher with O2 21 % than with none; observed G(O2) recorded
- [ ] `--threads 2` with O2 21 % runs without `FatalException` and the dump lists the three O2(B) lines
- [ ] ADR 0004 Consequences and CLAUDE.md describe the absorbed-O2-products behavior and the three O2(B) reactions

**Files to Touch:**
- `src/chemistry/catalog/PureWaterReactions.cc`
- `header/chemistry/catalog/PureWaterReactions.hh`
- `src/chemistry/catalog/BoscoloChemReactions.cc`
- `header/chemistry/catalog/BoscoloChemReactions.hh`
- `docs/adr/0004-scavenger-reactions-per-chemistry.md`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
# inside the vcvars64 wrapper
cmake --build build --config RelWithDebInfo --target sim
cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure
cd build
./sim scav_smoke.in --dir smoke/t04_o2 > smoke/t04_o2.log 2>&1        # macro with /chem/env/scavenger O2 21 %
./sim scav_smoke.in --dir smoke/t04_zero > smoke/t04_zero.log 2>&1    # macro without a scavenger line
./sim scav_smoke.in --threads 2 --dir smoke/t04_mt > smoke/t04_mt.log 2>&1   # O2 21 %
diff <(sort smoke/t04_zero/reactions_dump.txt) <(sort smoke/t03/reactions_dump.txt)
```

Expected:
All tests Passed. Every log has `[RunAccumulatorMessenger] dumped and reset` and `The simulation took`, and no `FatalException`. The zero-run diff shows exactly the three `+ O2` bulk lines. `t04_o2/Species.Txt` vs `t04_zero/Species.Txt` at 1 µs: G(e_aq) and G(H) lower, G(O2m) higher.

**Notes:**

Failing check first (Step 1): before editing, run with `O2 21 %`. The log must warn `scavenger O2 is inert` and the bulk section must have no O2 line. Put a comment above the list: the "O2" partner is the dissolved-O2 scavenger set by `/chem/env/scavenger`, from the UHDR scavenger processes, and is inert at 0. Reword the header line about the tracked-O2 bimolecular reactions ("against *tracked* radiolytic O2"). BoscoloChem gets the identical code and comment edit but is never run in a smoke test (user decision): the build is its only check. If the 2-event direction check is ambiguous, rerun with `/run/beamOn 10`. Finally, delete `build/smoke/` and `build/macro/scav_smoke.in`. Exact ADR/CLAUDE.md sentences are in plan Task 4 Step 6.
