# Ticket 01: spatial-output-switch

**Status:** ✅ Done

## Local Test Result

`cmake --build build-ninja --target MesoSettingsTest && ctest --test-dir build-ninja -R MesoSettingsTest --output-on-failure` (MSVC env): `100% tests passed, 0 tests failed out of 1`.

## Review Notes

Implemented by a haiku subagent. All three criteria hold: `Values::spatialOutput = false`; `/chem/meso/spatialOutput` (G4UIcmdWithABool, PreInit, guidance names SpeciesMesoSpatial.h5 and says it is heavy) with SetNewValue/GetCurrentValue; `TestDefaults()` in MesoSettingsTest. After review: reworded the header comment to say "Spatial snapshots" (the glossary term) instead of "spatial meshes".

## Blockers / Challenges

None

## Commits

- 1a7b32e feat(meso): add /chem/meso/spatialOutput switch (ticket 01)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
