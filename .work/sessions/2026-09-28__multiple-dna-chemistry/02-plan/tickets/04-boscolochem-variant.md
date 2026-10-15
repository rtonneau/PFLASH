# Ticket 04: boscolochem-variant

**Acceptance Criteria:**
- [ ] `header/BoscoloChemReactions.hh` and `src/BoscoloChemReactions.cc` exist, are self-contained like `PureWaterReactions`, expose `BuildBoscoloChemReactions` and `BuildBoscoloChemAcidBase`, and start as a verbatim copy of the `PureWater` content with a header comment marking them work in progress.
- [ ] `BoscoloChem` is registered; `/chem/list` shows `PureWater (default) BoscoloChem`.
- [ ] `/chem/select boscolochem` runs; its sorted reaction dump equals the `PureWater` baseline (the copy is identical).
- [ ] `/chem/select PureWater` followed by `/chem/select BoscoloChem` is fatal and the message contains `already set to 'PureWater'`.
- [ ] `macro/beam_boscolo.in` exists and runs with `/chem/select BoscoloChem`; `macro/beam.in` is unchanged.
- [ ] Empty-list check done: with `BuildBoscoloChemAcidBase()` temporarily returning `{}`, a smoke run exits 0 with no `EEEE`/`FatalException` and no acid-base lines in the dump. The temporary edit is reverted before the commit. The result is written in the commit message.

**Files to Touch:**
- `header/BoscoloChemReactions.hh` (create)
- `src/BoscoloChemReactions.cc` (create)
- `src/BuiltInChemistries.cc`
- `macro/beam_boscolo.in` (create)

**Verification Step:**

Run (scratch macros in `build/macro/`: `gps_boscolo.in` is `gps_dump.in` with `/chem/select boscolochem` before `/run/initialize`; `gps_conflict.in` has `/chem/select PureWater` then `/chem/select BoscoloChem`; `gps_list.in` from ticket 03):
```bash
cd build
S=C:/DEV/GEANT4/SIM/dnachem-min/.scratch/tests/2026-09-28__multiple-dna-chemistry
./sim.exe gps_boscolo.in --dir $S/after04_boscolo > after04_boscolo.log 2>&1
diff <(sort $S/baseline/ReactionTable.txt) <(sort $S/after04_boscolo/ReactionTable.txt) && echo BOSCOLO_SAME
grep -c "chemistry = BoscoloChem" after04_boscolo.log
./sim.exe gps_conflict.in > conflict.log 2>&1; grep -c "already set to 'PureWater'" conflict.log
./sim.exe gps_list.in > list.log 2>&1; grep "PureWater (default) BoscoloChem" list.log
./sim.exe beam_boscolo.in --dir $S/after04_macro > after04_macro.log 2>&1; grep -c "dumped and reset\|The simulation took" after04_macro.log
git diff --stat -- macro/beam.in
```

Expected:
`BOSCOLO_SAME`, `1`, `1`, the list line, `2`, and no output from `git diff --stat` for `macro/beam.in`. Then the empty-list check in the Notes. Run simulations with `run_in_background`.

**Notes:**

Create the two files by copying and renaming, then hand-edit the header comment:
```bash
cp header/PureWaterReactions.hh header/BoscoloChemReactions.hh
cp src/PureWaterReactions.cc src/BoscoloChemReactions.cc
sed -i 's/PureWaterReactions/BoscoloChemReactions/g; s/BuildPureWaterAcidBase/BuildBoscoloChemAcidBase/g' header/BoscoloChemReactions.hh src/BoscoloChemReactions.cc
```
After the `sed`, `BuildPureWaterReactions` becomes `BuildBoscoloChemReactions` (it contains `PureWaterReactions`). Check with `grep -n "PureWater" header/BoscoloChemReactions.hh src/BoscoloChemReactions.cc`: the only remaining hits should be in comments that you rewrite in the next step. Replace the top-of-file comment in both files with a work-in-progress notice, for example:
```cpp
/// WORK IN PROGRESS: the reaction rates, products and acid-base list below are a
/// verbatim copy of the PureWater Chemistry. Edit them here to reproduce the
/// BoscoloChem network. If this Chemistry omits the acid-base buffer (return an
/// empty list from BuildBoscoloChemAcidBase), say so here: the buffer is then
/// absent by design, see docs/adr/0002-named-chemistries.md.
```
Do not cite a paper or reference values you have not been given.

`src/BuiltInChemistries.cc`: add `#include "BoscoloChemReactions.hh"` and, after the `PureWater` entry:
```cpp
  add({"BoscoloChem", &BoscoloChemReactions::BuildBoscoloChemReactions,
       &BoscoloChemReactions::BuildBoscoloChemAcidBase});
```

`macro/beam_boscolo.in`:
```
/run/verbose 0
/tracking/verbose 0
/dnaLogger/verbose Info

/process/dna/e-SolvationSubType Ritchie1994
/process/chem/TimeStepModel SBS

# Select the BoscoloChem reaction network. PreInit only: issue it before
# /run/initialize. Leave the line out to get the default, PureWater.
# /chem/list prints the available names.
/chem/select BoscoloChem

/run/initialize

/gun/particle e-
/gun/energy 10 keV

/run/beamOn 2
```
The build copies `macro/` into the run build dir, so rebuild `sim` before running it.

Empty-list check (do it after `git add` of the new files, so `git restore` can undo the temporary edit):
1. `git add header/BoscoloChemReactions.hh src/BoscoloChemReactions.cc src/BuiltInChemistries.cc macro/beam_boscolo.in`
2. In `src/BoscoloChemReactions.cc`, change the body of `BuildBoscoloChemAcidBase()` to `return {};` (leave the rest; unused locals may warn).
3. Rebuild `sim`, run `gps_boscolo.in --dir $S/empty_acidbase`, then check the exit code, `grep -c "EEEE\|FatalException"` (expect 0), both success markers, and that `empty_acidbase/ReactionTable.txt` has no acid-base lines that were in the baseline (compare line counts: it must be shorter by the acid-base reactions).
4. `git restore src/BoscoloChemReactions.cc` to bring back the staged copy, rebuild, and re-run the `BOSCOLO_SAME` check.
5. If step 3 crashed or printed a warning about the scavenger material, do not commit yet: read the trace, gate the `G4DNAScavengerMaterial` installation in `DnaChemistryList::ConstructProcess` on a non-empty list only if the trace shows it is the cause, and add a line to `docs/adr/0002-named-chemistries.md`.

Commit message: `feat: add BoscoloChem chemistry (copy of PureWater) and example macro`; state the empty-list result in the body.
