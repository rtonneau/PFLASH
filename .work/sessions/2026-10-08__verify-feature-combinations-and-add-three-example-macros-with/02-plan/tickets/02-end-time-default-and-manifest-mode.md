# Ticket 02: end-time-default-and-manifest-mode

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.59)
**Effort:** medium

**Acceptance Criteria:**
- [ ] `ActionInitialization::Build()` sets the end time to 1 s with meso on and 1 us with meso off or SBS; `/scheduler/endTime` after `/run/initialize` still overrides it.
- [ ] `Manifest.json` records the time-step model name and whether the mesoscopic stage is enabled (`handOverTime_ns`, `voxelSize_nm`, `mesoPixels`, `mesoTimesPerDecade` are null or absent when it is off).
- [ ] No `SpeciesMeso.Txt/.csv` (nor `SpeciesMesoSpatial.h5`) are written when meso is off.
- [ ] `JsonWriterTest`/`RunAccumulatorTest` still pass; the model key is covered where the tree is built.

**Files to Touch:**
- `src/actions/ActionInitialization.cc`
- `src/scoring/RunManifest.cc`
- `src/scoring/RunAccumulatorMessenger.cc`

**Verification Step:**

Run:
```bash
cd build && ./sim _t.in && grep -n "chemistryModel\|meso" results/*/Manifest.json Manifest.json 2>/dev/null | head; ls | grep -c SpeciesMeso
```

Expected:
Manifest shows the SBS model and meso disabled; the SpeciesMeso count is 0 for the SBS run.

**Notes:**

Use the `_t.in` macro from ticket 1 with a `/run/dumpDataAndReset` added. The manifest tree is built only in `RunManifest.cc`.
