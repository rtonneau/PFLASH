# Ticket 01: manifest-writer

**Acceptance Criteria:**
- [ ] `header/scoring/ManifestData.hh`, `header/scoring/ManifestWriter.hh` and `src/scoring/ManifestWriter.cc` include only the standard library.
- [ ] `ManifestWriter::Write` emits the key order and formats given in plan Task 1; `totalEvents` and `totalEnergyDeposit_eV` are summed from `runs`.
- [ ] A run without a beam prints `null` for `particle`, `beamEnergy_keV`, `position_um` and `direction`; empty arrays print `[]`; strings are JSON-escaped.
- [ ] `ManifestWriterTest` is registered in `CMakeLists.txt` and passes in `build-ninja/`.
- [ ] `CONTEXT.md` and `docs/adr/0005-manifest-per-dump.md` are committed with this ticket.

**Files to Touch:**
- `header/scoring/ManifestData.hh` (create)
- `header/scoring/ManifestWriter.hh` (create)
- `src/scoring/ManifestWriter.cc` (create)
- `test/ManifestWriterTest.cc` (create)
- `CMakeLists.txt`
- `CONTEXT.md`, `docs/adr/0005-manifest-per-dump.md` (already written, uncommitted)

**Verification Step:**

Run:
```bash
ctest --test-dir build-ninja --output-on-failure -R ManifestWriterTest
```

Expected:
`100% tests passed, 0 tests failed out of 1`

**Notes:**

Follow plan Task 1 (`.claude/plans/2026-09-29-run-manifest-file.md`): write the test first and see it fail to link, then implement. Build from `build-ninja/` as described in `.claude/geant4-instructions.md`. Copy-paste portable like `PhysicsInteractionCounter`: no `globals.hh`, no project headers.
