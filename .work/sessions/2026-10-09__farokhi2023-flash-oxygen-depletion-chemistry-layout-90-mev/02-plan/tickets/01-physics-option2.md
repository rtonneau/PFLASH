# Ticket 01: physics-option2

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] `PhysicsList.cc`'s `fEmDNAPhysicsList` member is constructed with `new G4EmDNAPhysics_option2(/*verbose*/ 0)` instead of `new G4EmDNAPhysics(/*verbose*/ 0)`.
- [ ] The previous `new G4EmDNAPhysics(/*verbose*/ 0)` construction remains in the file as a commented-out line immediately next to the new one, with a one-line comment noting it's the pre-2026-10 default.
- [ ] `#include "G4EmDNAPhysics_option2.hh"` is added; the existing `#include "G4EmDNAPhysics.hh"` stays (needed for the commented-out line to remain valid reference, and because `G4EmDNAPhysics_option2.hh` may not transitively include it).
- [ ] The project builds clean in `build/` (RelWithDebInfo) with no new warnings about the physics constructor.

**Files to Touch:**
- `src/physics/PhysicsList.cc`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
```

Expected: build succeeds (exit 0), no new compiler warnings/errors mentioning `PhysicsList.cc` or `G4EmDNAPhysics_option2`.

Then run a smoke test from `build/` (Serial, 10 events, 25 keV e-, per `run.smoke` in `.claude/.claude-project.json`) and confirm the run still completes:

```bash
cd build
echo -e "/gun/particle e-\n/gun/energy 25 keV\n/run/initialize\n/scheduler/endTime 1 ms\n/run/beamOn 10\n/run/dumpDataAndReset" > macro/scratch_option2_smoke.in
./sim.exe scratch_option2_smoke.in --dir ../.scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/ticket01-smoke > ticket01-smoke.log 2>&1
```

Expected: exit 0; log contains `[RunAccumulatorMessenger] dumped and reset` and `The simulation took`; no `EEEE`/`FatalException`; `Species.Txt` in the `--dir` output is non-empty.

**Notes:**

`G4EmDNAPhysics_option2` is a standard Geant4-DNA physics-constructor class (same header-include pattern as the current `G4EmDNAPhysics`); no other file references the physics-constructor type name, so this is a single-file, single-line change. Do not touch `G4EmParameters::Instance()->SetTimeStepModel(...)` or any other line in `PhysicsList.cc`'s constructor — only the `fEmDNAPhysicsList` initializer and its include.
