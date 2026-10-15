# Ticket 01: sbs-mode-and-meso-switch

**Model:** opus
**Model (Jev):** opus-5.5 (confidence 0.61)
**Effort:** high

**Acceptance Criteria:**
- [ ] `/process/chem/TimeStepModel SBS` registers `G4DNAMolecularStepByStepModel`; `IRT_syn` still works and stays the default; `IRT` and unknown values stay fatal.
- [ ] `/chem/meso/enable <bool>` (PreInit, default true) writes `MesoSettings::Current()`; `/chem/meso/enable true` given explicitly with SBS is a fatal `G4Exception`; the default (true) with SBS silently means meso off.
- [ ] With meso off, `TimeStepAction` never calls `CompartmentBased()` and the particle-based stage runs to the end time.
- [ ] Anything specific to IRT_syn in `StackingAction`, `ReactionCounter` and `TimeStepAction` works under SBS (scavenger bulk reactions too).
- [ ] `MesoSettingsTest` covers the new flag; all ctest tests pass.

**Files to Touch:**
- `src/chemistry/DnaChemistryList.cc`
- `src/physics/PhysicsList.cc`
- `src/chemistry/MesoMessenger.cc`
- `src/chemistry/MesoSettings.cc`
- `header/chemistry/MesoSettings.hh`
- `src/chemistry/TimeStepAction.cc`
- `test/MesoSettingsTest.cc`

**Verification Step:**

Run:
```bash
cd build-ninja && ctest --output-on-failure && cd ../build && printf '/process/chem/TimeStepModel SBS\n/chem/meso/enable false\n/run/initialize\n/scheduler/endTime 10 ns\n/gun/particle e-\n/gun/energy 10 keV\n/run/beamOn 2\n' > macro/_t.in && ./sim _t.in
```

Expected:
ctest all passed; the sim run completes with `time-step model = SBS` logged and no exception.

**Notes:**

Old SBS code: `git show b26a262^:src/chemistry/DnaChemistryList.cc`. Grill decision: meso=true with SBS is fatal only when it was set explicitly; keep the default simple (track "explicitly set" in MesoSettings or compare against the default in the messenger). Update the code comments that say IRT_syn is the only model. Build per `.claude/geant4-instructions.md`.
