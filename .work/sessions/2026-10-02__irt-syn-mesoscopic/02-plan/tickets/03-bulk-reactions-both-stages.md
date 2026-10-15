# Ticket 03: bulk-reactions-both-stages

**Model:** opus

**Acceptance Criteria:**
- [ ] `DnaChemistryList::ConstructReactionTable` also adds every entry of the selected Chemistry's `buildBulkReactions()` to the reaction table:
  - one `G4DNAMolecularReactionData(rate, molecule, partner)` per bulk reaction, rate and products as listed;
  - `SetReactionType(reactionType)` when it isn't 0;
  - the same configurations `RegisterBulkReactionProcesses` resolves.
  
  `RegisterBulkReactionProcesses` and `G4DNAScavengerMaterial` stay as they are, following UHDR `ChemPureWaterBuilder::WaterScavengerReaction`. A Chemistry still defines each bulk reaction once.
- [ ] PureWater and BoscoloChem call `SetReactionType(1)` on their partially diffusion-controlled tracked-pair reactions. Each choice is checked against `G4EmDNAChemistry_option3.cc` (Type II/IV) and UHDR `ChemOxygenWaterBuilder.cc`, with a one-line comment per group citing the source. Pairs with no counterpart stay type 0, with a comment.
- [ ] The catalog headers' "Hard-coded SBS assumption" notes are rewritten to the new rule.
- [ ] Run with `/chem/env/scavenger O2 21 %` (10 keV × 2, end time 1 ms, Serial) passes all of these:
  - exit 0;
  - `O_2^-1` > 0 in `Species.Txt` at 5 ns (particle stage);
  - the Debug log shows the mesoscopic stage firing at least one O2 bulk reaction or reporting bulk O2 consumption;
  - no bulk-partner entry (`H3Op(B)`, `OHm(B)`, bulk O2) in `Reactions.Txt`;
  - no species line duplicated within one time bin of `Species.Txt`.
- [ ] Run without a scavenger: same checks, and `H3O^1` stays buffered at 5 ns, with no monotonic decay to 0.
- [ ] All unit tests pass in `build-ninja/`.

**Files to Touch:**
- `src/chemistry/DnaChemistryList.cc`, `header/chemistry/DnaChemistryList.hh`
- `src/chemistry/catalog/PureWaterReactions.cc`, `header/chemistry/catalog/PureWaterReactions.hh`
- `src/chemistry/catalog/BoscoloChemReactions.cc`, `header/chemistry/catalog/BoscoloChemReactions.hh`

**Verification Step:**

Run (PowerShell, MSVC env):
```bash
cmake --build build --target sim; cmake --build build-ninja; ctest --test-dir build-ninja --output-on-failure
cd build; ./sim 03-o2.in --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/03-o2 > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/03-o2.log 2>&1
./sim 03-water.in --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/03-water > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/03-water.log 2>&1
grep -c "(B)" ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/03-o2/Reactions.Txt
```

Expected:
Both runs exit 0, and all tests pass. `O_2^-1` > 0 at 5 ns in the O2 run. The `(B)` count is `0`, and no reaction label names a bulk partner (check labels against `ReactionsMetadata.csv`). No duplicated species line appears within a time bin.

**Notes:**

`Species.Txt` prints display names (`O_2^-1`, `H3O^1`), not user IDs (`O2m`, `H3Op`). Check display names, never `grep O2m`. A reaction-table entry against a bulk species never pairs in IRT_syn, because bulk species have no tracks. `G4DNAGillespieDirectMethod::FindScavenging` reads the partner count from `G4DNAScavengerMaterial`. `G4DNAMolecularReactionData::SetReactionType(1)` recomputes the reaction radii (`G4DNAMolecularReactionData.cc` ~l.275).
