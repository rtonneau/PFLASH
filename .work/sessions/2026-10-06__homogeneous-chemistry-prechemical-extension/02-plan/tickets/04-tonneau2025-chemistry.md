# Ticket 04: tonneau2025-chemistry

**Model:** opus
**Effort:** high

**Acceptance Criteria:**
- [ ] `header/chemistry/catalog/Tonneau2025Reactions.hh` and `src/chemistry/catalog/Tonneau2025Reactions.cc` provide `BuildTonneau2025Reactions`, `BuildTonneau2025BulkReactions` and `ConstructTonneau2025Molecules`, built from `Tonneau2025Table`.
- [ ] `ConstructTonneau2025Molecules` creates HO3 with the diffusion coefficient and radius from `docs/literature/ho3-parameters.md` and is safe to call twice.
- [ ] All 73 reactions are present; those with a pair in `PureWater` use the same reaction type, and the others use the type from Geant4 stock tables; the file header lists each reaction's type and its origin.
- [ ] The bulk list holds the acid-base block (R56 to R73, with the water pseudo-first-order reactions) and the O2 reactions R7, R31, R47, in the style of `PureWater`'s bulk list; the header states that the paper's pH drift is not reproduced.
- [ ] Every species used in a type-1 reaction has a vdW radius; a check or test covers this.
- [ ] `Tonneau2025` is registered in `BuiltInChemistries.cc` with all builders; `/chem/list` shows it.
- [ ] A smoke run with `/chem/select Tonneau2025` completes (10 keV e-, `/run/beamOn 2`, `/scheduler/endTime 1 ms` after `/run/initialize`) and its species output contains HO3.

**Files to Touch:**
- `header/chemistry/catalog/Tonneau2025Reactions.hh`
- `src/chemistry/catalog/Tonneau2025Reactions.cc`
- `src/chemistry/BuiltInChemistries.cc`
- `test/Tonneau2025TableTest.cc`

**Verification Step:**

Run:
```bash
cd build && ./sim beam_tonneau2025.in --dir ../.scratch/tests/2026-10-06__homogeneous-chemistry-prechemical-extension/t4 > ../.scratch/tests/2026-10-06__homogeneous-chemistry-prechemical-extension/t4.log 2>&1; grep -c HO3 ../.scratch/tests/2026-10-06__homogeneous-chemistry-prechemical-extension/t4/*Species*
```

Expected:
Exit code 0 with the run's success markers in the log, no `EEEE` lines, and a non-zero HO3 reference in the species files. If the macro does not exist yet, create a scratch macro in `build/macro` rather than waiting for ticket 5.

**Notes:**

Template: `PureWaterReactions.cc` (lambda `add`/`partial`, `Conf` helper, `BulkReactionList` shape) and `BoscoloChemReactions.cc`. Where the paper's reaction involves H2O as a reactant, route it to the bulk list rather than the two-molecule table, as `PureWater` does (see `H2O` entries at `PureWaterReactions.cc:152` and following). A reaction pair already in the table is shared, so check `PureWater`'s `SetReaction` behaviour for duplicates before re-adding a pair within the same Chemistry. Species-name mapping: e.g. `O2-` is `O2m`, `HO2-` is `HO2m`, `O-` is `Om`, `O3-` is `O3m`; O3 and O3m already exist from Geant4 `option1`.
