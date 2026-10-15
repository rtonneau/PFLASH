# Ticket 01: spatial-output-switch

**Model:** haiku

**Acceptance Criteria:**
- [ ] `MesoSettings::Values` has `bool spatialOutput = false;`.
- [ ] `/chem/meso/spatialOutput <bool>` (`G4UIcmdWithABool`, PreInit, guidance mentions the HDF5 file `SpeciesMesoSpatial.h5` and that it is heavy) sets it.
- [ ] `MesoSettingsTest` asserts the default is `false`.

**Files to Touch:**
- `header/chemistry/MesoSettings.hh`
- `header/chemistry/MesoMessenger.hh`
- `src/chemistry/MesoMessenger.cc`
- `test/MesoSettingsTest.cc`

**Verification Step:**

Run (MSVC env as in `.claude/geant4-instructions.md`):
```bash
cmake --build build-ninja --target MesoSettingsTest && ctest --test-dir build-ninja -R MesoSettingsTest --output-on-failure
```

Expected:
`100% tests passed, 0 tests failed out of 1`

**Notes:**

Follow the existing `fpPerDecadeCmd` pattern (member pointer, deleted in the destructor). No range check needed for a bool.
