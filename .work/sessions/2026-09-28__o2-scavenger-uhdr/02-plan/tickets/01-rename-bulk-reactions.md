# Ticket 01: rename-bulk-reactions

**Acceptance Criteria:**
- [ ] `ChemistryTypes::BulkReaction`, `BulkReactionEntry`, `BulkReactionList` replace the `AcidBase*` types; `ChemistryRegistry::Chemistry::buildBulkReactions` replaces `buildAcidBase`
- [ ] Builders renamed to `PureWaterReactions::BuildPureWaterBulkReactions` and `BoscoloChemReactions::BuildBoscoloChemBulkReactions`; `DnaChemistryList::RegisterBulkReactionProcesses`; `ReactionTableDump::WriteBulkReactions`; dump section header `# Bulk reactions (acid-base buffer + scavengers)`
- [ ] `grep -rn "AcidBase\|acid-base list" header src test` returns nothing
- [ ] All unit tests pass in `build-ninja/`
- [ ] Sorted reaction dump differs from the pre-edit baseline only by the section-header line; `EnergyDeposit.Txt` and `PhysicsInteractions.csv` identical

**Files to Touch:**
- `header/chemistry/ChemistryTypes.hh`
- `header/chemistry/ChemistryRegistry.hh`
- `src/chemistry/ChemistryRegistry.cc`
- `src/chemistry/BuiltInChemistries.cc`
- `header/chemistry/catalog/PureWaterReactions.hh`
- `src/chemistry/catalog/PureWaterReactions.cc`
- `header/chemistry/catalog/BoscoloChemReactions.hh`
- `src/chemistry/catalog/BoscoloChemReactions.cc`
- `header/chemistry/DnaChemistryList.hh`
- `src/chemistry/DnaChemistryList.cc`
- `header/chemistry/ReactionTableDump.hh`
- `src/chemistry/ReactionTableDump.cc`
- `test/ChemistryRegistryTest.cc`
- `docs/adr/0004-scavenger-reactions-per-chemistry.md`
- `docs/adr/0001-baseline-acid-base-buffer.md`
- `CONTEXT.md`

**Verification Step:**

Run:
```bash
# inside the vcvars64 wrapper
cmake --build build --config RelWithDebInfo --target sim
cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure
cd build && ./sim scav_smoke.in --dir smoke/t01 > smoke/t01.log 2>&1
diff <(sort smoke/t01/reactions_dump.txt) smoke/baseline.sorted
cmp smoke/t01/EnergyDeposit.Txt smoke/baseline/EnergyDeposit.Txt && cmp smoke/t01/PhysicsInteractions.csv smoke/baseline/PhysicsInteractions.csv
```

Expected:
All tests Passed. The diff shows only the dump section-header line. Both `cmp` commands are silent.

**Notes:**

Step 1 comes before any edit: build `sim`, write `build/macro/scav_smoke.in` (see plan file "Shared procedures") with an empty scavenger line, run it with `--dir smoke/baseline`, and save `sort smoke/baseline/reactions_dump.txt > smoke/baseline.sorted`. Tickets 03 and 04 reuse this baseline. Update `ChemistryRegistryTest.cc` first (`DummyBulkReactions`) so it fails to compile, then rename the rest. Rates and entries in both builders stay byte-identical. Reword doc comments from "acid-base list" to "bulk reactions (acid-base buffer ...)"; "acid-base buffer" as the name of the chemistry itself may stay. The new type definitions, with comments, are in plan Task 1 Step 2. Stage explicit paths only.
